#include "EditorApplication.h"
#include "Hyperion/Content/ContentPaths.h"
#include "Hyperion/IO/Path.h"

namespace Hyperion
{
std::shared_ptr<FSceneDocumentChange> FEditorPlugin::ChangeDocument(ESceneDocumentAction InAction,
                                                                    const FSceneLifecycleRequest& InRequest)
{
	return RequestSceneChange(InAction, InRequest, false);
}

std::shared_ptr<FSceneDocumentChange> FEditorPlugin::RequestSceneChange(ESceneDocumentAction InAction,
                                                                        const FSceneLifecycleRequest& InRequest,
                                                                        bool bInPrompt)
{
	UpdateDocumentInteraction();
	const auto Current = DescribeSceneDocument(SceneDocument);
	if (InRequest.Document != Current.Document)
	{
		throw FSceneEditError(SceneEditErrors::StaleDocument, "Query the current scene before new/close");
	}
	if (InRequest.Revision != Current.Revision)
	{
		throw FSceneEditError(SceneEditErrors::StaleRevision, "Scene changed before new/close");
	}
	if (Current.bBusy || Current.bSaving || PendingSceneChange || (Current.bLoaded && SceneTarget->IsPreparing()) ||
	    Transition.HasPendingClose())
	{
		throw FSceneEditError(SceneEditErrors::Busy, "Finish active edits, saves and transitions before new/close");
	}
	if (InAction != ESceneDocumentAction::New && InAction != ESceneDocumentAction::Close)
	{
		throw std::invalid_argument("Invalid scene lifecycle action");
	}
	if (InRequest.Action != ESceneDirtyAction::RejectDirty && InRequest.Action != ESceneDirtyAction::Save &&
	    InRequest.Action != ESceneDirtyAction::Discard)
	{
		throw std::invalid_argument("Invalid scene dirty decision");
	}
	if (Current.bDirty && !bInPrompt && InRequest.Action == ESceneDirtyAction::RejectDirty)
	{
		throw FSceneEditError(SceneEditErrors::DirtyDocument, "Save first or explicitly choose save/discard");
	}
	const auto Destination = InRequest.ScenePath.empty() ? CurrentPath : InRequest.ScenePath;
	if (Current.bDirty && InRequest.Action == ESceneDirtyAction::Save &&
	    (Destination.empty() || PathFromUtf8(Destination).extension() != ".hasset"))
	{
		throw std::invalid_argument("Supply a .hasset scenePath to save an untitled scene before new/close");
	}
	Transition.QueueScene(InAction, InRequest, bInPrompt && Current.bDirty);
	PendingSceneChange = std::make_shared<FSceneDocumentChange>();
	const auto Completion = PendingSceneChange;
	SceneChangeDocument.clear();
	if (Current.bDirty && InRequest.Action == ESceneDirtyAction::Save)
	{
		try
		{
			StartSceneChangeSave(Destination);
		}
		catch (...)
		{
			FailSceneChange(std::current_exception());
		}
	}
	return Completion;
}

void FEditorPlugin::RequestSceneCommand(ESceneDocumentAction InAction)
{
	try
	{
		FinishGizmo();
		FinishInspectorEdit();
		CancelReparentGesture();
		CancelPlacement();
		const auto Current = DescribeSceneDocument(SceneDocument);
		RequestSceneChange(InAction, {Current.Document, Current.Revision}, true);
	}
	catch (const std::exception& Failure)
	{
		Error = Failure.what();
	}
}

void FEditorPlugin::StartSceneChangeSave(const std::string& InPath)
{
	Transition.BeginSave(EEditorTransitionTarget::Scene);
	SaveScene(InPath);
	Transition.SaveAdmitted(EEditorTransitionTarget::Scene, true);
}

void FEditorPlugin::SaveBeforeSceneChange()
{
	if (CurrentPath.empty())
	{
		SavePath = std::string(GameContentRoot) + "/Scenes/Untitled.hasset";
		Transition.AwaitSavePath(EEditorTransitionTarget::Scene);
		bSaveDialog = bRequestSaveDialog = true;
		Gui->ClosePopup();
		return;
	}
	try
	{
		StartSceneChangeSave(CurrentPath);
		Gui->ClosePopup();
	}
	catch (...)
	{
		FailSceneChange(std::current_exception());
	}
}

void FEditorPlugin::FailSceneChange(std::exception_ptr InFailure)
{
	if (PendingSceneChange)
	{
		PendingSceneChange->Failure = InFailure;
		PendingSceneChange.reset();
	}
	SceneChangeDocument.clear();
	Transition.FinishSceneChange();
	try
	{
		std::rethrow_exception(InFailure);
	}
	catch (const std::exception& Failure)
	{
		Error = Failure.what();
	}
}

void FEditorPlugin::RetireSceneDocument()
{
	FinishGizmo();
	ResetDocument();
	Selection.Clear();
	InspectorDrafts.Clear();
	PendingInspectorEdit.reset();
	Viewport.ResetNavigation();
	Camera.Reset();
	FrozenCullingView.reset();
	bSelectionInitialized = false;
	ViewportClick.reset();
	Gui->CancelDragDrop();
	SceneDocument.SetPath({});
	OpenPath.clear();
	SavePath.clear();
	Filter.clear();
	OutlinerSelectionFilter.clear();
	CancelPlacement();
	PlacementStatus.clear();
	PlacementModels.clear();
	PlacementPublication.reset();
	PlacementPublicationPreview.reset();
	Scene->Close();
	Viewport.ViewportTarget = {};
	Viewport.ViewportSize = {};
	RenderStats = {};
	HudDiagnostics = {};
	ReadyFrames = 0;
	bReadyLogged = false;
	Error.clear();
}

void FEditorPlugin::ProcessSceneChange()
{
	if (!PendingSceneChange)
	{
		return;
	}
	try
	{
		if (!Transition.HasPendingScene())
		{
			throw FSceneEditError(SceneEditErrors::Conflict, "Scene transition was cancelled or superseded");
		}
		if (const auto Commit = Transition.TakeSceneCommit(DocumentSaveProgress()))
		{
			const auto Current = DescribeSceneDocument(SceneDocument);
			if (Commit->Request.Document != Current.Document || Commit->Request.Revision != Current.Revision)
			{
				throw FSceneEditError(SceneEditErrors::StaleDocument, "The requested scene transition is stale");
			}
			if (Current.bDirty && Commit->Request.Action != ESceneDirtyAction::Discard)
			{
				throw FSceneEditError(SceneEditErrors::DirtyDocument, "Scene changed before new/close; choose again");
			}
			RetireSceneDocument();
			if (Commit->Action == ESceneDocumentAction::New)
			{
				InitializeSceneDocument();
			}
			SceneChangeDocument = SceneDocument.Id();
			Log(ELogLevel::Info, Commit->Action == ESceneDocumentAction::New ? "Editor created an empty scene"
			                                                                 : "Editor closed the scene");
		}
		if (Transition.ScenePhase() == EEditorTransitionPhase::Failed)
		{
			throw FSceneEditError(SceneEditErrors::SaveFailed, "Scene remains dirty after saving; new/close cancelled");
		}
		if (Transition.ScenePhase() != EEditorTransitionPhase::Preparing)
		{
			return;
		}
		if (SceneDocument.Id() != SceneChangeDocument)
		{
			throw FSceneEditError(SceneEditErrors::StaleDocument, "Another action replaced the requested scene");
		}
		const auto Status = DocumentStatus();
		if (!Status.Error.empty())
		{
			throw FSceneEditError(SceneEditErrors::LoadFailed, Status.Error);
		}
		if (Transition.SceneChange().Action == ESceneDocumentAction::Close || Status.Scene.bReady)
		{
			const auto Completion = PendingSceneChange;
			PendingSceneChange.reset();
			SceneChangeDocument.clear();
			Transition.FinishSceneChange();
			UpdateDocumentInteraction();
			Completion->Result = DocumentStatus();
		}
	}
	catch (...)
	{
		FailSceneChange(std::current_exception());
	}
}

void FEditorPlugin::RequestSceneSave()
{
	if (CurrentPath.empty())
	{
		SavePath = std::string(GameContentRoot) + "/Scenes/Untitled.hasset";
		bSaveDialog = bRequestSaveDialog = true;
	}
	else
	{
		SaveScene(CurrentPath);
	}
}
} // namespace Hyperion
