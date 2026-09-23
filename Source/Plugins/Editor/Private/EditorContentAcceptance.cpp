#include "EditorApplication.h"
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

void FEditorPlugin::ExerciseContentInput(std::vector<FInputEvent>& InEvents)
{
	if (!Scene->GetStatus().Error.empty())
	{
		throw std::runtime_error(Scene->GetStatus().Error);
	}
	switch (ExerciseStep)
	{
		case 0:
			QueueContentRoot(Options.ExerciseContent / "A");
			++ExerciseStep;
			break;
		case 1:
			CheckContent(CurrentPath.empty() && !Selection && History.empty(),
			             "Root switch retained old document state");
			RequestOpenAsset("/Game/Texture.hasset");
			++ExerciseStep;
			break;
		case 2:
			if (!PendingAssetOpen)
			{
				CheckContent(AssetWorkspace->HasActive(), "Texture asset editor did not open");
				Gui->ClosePopups();
				bAssetMessage = bRequestAssetMessage = false;
				RequestOpenAsset("/Game/Scene.hasset");
				++ExerciseStep;
			}
			break;
		case 3:
			if (Scene->GetStatus().bReady && ReadyFrames > 8 && !Browser->IsScanning())
			{
				CheckContent(Scene->FindNode(Scene->FindHandle("model"))->Name == "A", "Wrong initial root scene");
				CheckContent(std::find(ScenePaths.begin(), ScenePaths.end(), "/Game/Other/Scene.hasset") !=
				                 ScenePaths.end(),
				             "Uncataloged scene was not discovered");
				QueueContentRoot(Options.ExerciseContent / "A");
				++ExerciseStep;
			}
			break;
		case 4:
		{
			CheckContent(CurrentPath == "/Game/Scene.hasset", "Same-root selection closed the scene");
			const auto Handle = Scene->FindHandle("model");
			auto Node = *Scene->FindNode(Handle);
			Node.Name = "A saved";
			CommitEdit(Handle, std::move(Node), Scene->GetRevision());
			QueueContentRoot(Options.ExerciseContent / "B");
			++ExerciseStep;
			break;
		}
		case 5:
			ExerciseClick(InEvents, CancelChangesBounds);
			break;
		case 6:
			CheckContent(!PendingRoot && IsDirty() && CurrentPath == "/Game/Scene.hasset",
			             "Cancel lost document changes");
			QueueContentRoot(Options.ExerciseContent / "B");
			++ExerciseStep;
			break;
		case 7:
			ExerciseClick(InEvents, SaveSwitchBounds);
			break;
		default:
			ExerciseContentSwitch(InEvents);
			break;
	}
}

void FEditorPlugin::ExerciseContentSwitch(std::vector<FInputEvent>& InEvents)
{
	switch (ExerciseStep)
	{
		case 8:
			if (!PendingRoot && !PendingSave)
			{
				CheckContent(CurrentPath.empty() && !IsDirty() && History.empty(),
				             "Saved root transition retained document");
				RequestOpenAsset("/Game/Scene.hasset");
				++ExerciseStep;
			}
			break;
		case 9:
			if (Scene->GetStatus().bReady && ReadyFrames > 8)
			{
				CheckContent(Scene->FindNode(Scene->FindHandle("model"))->Name == "B", "Root B reused cached scene A");
				const auto Handle = Scene->FindHandle("model");
				auto Node = *Scene->FindNode(Handle);
				Node.Name = "B discarded";
				CommitEdit(Handle, std::move(Node), Scene->GetRevision());
				QueueContentRoot(Options.ExerciseContent / "A");
				++ExerciseStep;
			}
			break;
		case 10:
			ExerciseClick(InEvents, DiscardChangesBounds);
			break;
		case 11:
			if (!PendingRoot)
			{
				RequestOpenAsset("/Game/Scene.hasset");
				++ExerciseStep;
			}
			break;
		case 12:
			if (Scene->GetStatus().bReady && ReadyFrames > 8)
			{
				CheckContent(Scene->FindNode(Scene->FindHandle("model"))->Name == "A saved",
				             "Save did not target old root A");
				const auto B =
				    DecodeAsset(IO.FileSystem()->Read(Options.ExerciseContent / "B/Scene.hasset", 1024 * 1024));
				const auto Manifest = ReadRecord(RecordType<FSceneManifest>(), B.Object);
				CheckContent(std::static_pointer_cast<FSceneManifest>(Manifest)->Nodes.front().Name == "B",
				             "Discard unexpectedly saved B");
				CheckContent(Options.Preferences.RecentRoots.size() == 2, "Recent roots did not deduplicate");
				++ExerciseStep;
			}
			break;
		default:
			ExerciseContentBrowser(InEvents);
			break;
	}
}

void FEditorPlugin::ExerciseContentBrowser(std::vector<FInputEvent>& InEvents)
{
	switch (ExerciseStep)
	{
		case 13:
			if (ContentTileBounds.contains("/Game/Other"))
			{
				ContentClickBounds = ContentTileBounds.at("/Game/Other");
				ExerciseClick(InEvents, ContentClickBounds);
			}
			break;
		case 14:
			ExerciseClick(InEvents, ContentClickBounds);
			break;
		case 15:
		case 16:
			CheckContent(Browser->SelectedDirectory == "/Game/Other", "Folder double-click did not navigate");
			if (ContentTileBounds.contains("/Game/Other/Scene.hasset"))
			{
				ExerciseClick(InEvents, ContentTileBounds.at("/Game/Other/Scene.hasset"));
			}
			break;
		case 17:
			if (Scene->GetStatus().bReady && ReadyFrames > 8 && CurrentPath == "/Game/Other/Scene.hasset")
			{
				Browser->Navigate("/Game");
				++ExerciseStep;
			}
			break;
		case 18:
			if (!Browser->IsScanning() && ContentTileBounds.contains("/Game/.assets"))
			{
				CheckContent(!ContentTileBounds.contains("/Game/.cache"), "Cache directory remained visible");
				ExerciseStep = 20;
			}
			break;
		default:
			ExerciseContentFailures(InEvents);
			break;
	}
}

void FEditorPlugin::ExerciseContentFailures(std::vector<FInputEvent>& InEvents)
{
	const auto OldScene = Options.ExerciseContent / "A/Other/Scene.hasset";
	switch (ExerciseStep)
	{
		case 20:
		{
			const auto Handle = Scene->FindHandle("model");
			auto Node = *Scene->FindNode(Handle);
			Node.Name = "Unsaved after write failure";
			CommitEdit(Handle, std::move(Node), Scene->GetRevision());
			std::filesystem::permissions(OldScene, std::filesystem::perms::owner_read);
			QueueContentRoot(Options.ExerciseContent / "B");
			++ExerciseStep;
			break;
		}
		case 21:
			ExerciseClick(InEvents, SaveSwitchBounds);
			break;
		case 22:
			if (!PendingSave && !PendingRoot)
			{
				std::filesystem::permissions(OldScene, std::filesystem::perms::owner_all);
				CheckContent(IsDirty() && bAssetMessage && AssetMessage.find("Save failed") != std::string::npos,
				             "Save failure did not preserve old document");
				Gui->ClosePopups();
				bAssetMessage = bRequestAssetMessage = false;
				QueueContentRoot(Options.ExerciseContent / "Missing");
				++ExerciseStep;
			}
			break;
		case 23:
			CheckContent(IsDirty() && bAssetMessage && CurrentPath == "/Game/Other/Scene.hasset",
			             "Invalid root destroyed old document");
			Gui->ClosePopups();
			bAssetMessage = bRequestAssetMessage = false;
			QueueContentRoot(Options.ExerciseContent / "B");
			++ExerciseStep;
			break;
		case 24:
			ExerciseClick(InEvents, DiscardChangesBounds);
			break;
		case 25:
			if (!PendingRoot)
			{
				OpenScene("/Game/Scene.hasset");
				QueueContentRoot(Options.ExerciseContent / "A");
				++ExerciseStep;
			}
			break;
		case 26:
			if (!PendingRoot && !Browser->IsScanning())
			{
				CheckContent(CurrentPath.empty() && History.empty() && !Selection &&
				                 SameAssetRoot(Context.Require<FContentRootService>().Directory(),
				                               Options.ExerciseContent / "A"),
				             "Loading transition retained old content");
				OpenScene("/Game/Scene.hasset");
				++ExerciseStep;
			}
			break;
		default:
			ExerciseContentDismissal(InEvents);
			break;
	}
}

void FEditorPlugin::ExerciseContentDismissal(std::vector<FInputEvent>& InEvents)
{
	switch (ExerciseStep)
	{
		case 27:
			if (Scene->GetStatus().bReady && ReadyFrames > 8)
			{
				const auto Handle = Scene->FindHandle("model");
				auto Node = *Scene->FindNode(Handle);
				Node.Name = "A saved after dismissal";
				CommitEdit(Handle, std::move(Node), Scene->GetRevision());
				QueueContentRoot(Options.ExerciseContent / "B");
				++ExerciseStep;
			}
			break;
		case 28:
			CheckContent(PendingRoot.has_value() && bDiscardDialog, "Root prompt was not prepared");
			// Keep the real save queued until the title-bar click has been processed. The timeout
			// permits orderly shutdown even if an earlier assertion interrupts the exercise.
			ContentSaveGate = std::make_shared<std::binary_semaphore>(0);
			Tasks.Dispatch({EDomain::Io},
			               [Gate = ContentSaveGate]
			               {
				               std::ignore = Gate->try_acquire_for(std::chrono::seconds(30));
			               });
			++ExerciseStep;
			break;
		case 29:
			ExerciseClick(InEvents, SaveSwitchBounds);
			break;
		case 30:
		{
			CheckContent(PendingSave && !PendingSave->Result.Ready() && bSaveThenSwitch,
			             "Save was not held pending during dismissal");
			// BeginModal leaves the title bar as the last item; its rightmost square contains X.
			auto CloseBounds = DiscardTitleBounds;
			CloseBounds.X = CloseBounds.Z - (CloseBounds.W - CloseBounds.Y);
			ExerciseClick(InEvents, CloseBounds);
			break;
		}
		case 31:
			CheckContent(PendingSave && !PendingSave->Result.Ready() && !PendingRoot && !bDiscardDialog &&
			                 !bSaveThenSwitch && !bCommitRoot,
			             "Title-bar dismissal retained the root transition");
			ContentSaveGate->release();
			ContentSaveGate.reset();
			++ExerciseStep;
			break;
		case 32:
			if (!PendingSave)
			{
				CheckContent(!IsDirty() && CurrentPath == "/Game/Scene.hasset" &&
				                 Scene->FindNode(Scene->FindHandle("model"))->Name == "A saved after dismissal" &&
				                 SameAssetRoot(Context.Require<FContentRootService>().Directory(),
				                               Options.ExerciseContent / "A"),
				             "Finishing the dismissed save switched roots or lost the document");
				const auto Saved =
				    DecodeAsset(IO.FileSystem()->Read(Options.ExerciseContent / "A/Scene.hasset", 1024 * 1024));
				const auto Manifest = ReadRecord(RecordType<FSceneManifest>(), Saved.Object);
				CheckContent(std::static_pointer_cast<FSceneManifest>(Manifest)->Nodes.front().Name ==
				                 "A saved after dismissal",
				             "Dismissal prevented the admitted save from completing against A");
				const auto Handle = Scene->FindHandle("model");
				auto Node = *Scene->FindNode(Handle);
				Node.Name = "Discard on application close";
				CommitEdit(Handle, std::move(Node), Scene->GetRevision());
				QueueContentRoot(Options.ExerciseContent / "B");
				++ExerciseStep;
			}
			break;
		default:
			ExerciseContentClose(InEvents);
			break;
	}
}

void FEditorPlugin::ExerciseContentClose(std::vector<FInputEvent>& InEvents)
{
	switch (ExerciseStep)
	{
		case 33:
			CheckContent(PendingRoot.has_value() && bDiscardDialog, "Close overlap lacked a root prompt");
			CancelDiscardAction();
			Gui->ClosePopups();
			std::filesystem::permissions(Options.ExerciseContent / "A/Scene.hasset",
			                             std::filesystem::perms::owner_read);
			RequestApplicationClose({EApplicationCloseAction::Save, CurrentPath});
			++ExerciseStep;
			break;
		case 34:
			if (CloseState == "failed" && !PendingSave)
			{
				std::filesystem::permissions(Options.ExerciseContent / "A/Scene.hasset",
				                             std::filesystem::perms::owner_all);
				CheckContent(bPendingClose && bDiscardDialog && IsDirty() && !Window->ShouldClose(),
				             "Failed API save-close lost GUI exit intent or dirty work");
				++ExerciseStep;
			}
			break;
		case 35:
			ExerciseClick(InEvents, CancelChangesBounds);
			break;
		case 36:
			CheckContent(CloseState == "idle" && !bPendingClose && !bDiscardDialog && IsDirty(),
			             "GUI cancel after API save-close failure lost work or retained exit intent");
			QueueContentRoot(Options.ExerciseContent / "B");
			++ExerciseStep;
			break;
		case 37:
			CheckContent(PendingRoot.has_value() && bDiscardDialog, "Close overlap lacked a root prompt");
			Window->RequestClose();
			++ExerciseStep;
			break;
		case 38:
			CheckContent(bPendingClose && PendingRoot && IsDirty(), "Close overlap was not exercised");
			ExerciseClick(InEvents, DiscardChangesBounds);
			if (ExerciseStep == 39)
			{
				bContentVerified = true;
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
