#include "EditorApplication.h"
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
	Browser->SelectedDirectory = "/Game";
	Browser->SelectedFile.clear();
	bOpenDialog = bSaveDialog = Transition.bDiscardDialog = bRequestOpen = bRequestSaveDialog =
	    Transition.bRequestDiscard = false;
	Transition.bSaveThenSwitch = false;
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
	Transition.PendingOpen.clear();
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

void FEditorPlugin::PrepareContentRoot()
{
	auto& Content = Context.Require<FContentRootService>();
	const auto Requested = std::exchange(Transition.RequestedRoot, {});
	std::error_code PathError;
	const auto Canonical = std::filesystem::canonical(Requested, PathError);
	if (PathError)
	{
		if (PathError == std::errc::no_such_file_or_directory || PathError == std::errc::not_a_directory)
		{
			const auto Removed = std::erase_if(Options.Preferences.RecentRoots,
			                                   [&](const auto& InRoot)
			                                   {
				                                   return SameAssetRoot(InRoot, Requested);
			                                   });
			if (Removed > 0)
			{
				SavePreferences();
			}
		}
		throw std::filesystem::filesystem_error("canonical", Requested, PathError);
	}
	if (SameAssetRoot(Canonical, Content.Directory()))
	{
		RememberAssetRoot(Options.Preferences, Canonical);
		SavePreferences();
		return;
	}
	Transition.PendingRoot.emplace(Content.Prepare(Canonical));
	Transition.bDiscardRoot = false;
	FinishGizmo();
	FinishInspectorEdit();
	if (ApplicationCloseState().bDirty || PendingSave || AssetWorkspace->IsSaving())
	{
		Transition.bDiscardDialog = Transition.bRequestDiscard = true;
	}
	else
	{
		Transition.bCommitRoot = true;
	}
}

void FEditorPlugin::CompleteContentRoot()
{
	auto& Content = Context.Require<FContentRootService>();
	auto Prepared = Content.Prepare(Transition.PendingRoot->GetDirectory());
	Transition.PendingRoot.reset();
	Transition.bCommitRoot = false;
	try
	{
		Content.Commit(std::move(Prepared), Transition.bDiscardRoot);
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
		if (!Transition.RequestedRoot.empty())
		{
			PrepareContentRoot();
		}
		if (Transition.bSaveThenSwitch && !bSaveDialog && !PendingSave && !AssetWorkspace->IsSaving())
		{
			Transition.bSaveThenSwitch = false;
			Transition.bCommitRoot = !IsDirty() && !AssetWorkspace->IsDirty();
			Transition.bDiscardDialog = Transition.bRequestDiscard = !Transition.bCommitRoot;
			if (Transition.bCommitRoot)
			{
				Gui->ClosePopups();
			}
		}
		if (!Transition.bCommitRoot || !Transition.PendingRoot || PendingSave || AssetWorkspace->IsSaving() ||
		    AssetWorkspace->HasPendingEdits())
		{
			if (Transition.PendingRoot && !Transition.bDiscardDialog && !bSaveDialog && !Transition.bSaveThenSwitch &&
			    !Transition.bCommitRoot)
			{
				Transition.PendingRoot.reset();
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
		Transition.PendingRoot.reset();
		Transition.bCommitRoot = Transition.bSaveThenSwitch = false;
	}
}
} // namespace Hyperion
