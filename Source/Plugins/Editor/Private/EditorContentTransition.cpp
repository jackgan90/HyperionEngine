#include "EditorApplication.h"
#include "Hyperion/Platform/FileDialog.h"

namespace Hyperion
{
FContentRootParticipantState FEditorPlugin::ContentRootState() const
{
	return {IsDirty() || AssetWorkspace->IsDirty(),
	        PendingSave.has_value() || AssetWorkspace->IsSaving() || AssetWorkspace->HasPendingEdits()};
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
	Browser->SelectedDirectory = "/Game";
	Browser->SelectedFile.clear();
	bOpenDialog = bSaveDialog = bDiscardDialog = bRequestOpen = bRequestSaveDialog = bRequestDiscard = false;
	bSaveThenSwitch = false;
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
	if (!PendingRoot)
	{
		RequestedRoot = InDirectory;
	}
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
	Camera.Reset();
	bCameraDragging = bViewportCameraInitialized = bSelectionInitialized = false;
	ViewportClick.reset();
	SceneDocument.SetPath({});
	OpenPath.clear();
	PendingOpen.clear();
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
	ViewportTarget = {};
	ViewportSize = {};
	GuiRenderer->ReleaseFrame();
	Session->ResetContent();
}

void FEditorPlugin::PrepareContentRoot()
{
	auto& Content = Context.Require<FContentRootService>();
	const auto Requested = std::exchange(RequestedRoot, {});
	const auto Canonical = std::filesystem::canonical(Requested);
	if (SameAssetRoot(Canonical, Content.Directory()))
	{
		RememberAssetRoot(Options.Preferences, Canonical);
		SavePreferences();
		return;
	}
	PendingRoot.emplace(Content.Prepare(Canonical));
	bDiscardRoot = false;
	FinishGizmo();
	FinishInspectorEdit();
	if (IsDirty() || PendingSave || AssetWorkspace->IsDirty() || AssetWorkspace->IsSaving())
	{
		bDiscardDialog = bRequestDiscard = true;
	}
	else
	{
		bCommitRoot = true;
	}
}

void FEditorPlugin::CompleteContentRoot()
{
	auto& Content = Context.Require<FContentRootService>();
	auto Prepared = Content.Prepare(PendingRoot->GetDirectory());
	PendingRoot.reset();
	PendingRoot.emplace(std::move(Prepared));
	bCommitRoot = false;
	try
	{
		Content.Commit(std::move(*PendingRoot), bDiscardRoot);
		PendingRoot.reset();
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
			bCameraDragging = false;
			if (const auto Selected = SelectFolder(Window->Surface(), Content.Directory()))
			{
				QueueContentRoot(*Selected);
			}
		}
		if (!RequestedRoot.empty())
		{
			PrepareContentRoot();
		}
		if (bSaveThenSwitch && !bSaveDialog && !PendingSave && !AssetWorkspace->IsSaving())
		{
			bSaveThenSwitch = false;
			bCommitRoot = !IsDirty() && !AssetWorkspace->IsDirty();
			bDiscardDialog = bRequestDiscard = !bCommitRoot;
			if (bCommitRoot)
			{
				Gui->ClosePopups();
			}
		}
		if (!bCommitRoot || !PendingRoot || PendingSave || AssetWorkspace->IsSaving() ||
		    AssetWorkspace->HasPendingEdits())
		{
			if (PendingRoot && !bDiscardDialog && !bSaveDialog && !bSaveThenSwitch && !bCommitRoot)
			{
				PendingRoot.reset();
			}
			return;
		}
		CompleteContentRoot();
	}
	catch (const std::exception& Failure)
	{
		Error = "Could not change asset root: " + std::string(Failure.what());
		AssetMessage = Error;
		bAssetMessage = bRequestAssetMessage = true;
		PendingRoot.reset();
		bCommitRoot = bSaveThenSwitch = false;
	}
}
} // namespace Hyperion
