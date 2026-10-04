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
	if (Scenario.ExerciseStep >= 100)
	{
		ExerciseContentSaveAs(InEvents);
		return;
	}
	switch (Scenario.ExerciseStep)
	{
		case 0:
			Editor.QueueContentRoot(Editor.Options.ExerciseContent / "A");
			++Scenario.ExerciseStep;
			break;
		case 1:
			CheckContent(Editor.CurrentPath.empty() && !Editor.Selection && Editor.History.empty(),
			             "Root switch retained old document state");
			Editor.RequestOpenAsset("/Game/Texture.hasset");
			++Scenario.ExerciseStep;
			break;
		case 2:
			if (!Editor.PendingAssetOpen)
			{
				CheckContent(Editor.AssetWorkspace->HasActive(), "Texture asset editor did not open");
				Editor.Gui->ClosePopups();
				Editor.bAssetMessage = Editor.bRequestAssetMessage = false;
				Editor.RequestOpenAsset("/Game/Scene.hasset");
				++Scenario.ExerciseStep;
			}
			break;
		case 3:
			if (Editor.Scene->GetStatus().bReady && Editor.ReadyFrames > 8 && !Editor.Browser->IsScanning())
			{
				CheckContent(Editor.Scene->FindNode(Editor.Scene->FindHandle("model"))->Name == "A",
				             "Wrong initial root scene");
				CheckContent(std::find(Editor.ScenePaths.begin(), Editor.ScenePaths.end(),
				                       "/Game/Other/Scene.hasset") != Editor.ScenePaths.end(),
				             "Uncataloged scene was not discovered");
				CheckContent(!Editor.IsDirty(), "Save As regression requires an unmodified loaded scene");
				Scenario.ContentSaveOriginal = Editor.IO.FileSystem()->Read("/Game/Scene.hasset", 1024 * 1024);
				Scenario.ExerciseStep = 100;
			}
			break;
		case 4:
		{
			CheckContent(Editor.CurrentPath == "/Game/Scene.hasset", "Same-root selection closed the scene");
			const auto Handle = Editor.Scene->FindHandle("model");
			auto Node = *Editor.Scene->FindNode(Handle);
			Node.Name = "A saved";
			Editor.CommitEdit(Handle, std::move(Node), Editor.Scene->GetRevision());
			Editor.QueueContentRoot(Editor.Options.ExerciseContent / "B");
			++Scenario.ExerciseStep;
			break;
		}
		case 5:
			ExerciseClick(InEvents, Scenario.CancelChangesBounds);
			break;
		case 6:
			CheckContent(!Editor.Transition.HasPendingRoot() && Editor.IsDirty() &&
			                 Editor.CurrentPath == "/Game/Scene.hasset",
			             "Cancel lost document changes");
			Editor.QueueContentRoot(Editor.Options.ExerciseContent / "B");
			++Scenario.ExerciseStep;
			break;
		case 7:
			ExerciseClick(InEvents, Scenario.SaveSwitchBounds);
			break;
		default:
			ExerciseContentSwitch(InEvents);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseContentSaveAs(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.ExerciseStep)
	{
		case 100:
			ExerciseClick(InEvents, Scenario.FileMenuBounds);
			break;
		case 101:
			ExerciseClick(InEvents, Scenario.InspectionBounds.at("document/save-as"));
			break;
		case 102:
			ExerciseClick(InEvents, Scenario.InspectionBounds.at("document/save-path"));
			break;
		case 103:
		case 104:
		{
			FInputEvent Key;
			Key.Type = EEventType::Key;
			Key.Key = EKey::A;
			Key.bDown = Scenario.ExerciseStep == 103;
			Key.Modifiers = Key.bDown ? 1 : 0;
			InEvents.push_back(Key);
			if (!Key.bDown)
			{
				FInputEvent Text;
				Text.Type = EEventType::Text;
				Text.Text = "Other/SceneCopy.hasset";
				InEvents.push_back(std::move(Text));
			}
			++Scenario.ExerciseStep;
			break;
		}
		case 105:
			// Click Save directly: do not press Enter to commit the path first.
			ExerciseClick(InEvents, Scenario.InspectionBounds.at("document/save-confirm"));
			break;
		case 106:
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
				++Scenario.ExerciseStep;
			}
			break;
		case 107:
			if (Editor.Scene->GetStatus().bReady && Editor.ReadyFrames > 8)
			{
				CheckContent(Editor.Scene->FindNode(Editor.Scene->FindHandle("model"))->Name == "A" &&
				                 !Editor.IsDirty(),
				             "Save As copy did not reopen with the original scene contents");
				Editor.OpenScene("/Game/Scene.hasset");
				++Scenario.ExerciseStep;
			}
			break;
		case 108:
			if (Editor.Scene->GetStatus().bReady && Editor.ReadyFrames > 8)
			{
				Editor.QueueContentRoot(Editor.Options.ExerciseContent / "A");
				Scenario.ExerciseStep = 4;
			}
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseContentSwitch(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.ExerciseStep)
	{
		case 8:
			if (!Editor.Transition.HasPendingRoot() && !Editor.PendingSave)
			{
				CheckContent(Editor.CurrentPath.empty() && !Editor.IsDirty() && Editor.History.empty(),
				             "Saved root transition retained document");
				Editor.RequestOpenAsset("/Game/Scene.hasset");
				++Scenario.ExerciseStep;
			}
			break;
		case 9:
			if (Editor.Scene->GetStatus().bReady && Editor.ReadyFrames > 8)
			{
				CheckContent(Editor.Scene->FindNode(Editor.Scene->FindHandle("model"))->Name == "B",
				             "Root B reused cached scene A");
				const auto Handle = Editor.Scene->FindHandle("model");
				auto Node = *Editor.Scene->FindNode(Handle);
				Node.Name = "B discarded";
				Editor.CommitEdit(Handle, std::move(Node), Editor.Scene->GetRevision());
				Editor.QueueContentRoot(Editor.Options.ExerciseContent / "A");
				++Scenario.ExerciseStep;
			}
			break;
		case 10:
			ExerciseClick(InEvents, Scenario.DiscardChangesBounds);
			break;
		case 11:
			if (!Editor.Transition.HasPendingRoot())
			{
				Editor.RequestOpenAsset("/Game/Scene.hasset");
				++Scenario.ExerciseStep;
			}
			break;
		case 12:
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
				++Scenario.ExerciseStep;
			}
			break;
		default:
			ExerciseContentBrowser(InEvents);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseContentBrowser(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.ExerciseStep)
	{
		case 13:
			if (Scenario.ContentTileBounds.contains("/Game/Other"))
			{
				Scenario.ContentClickBounds = Scenario.ContentTileBounds.at("/Game/Other");
				ExerciseClick(InEvents, Scenario.ContentClickBounds);
			}
			break;
		case 14:
			ExerciseClick(InEvents, Scenario.ContentClickBounds);
			break;
		case 15:
		case 16:
			CheckContent(Editor.Browser->SelectedDirectory == "/Game/Other", "Folder double-click did not navigate");
			if (Scenario.ContentTileBounds.contains("/Game/Other/Scene.hasset"))
			{
				ExerciseClick(InEvents, Scenario.ContentTileBounds.at("/Game/Other/Scene.hasset"));
			}
			break;
		case 17:
			if (Editor.Scene->GetStatus().bReady && Editor.ReadyFrames > 8 &&
			    Editor.CurrentPath == "/Game/Other/Scene.hasset")
			{
				Editor.Browser->Navigate("/Game");
				++Scenario.ExerciseStep;
			}
			break;
		case 18:
			if (!Editor.Browser->IsScanning() && Scenario.ContentTileBounds.contains("/Game/.assets"))
			{
				CheckContent(!Scenario.ContentTileBounds.contains("/Game/.cache"), "Cache directory remained visible");
				Scenario.ExerciseStep = 20;
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
	switch (Scenario.ExerciseStep)
	{
		case 20:
		{
			const auto Handle = Editor.Scene->FindHandle("model");
			auto Node = *Editor.Scene->FindNode(Handle);
			Node.Name = "Unsaved after write failure";
			Editor.CommitEdit(Handle, std::move(Node), Editor.Scene->GetRevision());
			std::filesystem::permissions(OldScene, std::filesystem::perms::owner_read);
			Editor.QueueContentRoot(Editor.Options.ExerciseContent / "B");
			++Scenario.ExerciseStep;
			break;
		}
		case 21:
			ExerciseClick(InEvents, Scenario.SaveSwitchBounds);
			break;
		case 22:
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
				++Scenario.ExerciseStep;
			}
			break;
		case 23:
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
			++Scenario.ExerciseStep;
			break;
		case 24:
			ExerciseClick(InEvents, Scenario.DiscardChangesBounds);
			break;
		case 25:
			if (!Editor.Transition.HasPendingRoot())
			{
				Editor.OpenScene("/Game/Scene.hasset");
				Editor.QueueContentRoot(Editor.Options.ExerciseContent / "A");
				++Scenario.ExerciseStep;
			}
			break;
		case 26:
			if (!Editor.Transition.HasPendingRoot() && !Editor.Browser->IsScanning())
			{
				CheckContent(Editor.CurrentPath.empty() && Editor.History.empty() && !Editor.Selection &&
				                 SameAssetRoot(Editor.Context.Require<FContentRootService>().Directory(),
				                               Editor.Options.ExerciseContent / "A"),
				             "Loading transition retained old content");
				Editor.OpenScene("/Game/Scene.hasset");
				++Scenario.ExerciseStep;
			}
			break;
		default:
			ExerciseContentDismissal(InEvents);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseContentDismissal(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.ExerciseStep)
	{
		case 27:
			if (Editor.Scene->GetStatus().bReady && Editor.ReadyFrames > 8)
			{
				const auto Handle = Editor.Scene->FindHandle("model");
				auto Node = *Editor.Scene->FindNode(Handle);
				Node.Name = "A saved after dismissal";
				Editor.CommitEdit(Handle, std::move(Node), Editor.Scene->GetRevision());
				Editor.QueueContentRoot(Editor.Options.ExerciseContent / "B");
				++Scenario.ExerciseStep;
			}
			break;
		case 28:
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
			++Scenario.ExerciseStep;
			break;
		case 29:
			ExerciseClick(InEvents, Scenario.SaveSwitchBounds);
			break;
		case 30:
		{
			CheckContent(Editor.PendingSave && !Editor.PendingSave->Result.Ready() && Editor.Transition.IsSavingRoot(),
			             "Save was not held pending during dismissal");
			// BeginModal leaves the title bar as the last item; its rightmost square contains X.
			auto CloseBounds = Scenario.DiscardTitleBounds;
			CloseBounds.X = CloseBounds.Z - (CloseBounds.W - CloseBounds.Y);
			ExerciseClick(InEvents, CloseBounds);
			break;
		}
		case 31:
			CheckContent(Editor.PendingSave && !Editor.PendingSave->Result.Ready() &&
			                 !Editor.Transition.HasPendingRoot() && !Editor.Transition.IsDecisionVisible() &&
			                 !Editor.Transition.IsSavingRoot() &&
			                 Editor.Transition.RootPhase() != EEditorTransitionPhase::Ready,
			             "Title-bar dismissal retained the root transition");
			Scenario.ContentSaveGate->release();
			Scenario.ContentSaveGate.reset();
			++Scenario.ExerciseStep;
			break;
		case 32:
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
				++Scenario.ExerciseStep;
			}
			break;
		default:
			ExerciseContentClose(InEvents);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseContentClose(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.ExerciseStep)
	{
		case 33:
			CheckContent(Editor.Transition.HasPendingRoot() && Editor.Transition.IsDecisionVisible(),
			             "Close overlap lacked a root prompt");
			Editor.CancelDiscardAction();
			Editor.Gui->ClosePopups();
			std::filesystem::permissions(Editor.Options.ExerciseContent / "A/Scene.hasset",
			                             std::filesystem::perms::owner_read);
			Editor.RequestApplicationClose({EApplicationCloseAction::Save, Editor.CurrentPath});
			++Scenario.ExerciseStep;
			break;
		case 34:
			if (Editor.Transition.ClosePhase() == EEditorTransitionPhase::Failed && !Editor.PendingSave)
			{
				std::filesystem::permissions(Editor.Options.ExerciseContent / "A/Scene.hasset",
				                             std::filesystem::perms::owner_all);
				CheckContent(Editor.Transition.HasPendingClose() && Editor.Transition.IsDecisionVisible() &&
				                 Editor.IsDirty() && !Editor.Window->ShouldClose(),
				             "Failed API save-close lost GUI exit intent or dirty work");
				++Scenario.ExerciseStep;
			}
			break;
		case 35:
			ExerciseClick(InEvents, Scenario.CancelChangesBounds);
			break;
		case 36:
			CheckContent(Editor.Transition.ClosePhase() == EEditorTransitionPhase::Idle &&
			                 !Editor.Transition.HasPendingClose() && !Editor.Transition.IsDecisionVisible() &&
			                 Editor.IsDirty(),
			             "GUI cancel after API save-close failure lost work or retained exit intent");
			Editor.QueueContentRoot(Editor.Options.ExerciseContent / "B");
			++Scenario.ExerciseStep;
			break;
		case 37:
			CheckContent(Editor.Transition.HasPendingRoot() && Editor.Transition.IsDecisionVisible(),
			             "Close overlap lacked a root prompt");
			Editor.Window->RequestClose();
			++Scenario.ExerciseStep;
			break;
		case 38:
			CheckContent(Editor.Transition.HasPendingClose() && Editor.Transition.HasPendingRoot() && Editor.IsDirty(),
			             "Close overlap was not exercised");
			ExerciseClick(InEvents, Scenario.DiscardChangesBounds);
			if (Scenario.ExerciseStep == 39)
			{
				Scenario.bContentVerified = true;
				Log(ELogLevel::Info,
				    "Editor content A/B/A, save/cancel/discard, double-click, internal filter, "
				    "save failure, loading transition, title-bar dismissal and API close failure GUI cancel verified");
			}
			break;
		case 39:
			throw std::runtime_error("Discard during root prompt failed to close the application");
	}
}
} // namespace Hyperion
