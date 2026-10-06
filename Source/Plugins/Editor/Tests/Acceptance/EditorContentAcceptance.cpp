#include "EditorAcceptanceHarness.h"
#include "Hyperion/Core/Core.h"
#include <algorithm>
#include <tuple>

namespace Hyperion
{
namespace
{
void CheckContent(bool bInValue, const char* InMessage)
{
	if (!bInValue)
	{
		throw std::runtime_error(InMessage);
	}
}
} // namespace

void FEditorAcceptanceHarness::ExerciseContentInput(std::vector<FInputEvent>& InEvents)
{
	if (!Editor.Scene->GetStatus().Error.empty())
	{
		throw std::runtime_error(Editor.Scene->GetStatus().Error);
	}
	if (Scenario.Content.Progress.IsAny(
	        {EContentState::OpenSaveAsFileMenu, EContentState::OpenSaveAs, EContentState::FocusSaveAsPath,
	         EContentState::SelectSaveAsPathText, EContentState::TypeSaveAsPath, EContentState::ConfirmSaveAs,
	         EContentState::AwaitSaveAs, EContentState::VerifySaveAsCopy, EContentState::RestoreOriginalScene}))
	{
		ExerciseContentSaveAs(InEvents);
		return;
	}
	switch (Scenario.Content.Progress.GetState())
	{
		case EContentState::SelectRootA:
			Editor.QueueContentRoot(Editor.Options.ExerciseContent / "A");
			Scenario.Content.Progress.TransitionTo(EContentState::VerifyRootAndOpenTexture);
			break;
		case EContentState::VerifyRootAndOpenTexture:
			CheckContent(Editor.CurrentPath.empty() && !Editor.Selection && Editor.History.empty(),
			             "Root switch retained old document state");
			Editor.RequestOpenAsset("/Game/Texture.hasset");
			Scenario.Content.Progress.TransitionTo(EContentState::AwaitTextureAndOpenScene);
			break;
		case EContentState::AwaitTextureAndOpenScene:
			if (!Editor.PendingAssetOpen)
			{
				CheckContent(Editor.AssetWorkspace->HasActive(), "Texture asset editor did not open");
				Editor.Gui->ClosePopups();
				Editor.bAssetMessage = Editor.bRequestAssetMessage = false;
				Editor.RequestOpenAsset("/Game/Scene.hasset");
				Scenario.Content.Progress.TransitionTo(EContentState::AwaitRootAScene);
			}
			break;
		case EContentState::AwaitRootAScene:
			if (Editor.Scene->GetStatus().bReady && Editor.ReadyFrames > 8 && !Editor.Browser->IsScanning())
			{
				CheckContent(Editor.Scene->FindNode(Editor.Scene->FindHandle("model"))->Name == "A",
				             "Wrong initial root scene");
				CheckContent(std::find(Editor.ScenePaths.begin(), Editor.ScenePaths.end(),
				                       "/Game/Other/Scene.hasset") != Editor.ScenePaths.end(),
				             "Uncataloged scene was not discovered");
				CheckContent(!Editor.IsDirty(), "Save As regression requires an unmodified loaded scene");
				Scenario.ContentSaveOriginal = Editor.IO.FileSystem()->Read("/Game/Scene.hasset", 1024 * 1024);
				Scenario.Content.Progress.TransitionTo(EContentState::OpenSaveAsFileMenu);
			}
			break;
		case EContentState::EditSceneAndSelectRootB:
		{
			CheckContent(Editor.CurrentPath == "/Game/Scene.hasset", "Same-root selection closed the scene");
			const auto Handle = Editor.Scene->FindHandle("model");
			auto Node = *Editor.Scene->FindNode(Handle);
			Node.Name = "A saved";
			Editor.CommitEdit(Handle, std::move(Node), Editor.Scene->GetRevision());
			Editor.QueueContentRoot(Editor.Options.ExerciseContent / "B");
			Scenario.Content.Progress.TransitionTo(EContentState::CancelRootSwitch);
			break;
		}
		case EContentState::CancelRootSwitch:
			if (ExerciseClick(InEvents, Scenario.CancelChangesBounds, Scenario.Content.Click))
			{
				Scenario.Content.Progress.TransitionTo(EContentState::VerifyCancellationAndRetry);
			}
			break;
		case EContentState::VerifyCancellationAndRetry:
			CheckContent(!Editor.Transition.HasPendingRoot() && Editor.IsDirty() &&
			                 Editor.CurrentPath == "/Game/Scene.hasset",
			             "Cancel lost document changes");
			Editor.QueueContentRoot(Editor.Options.ExerciseContent / "B");
			Scenario.Content.Progress.TransitionTo(EContentState::SaveAndSwitchRoot);
			break;
		case EContentState::SaveAndSwitchRoot:
			if (ExerciseClick(InEvents, Scenario.SaveSwitchBounds, Scenario.Content.Click))
			{
				Scenario.Content.Progress.TransitionTo(EContentState::AwaitSavedRootSwitch);
			}
			break;
		default:
			ExerciseContentSwitch(InEvents);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseContentSaveAs(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.Content.Progress.GetState())
	{
		case EContentState::OpenSaveAsFileMenu:
			if (ExerciseClick(InEvents, Scenario.FileMenuBounds, Scenario.Content.Click))
			{
				Scenario.Content.Progress.TransitionTo(EContentState::OpenSaveAs);
			}
			break;
		case EContentState::OpenSaveAs:
			if (ExerciseClick(InEvents, Scenario.InspectionBounds.at("document/save-as"), Scenario.Content.Click))
			{
				Scenario.Content.Progress.TransitionTo(EContentState::FocusSaveAsPath);
			}
			break;
		case EContentState::FocusSaveAsPath:
			if (ExerciseClick(InEvents, Scenario.InspectionBounds.at("document/save-path"), Scenario.Content.Click))
			{
				Scenario.Content.Progress.TransitionTo(EContentState::SelectSaveAsPathText);
			}
			break;
		case EContentState::SelectSaveAsPathText:
		case EContentState::TypeSaveAsPath:
		{
			FInputEvent Key;
			Key.Type = EEventType::Key;
			Key.Key = EKey::A;
			Key.bDown = Scenario.Content.Progress.Is(EContentState::SelectSaveAsPathText);
			Key.Modifiers = Key.bDown ? 1 : 0;
			InEvents.push_back(Key);
			if (!Key.bDown)
			{
				FInputEvent Text;
				Text.Type = EEventType::Text;
				Text.Text = "Other/SceneCopy.hasset";
				InEvents.push_back(std::move(Text));
			}
			Scenario.Content.Progress.TransitionTo(Scenario.Content.Progress.Is(EContentState::SelectSaveAsPathText)
			                                           ? EContentState::TypeSaveAsPath
			                                           : EContentState::ConfirmSaveAs);
			break;
		}
		case EContentState::ConfirmSaveAs:
			// Click Save directly: do not press Enter to commit the path first.
			if (ExerciseClick(InEvents, Scenario.InspectionBounds.at("document/save-confirm"), Scenario.Content.Click))
			{
				Scenario.Content.Progress.TransitionTo(EContentState::AwaitSaveAs);
			}
			break;
		case EContentState::AwaitSaveAs:
			if (!Editor.PendingSave && !Editor.bSaveDialog)
			{
				const bool bOriginalUnchanged =
				    Editor.IO.FileSystem()->Read("/Game/Scene.hasset", 1024 * 1024) == Scenario.ContentSaveOriginal;
				const bool bCopyExists = Editor.IO.FileSystem()->Exists("/Game/Other/SceneCopy.hasset");
				CheckContent(Editor.CurrentPath == "/Game/Other/SceneCopy.hasset" && bOriginalUnchanged && bCopyExists,
				             ("Save As used the wrong destination: current=" + Editor.CurrentPath +
				              ", original unchanged=" + std::to_string(bOriginalUnchanged) +
				              ", copy exists=" + std::to_string(bCopyExists))
				                 .c_str());
				CheckContent(!Editor.IsDirty(), "Save As left an unmodified scene dirty");
				Editor.OpenScene("/Game/Other/SceneCopy.hasset");
				Scenario.Content.Progress.TransitionTo(EContentState::VerifySaveAsCopy);
			}
			break;
		case EContentState::VerifySaveAsCopy:
			if (Editor.Scene->GetStatus().bReady && Editor.ReadyFrames > 8)
			{
				CheckContent(Editor.Scene->FindNode(Editor.Scene->FindHandle("model"))->Name == "A" &&
				                 !Editor.IsDirty(),
				             "Save As copy did not reopen with the original scene contents");
				Editor.OpenScene("/Game/Scene.hasset");
				Scenario.Content.Progress.TransitionTo(EContentState::RestoreOriginalScene);
			}
			break;
		case EContentState::RestoreOriginalScene:
			if (Editor.Scene->GetStatus().bReady && Editor.ReadyFrames > 8)
			{
				Editor.QueueContentRoot(Editor.Options.ExerciseContent / "A");
				Scenario.Content.Progress.TransitionTo(EContentState::EditSceneAndSelectRootB);
			}
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseContentSwitch(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.Content.Progress.GetState())
	{
		case EContentState::AwaitSavedRootSwitch:
			if (!Editor.Transition.HasPendingRoot() && !Editor.PendingSave)
			{
				CheckContent(Editor.CurrentPath.empty() && !Editor.IsDirty() && Editor.History.empty(),
				             "Saved root transition retained document");
				Editor.RequestOpenAsset("/Game/Scene.hasset");
				Scenario.Content.Progress.TransitionTo(EContentState::EditRootBScene);
			}
			break;
		case EContentState::EditRootBScene:
			if (Editor.Scene->GetStatus().bReady && Editor.ReadyFrames > 8)
			{
				CheckContent(Editor.Scene->FindNode(Editor.Scene->FindHandle("model"))->Name == "B",
				             "Root B reused cached scene A");
				const auto Handle = Editor.Scene->FindHandle("model");
				auto Node = *Editor.Scene->FindNode(Handle);
				Node.Name = "B discarded";
				Editor.CommitEdit(Handle, std::move(Node), Editor.Scene->GetRevision());
				Editor.QueueContentRoot(Editor.Options.ExerciseContent / "A");
				Scenario.Content.Progress.TransitionTo(EContentState::DiscardAndSwitchRoot);
			}
			break;
		case EContentState::DiscardAndSwitchRoot:
			if (ExerciseClick(InEvents, Scenario.DiscardChangesBounds, Scenario.Content.Click))
			{
				Scenario.Content.Progress.TransitionTo(EContentState::AwaitDiscardedRootSwitch);
			}
			break;
		case EContentState::AwaitDiscardedRootSwitch:
			if (!Editor.Transition.HasPendingRoot())
			{
				Editor.RequestOpenAsset("/Game/Scene.hasset");
				Scenario.Content.Progress.TransitionTo(EContentState::VerifyRootASave);
			}
			break;
		case EContentState::VerifyRootASave:
			if (Editor.Scene->GetStatus().bReady && Editor.ReadyFrames > 8)
			{
				CheckContent(Editor.Scene->FindNode(Editor.Scene->FindHandle("model"))->Name == "A saved",
				             "Save did not target old root A");
				const auto B = DecodeAsset(
				    Editor.IO.FileSystem()->Read(Editor.Options.ExerciseContent / "B/Scene.hasset", 1024 * 1024));
				const auto Manifest = ReadRecord(RecordType<FSceneManifest>(), B.Object);
				CheckContent(std::static_pointer_cast<FSceneManifest>(Manifest)->Nodes.front().Name == "B",
				             "Discard unexpectedly saved B");
				CheckContent(Editor.Options.Preferences.RecentRoots.size() == 2, "Recent roots did not deduplicate");
				Scenario.Content.Progress.TransitionTo(EContentState::SelectFolder);
			}
			break;
		default:
			ExerciseContentBrowser(InEvents);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseContentBrowser(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.Content.Progress.GetState())
	{
		case EContentState::SelectFolder:
			if (Scenario.ContentTileBounds.contains("/Game/Other"))
			{
				Scenario.ContentClickBounds = Scenario.ContentTileBounds.at("/Game/Other");
				if (ExerciseClick(InEvents, Scenario.ContentClickBounds, Scenario.Content.Click))
				{
					Scenario.Content.Progress.TransitionTo(EContentState::DoubleClickFolder);
				}
			}
			break;
		case EContentState::DoubleClickFolder:
			if (ExerciseClick(InEvents, Scenario.ContentClickBounds, Scenario.Content.Click))
			{
				Scenario.Content.Progress.TransitionTo(EContentState::SelectNestedScene);
			}
			break;
		case EContentState::SelectNestedScene:
		case EContentState::DoubleClickNestedScene:
			CheckContent(Editor.Browser->SelectedDirectory == "/Game/Other", "Folder double-click did not navigate");
			if (Scenario.ContentTileBounds.contains("/Game/Other/Scene.hasset"))
			{
				if (ExerciseClick(InEvents, Scenario.ContentTileBounds.at("/Game/Other/Scene.hasset"),
				                  Scenario.Content.Click))
				{
					Scenario.Content.Progress.TransitionTo(
					    Scenario.Content.Progress.Is(EContentState::SelectNestedScene)
					        ? EContentState::DoubleClickNestedScene
					        : EContentState::AwaitNestedScene);
				}
			}
			break;
		case EContentState::AwaitNestedScene:
			if (Editor.Scene->GetStatus().bReady && Editor.ReadyFrames > 8 &&
			    Editor.CurrentPath == "/Game/Other/Scene.hasset")
			{
				Editor.Browser->Navigate("/Game");
				Scenario.Content.Progress.TransitionTo(EContentState::VerifyInternalDirectoryFilter);
			}
			break;
		case EContentState::VerifyInternalDirectoryFilter:
			if (!Editor.Browser->IsScanning() && Scenario.ContentTileBounds.contains("/Game/.assets"))
			{
				CheckContent(!Scenario.ContentTileBounds.contains("/Game/.cache"), "Cache directory remained visible");
				Scenario.Content.Progress.TransitionTo(EContentState::PrepareRootSaveFailure);
			}
			break;
		default:
			ExerciseContentFailures(InEvents);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseContentFailures(std::vector<FInputEvent>& InEvents)
{
	const auto OldScene = Editor.Options.ExerciseContent / "A/Other/Scene.hasset";
	switch (Scenario.Content.Progress.GetState())
	{
		case EContentState::PrepareRootSaveFailure:
		{
			const auto Handle = Editor.Scene->FindHandle("model");
			auto Node = *Editor.Scene->FindNode(Handle);
			Node.Name = "Unsaved after write failure";
			Editor.CommitEdit(Handle, std::move(Node), Editor.Scene->GetRevision());
			std::filesystem::permissions(OldScene, std::filesystem::perms::owner_read);
			Editor.QueueContentRoot(Editor.Options.ExerciseContent / "B");
			Scenario.Content.Progress.TransitionTo(EContentState::AttemptRootSave);
			break;
		}
		case EContentState::AttemptRootSave:
			if (ExerciseClick(InEvents, Scenario.SaveSwitchBounds, Scenario.Content.Click))
			{
				Scenario.Content.Progress.TransitionTo(EContentState::AwaitRootSaveFailure);
			}
			break;
		case EContentState::AwaitRootSaveFailure:
			if (!Editor.PendingSave && !Editor.Transition.HasPendingRoot())
			{
				std::filesystem::permissions(OldScene, std::filesystem::perms::owner_all);
				CheckContent(Editor.IsDirty() && Editor.bAssetMessage &&
				                 Editor.AssetMessage.find("Save failed") != std::string::npos,
				             "Save failure did not preserve old document");
				Editor.Gui->ClosePopups();
				Editor.bAssetMessage = Editor.bRequestAssetMessage = false;
				Editor.Options.Preferences.RecentRoots.push_back(Editor.Options.ExerciseContent / "Missing");
				Editor.SavePreferences();
				Editor.QueueContentRoot(Editor.Options.ExerciseContent / "Missing");
				Scenario.Content.Progress.TransitionTo(EContentState::VerifyMissingRootFailure);
			}
			break;
		case EContentState::VerifyMissingRootFailure:
			CheckContent(Editor.IsDirty() && Editor.bAssetMessage && Editor.CurrentPath == "/Game/Other/Scene.hasset",
			             "Invalid root destroyed old document");
			CheckContent(Editor.Options.Preferences.RecentRoots.size() == 2 &&
			                 std::ranges::none_of(Editor.Options.Preferences.RecentRoots,
			                                      [&](const auto& InRoot)
			                                      {
				                                      return SameAssetRoot(InRoot,
				                                                           Editor.Options.ExerciseContent / "Missing");
			                                      }),
			             "Missing root remained in Recent");
			CheckContent(LoadEditorPreferences(Editor.Options.PreferencesPath).RecentRoots ==
			                     Editor.Options.Preferences.RecentRoots &&
			                 SameAssetRoot(Editor.Options.Preferences.AssetRoot, Editor.Options.ExerciseContent / "A"),
			             "Missing root removal was not persisted or changed the active root");
			Editor.Gui->ClosePopups();
			Editor.bAssetMessage = Editor.bRequestAssetMessage = false;
			Editor.QueueContentRoot(Editor.Options.ExerciseContent / "B");
			Scenario.Content.Progress.TransitionTo(EContentState::DiscardAndSwitchAfterFailure);
			break;
		case EContentState::DiscardAndSwitchAfterFailure:
			if (ExerciseClick(InEvents, Scenario.DiscardChangesBounds, Scenario.Content.Click))
			{
				Scenario.Content.Progress.TransitionTo(EContentState::SwitchRootWhileLoading);
			}
			break;
		case EContentState::SwitchRootWhileLoading:
			if (!Editor.Transition.HasPendingRoot())
			{
				Editor.OpenScene("/Game/Scene.hasset");
				Editor.QueueContentRoot(Editor.Options.ExerciseContent / "A");
				Scenario.Content.Progress.TransitionTo(EContentState::VerifyLoadingRootSwitch);
			}
			break;
		case EContentState::VerifyLoadingRootSwitch:
			if (!Editor.Transition.HasPendingRoot() && !Editor.Browser->IsScanning())
			{
				CheckContent(Editor.CurrentPath.empty() && Editor.History.empty() && !Editor.Selection &&
				                 SameAssetRoot(Editor.Context.Require<FContentRootService>().Directory(),
				                               Editor.Options.ExerciseContent / "A"),
				             "Loading transition retained old content");
				Editor.OpenScene("/Game/Scene.hasset");
				Scenario.Content.Progress.TransitionTo(EContentState::PrepareDismissedSave);
			}
			break;
		default:
			ExerciseContentDismissal(InEvents);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseContentDismissal(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.Content.Progress.GetState())
	{
		case EContentState::PrepareDismissedSave:
			if (Editor.Scene->GetStatus().bReady && Editor.ReadyFrames > 8)
			{
				const auto Handle = Editor.Scene->FindHandle("model");
				auto Node = *Editor.Scene->FindNode(Handle);
				Node.Name = "A saved after dismissal";
				Editor.CommitEdit(Handle, std::move(Node), Editor.Scene->GetRevision());
				Editor.QueueContentRoot(Editor.Options.ExerciseContent / "B");
				Scenario.Content.Progress.TransitionTo(EContentState::HoldSavePending);
			}
			break;
		case EContentState::HoldSavePending:
			CheckContent(Editor.Transition.HasPendingRoot() && Editor.Transition.IsDecisionVisible(),
			             "Root prompt was not prepared");
			// Keep the real save queued until the title-bar click has been processed. The timeout
			// permits orderly shutdown even if an earlier assertion interrupts the exercise.
			Scenario.ContentSaveGate = std::make_shared<std::binary_semaphore>(0);
			Editor.Tasks.Dispatch({EDomain::Io},
			                      [Gate = Scenario.ContentSaveGate]
			                      {
				                      std::ignore = Gate->try_acquire_for(std::chrono::seconds(30));
			                      });
			Scenario.Content.Progress.TransitionTo(EContentState::StartHeldSave);
			break;
		case EContentState::StartHeldSave:
			if (ExerciseClick(InEvents, Scenario.SaveSwitchBounds, Scenario.Content.Click))
			{
				Scenario.Content.Progress.TransitionTo(EContentState::DismissPendingSave);
			}
			break;
		case EContentState::DismissPendingSave:
		{
			CheckContent(Editor.PendingSave && !Editor.PendingSave->Result.Ready() && Editor.Transition.IsSavingRoot(),
			             "Save was not held pending during dismissal");
			// BeginModal leaves the title bar as the last item; its rightmost square contains X.
			auto CloseBounds = Scenario.DiscardTitleBounds;
			CloseBounds.X = CloseBounds.Z - (CloseBounds.W - CloseBounds.Y);
			if (ExerciseClick(InEvents, CloseBounds, Scenario.Content.Click))
			{
				Scenario.Content.Progress.TransitionTo(EContentState::ReleaseDismissedSave);
			}
			break;
		}
		case EContentState::ReleaseDismissedSave:
			CheckContent(Editor.PendingSave && !Editor.PendingSave->Result.Ready() &&
			                 !Editor.Transition.HasPendingRoot() && !Editor.Transition.IsDecisionVisible() &&
			                 !Editor.Transition.IsSavingRoot() &&
			                 Editor.Transition.RootPhase() != EEditorTransitionPhase::Ready,
			             "Title-bar dismissal retained the root transition");
			Scenario.ContentSaveGate->release();
			Scenario.ContentSaveGate.reset();
			Scenario.Content.Progress.TransitionTo(EContentState::VerifyDismissedSave);
			break;
		case EContentState::VerifyDismissedSave:
			if (!Editor.PendingSave)
			{
				CheckContent(!Editor.IsDirty() && Editor.CurrentPath == "/Game/Scene.hasset" &&
				                 Editor.Scene->FindNode(Editor.Scene->FindHandle("model"))->Name ==
				                     "A saved after dismissal" &&
				                 SameAssetRoot(Editor.Context.Require<FContentRootService>().Directory(),
				                               Editor.Options.ExerciseContent / "A"),
				             "Finishing the dismissed save switched roots or lost the document");
				const auto Saved = DecodeAsset(
				    Editor.IO.FileSystem()->Read(Editor.Options.ExerciseContent / "A/Scene.hasset", 1024 * 1024));
				const auto Manifest = ReadRecord(RecordType<FSceneManifest>(), Saved.Object);
				CheckContent(std::static_pointer_cast<FSceneManifest>(Manifest)->Nodes.front().Name ==
				                 "A saved after dismissal",
				             "Dismissal prevented the admitted save from completing against A");
				const auto Handle = Editor.Scene->FindHandle("model");
				auto Node = *Editor.Scene->FindNode(Handle);
				Node.Name = "Discard on application close";
				Editor.CommitEdit(Handle, std::move(Node), Editor.Scene->GetRevision());
				Editor.QueueContentRoot(Editor.Options.ExerciseContent / "B");
				Scenario.Content.Progress.TransitionTo(EContentState::PrepareFailedClose);
			}
			break;
		default:
			ExerciseContentClose(InEvents);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseContentClose(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.Content.Progress.GetState())
	{
		case EContentState::PrepareFailedClose:
			CheckContent(Editor.Transition.HasPendingRoot() && Editor.Transition.IsDecisionVisible(),
			             "Close overlap lacked a root prompt");
			Editor.CancelDiscardAction();
			Editor.Gui->ClosePopups();
			std::filesystem::permissions(Editor.Options.ExerciseContent / "A/Scene.hasset",
			                             std::filesystem::perms::owner_read);
			Editor.RequestApplicationClose({EApplicationCloseAction::Save, Editor.CurrentPath});
			Scenario.Content.Progress.TransitionTo(EContentState::AwaitFailedClose);
			break;
		case EContentState::AwaitFailedClose:
			if (Editor.Transition.ClosePhase() == EEditorTransitionPhase::Failed && !Editor.PendingSave)
			{
				std::filesystem::permissions(Editor.Options.ExerciseContent / "A/Scene.hasset",
				                             std::filesystem::perms::owner_all);
				CheckContent(Editor.Transition.HasPendingClose() && Editor.Transition.IsDecisionVisible() &&
				                 Editor.IsDirty() && !Editor.Window->ShouldClose(),
				             "Failed API save-close lost GUI exit intent or dirty work");
				Scenario.Content.Progress.TransitionTo(EContentState::CancelFailedClose);
			}
			break;
		case EContentState::CancelFailedClose:
			if (ExerciseClick(InEvents, Scenario.CancelChangesBounds, Scenario.Content.Click))
			{
				Scenario.Content.Progress.TransitionTo(EContentState::VerifyCancelledClose);
			}
			break;
		case EContentState::VerifyCancelledClose:
			CheckContent(Editor.Transition.ClosePhase() == EEditorTransitionPhase::Idle &&
			                 !Editor.Transition.HasPendingClose() && !Editor.Transition.IsDecisionVisible() &&
			                 Editor.IsDirty(),
			             "GUI cancel after API save-close failure lost work or retained exit intent");
			Editor.QueueContentRoot(Editor.Options.ExerciseContent / "B");
			Scenario.Content.Progress.TransitionTo(EContentState::RequestCloseDuringRootSwitch);
			break;
		case EContentState::RequestCloseDuringRootSwitch:
			CheckContent(Editor.Transition.HasPendingRoot() && Editor.Transition.IsDecisionVisible(),
			             "Close overlap lacked a root prompt");
			Editor.Window->RequestClose();
			Scenario.Content.Progress.TransitionTo(EContentState::DiscardDuringRootSwitch);
			break;
		case EContentState::DiscardDuringRootSwitch:
			CheckContent(Editor.Transition.HasPendingClose() && Editor.Transition.HasPendingRoot() && Editor.IsDirty(),
			             "Close overlap was not exercised");
			if (ExerciseClick(InEvents, Scenario.DiscardChangesBounds, Scenario.Content.Click))
			{
				Scenario.Content.Progress.TransitionTo(EContentState::VerifyApplicationClosed);
			}
			if (Scenario.Content.Progress.Is(EContentState::VerifyApplicationClosed))
			{
				Scenario.bContentVerified = true;
				Log(ELogLevel::Info,
				    "Editor content A/B/A, save/cancel/discard, double-click, internal filter, "
				    "save failure, loading transition, title-bar dismissal and API close failure GUI cancel verified");
			}
			break;
		case EContentState::VerifyApplicationClosed:
			throw std::runtime_error("Discard during root prompt failed to close the application");
	}
}
} // namespace Hyperion
