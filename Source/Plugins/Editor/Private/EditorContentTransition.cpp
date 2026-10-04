#include "EditorApplication.h"
#include "Hyperion/Content/ContentPaths.h"
#include "Hyperion/Platform/FileDialog.h"

namespace Hyperion
{
FContentRootParticipantState FEditorPlugin::ContentRootState() const
{
	return {IsDirty() || AssetWorkspace->IsDirty(),
	        PendingSave.has_value() || AssetWorkspace->IsSaving() || AssetWorkspace->HasPendingEdits() ||
	            AssetWorkspace->HasActiveInteraction() || IsDocumentInteractionBusy()};
}

void FEditorPlugin::ReleaseContentRoot()
{
	CloseContentDocument();
}

void FEditorPlugin::ContentRootChanged()
{
	InitializeSceneDocument();
	auto Features = Context.Require<FRenderFeatureRegistry>().Create();
	Features.push_back(MakeTransientGeometryFeature());
	Features.push_back(MakeSelectionOutlineFeature(Device->GetCapabilities()));
	Pipeline = std::make_unique<FSceneRenderPipeline>(*Session, Device->GetCapabilities(), FScenePipelineSettings{},
	                                                  std::move(Features));
	InitializePlacement();
	Browser->SelectedDirectory = GameContentRoot;
	Browser->SelectedFile.clear();
	bOpenDialog = bSaveDialog = bRequestOpen = bRequestSaveDialog = false;
	Transition.ContentRootChanged();
	bAssetMessage = bRequestAssetMessage = false;
	Error.clear();
	RefreshContent();
	const auto Root = Context.Require<FContentRootService>().Directory();
	if (Root.empty())
	{
		Options.Preferences.AssetRoot.clear();
	}
	else
	{
		RememberAssetRoot(Options.Preferences, Root);
	}
	SavePreferences();
}

void FEditorPlugin::CancelContentRequests()
{
	if (Browser)
	{
		Browser->Stop();
	}
	AssetOpenCancellation.Cancel();
	if (PendingAssetOpen)
	{
		try
		{
			Tasks.Wait(PendingAssetOpen->Task());
		}
		catch (...)
		{
		}
		PendingAssetOpen.reset();
	}
}

void FEditorPlugin::QueueContentRoot(const std::filesystem::path& InDirectory)
{
	Transition.QueueRoot(InDirectory);
}

void FEditorPlugin::CloseContentDocument()
{
	CancelContentRequests();
	CloseAssetWindow();
	FinishGizmo();
	FinishInspectorEdit();
	CancelPlacement();
	Gui->CancelDragDrop();
	Gui->ClosePopups();
	ResetDocument();
	Selection.Clear();
	InspectorDrafts.Clear();
	PendingInspectorEdit.reset();
	Viewport.ResetNavigation();
	bSelectionInitialized = false;
	ViewportClick.reset();
	SceneDocument.SetPath({});
	OpenPath.clear();
	Transition.ContentDocumentClosed();
	SavePath.clear();
	ScenePaths.clear();
	bReadyLogged = false;
	ReadyFrames = 0;
	AssetOpenPath.clear();
	AssetMessage.clear();
	Scene->Close();
	PlacementModels.clear();
	PlacementIcons.clear();
	PlacementPublication.reset();
	PlacementPublicationPreview.reset();
	PlacementMaterial.reset();
	PlacementLifetime.reset();
	PlacementRegistry = {};
	Pipeline.reset();
	Viewport.ViewportTarget = {};
	Viewport.ViewportSize = {};
	GuiRenderer->ReleaseFrame();
	Session->ResetContent();
}

void FEditorPlugin::PrepareContentRoot(const std::filesystem::path& InRequested)
{
	auto& Content = Context.Require<FContentRootService>();
	std::error_code PathError;
	const auto Canonical = std::filesystem::canonical(InRequested, PathError);
	if (PathError)
	{
		if (PathError == std::errc::no_such_file_or_directory || PathError == std::errc::not_a_directory)
		{
			const auto Removed = std::erase_if(Options.Preferences.RecentRoots,
			                                   [&](const auto& InRoot)
			                                   {
				                                   return SameAssetRoot(InRoot, InRequested);
			                                   });
			if (Removed > 0)
			{
				SavePreferences();
			}
		}
		throw std::filesystem::filesystem_error("canonical", InRequested, PathError);
	}
	if (SameAssetRoot(Canonical, Content.Directory()))
	{
		RememberAssetRoot(Options.Preferences, Canonical);
		SavePreferences();
		Transition.FinishRootRequest();
		return;
	}
	auto Prepared = Content.Prepare(Canonical);
	FinishGizmo();
	FinishInspectorEdit();
	Transition.AcceptRootCandidate(std::move(Prepared), DocumentSaveProgress());
}

void FEditorPlugin::CompleteContentRoot(const FEditorRootCommit& InRequest)
{
	auto& Content = Context.Require<FContentRootService>();
	auto Prepared = Content.Prepare(InRequest.Directory);
	try
	{
		Content.Commit(std::move(Prepared), InRequest.bDiscard);
	}
	catch (const FContentRootError& Failure)
	{
		if (Failure.Code == "content_failed")
		{
			Control.ReportFailure(std::current_exception());
			Control.RequestExit();
			bFinished = true;
		}
		throw;
	}
}

void FEditorPlugin::ProcessContentRoot()
{
	auto& Content = Context.Require<FContentRootService>();
	try
	{
		if (std::exchange(bRequestRootDialog, false))
		{
			Camera.Reset();
			Viewport.bCameraDragging = false;
			if (const auto Selected = SelectFolder(Window->Surface(), Content.Directory()))
			{
				QueueContentRoot(*Selected);
			}
		}
		if (const auto Requested = Transition.TakeRootRequest())
		{
			PrepareContentRoot(*Requested);
		}
		if (!Transition.HasPendingRoot())
		{
			return;
		}
		const auto Progress = DocumentSaveProgress();
		if (Transition.AdvanceRootSave(Progress))
		{
			Gui->ClosePopups();
		}
		if (const auto Commit = Transition.TakeRootCommit(Progress))
		{
			CompleteContentRoot(*Commit);
		}
	}
	catch (const std::exception& Failure)
	{
		Error = "Could not change asset root: " + std::string(Failure.what());
		AssetMessage = Error;
		bAssetMessage = bRequestAssetMessage = true;
		Transition.FinishRootRequest();
	}
}
} // namespace Hyperion
