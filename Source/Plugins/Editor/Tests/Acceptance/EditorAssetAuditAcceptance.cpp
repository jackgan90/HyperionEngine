#include "EditorAcceptanceHarness.h"
#include "Hyperion/Core/Core.h"
#include <chrono>
#include <thread>

namespace Hyperion
{
namespace
{
void RequireAudit(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(InMessage);
	}
}

std::vector<FInputEvent> SaveShortcut()
{
	FInputEvent Event;
	Event.Type = EEventType::Key;
	Event.Key = EKey::S;
	Event.Modifiers = InputModifiers::Control;
	Event.bDown = true;
	return {Event};
}
} // namespace

void FEditorAcceptanceHarness::CheckAssetSaveShortcut()
{
	const auto State = Editor.DocumentState;
	const auto AssetDraft = HashArchive(Editor.AssetWorkspace->ActiveDocument()->Snapshot());
	const auto SendSave = [&]
	{
		auto Events = SaveShortcut();
		Editor.RouteHistoryShortcuts(Events);
		RequireAudit(!Editor.PendingSave, "Unavailable scene shortcut admitted a save");
	};
	for (bool* bModal : {&Editor.bOpenDialog, &Editor.bSaveDialog, &Editor.bAssetMessage, &Editor.bPreferencesDialog})
	{
		*bModal = true;
		SendSave();
		*bModal = false;
	}
	RequireAudit(!Editor.Transition.RequestWindowClose({.bSceneDirty = true}), "Dirty close skipped its decision");
	SendSave();
	Editor.Transition.Cancel();
	Editor.Transition.BeginSave(EEditorTransitionTarget::Close);
	Editor.Transition.SaveAdmitted(EEditorTransitionTarget::Close, false);
	SendSave();
	Editor.Transition.Cancel();
	{
		FRenderSession TemporarySession(Editor.Tasks, Editor.Session->GetResources(), Editor.Device->GetCapabilities());
		auto TemporaryScene = std::make_unique<FSceneInstance>(TemporarySession, Editor.Tasks, Editor.Assets, true);
		auto PreviousScene = std::move(Editor.Scene);
		Editor.Scene = std::move(TemporaryScene);
		try
		{
			Editor.Scene->Load("/Game/MissingSaveShortcutFixture.hasset");
			SendSave();
			const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
			while (Editor.Scene->GetStatus().Error.empty())
			{
				Editor.Scene->Tick();
				RequireAudit(std::chrono::steady_clock::now() < Deadline,
				             "Missing scene did not report a load failure");
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			SendSave();
		}
		catch (...)
		{
			Editor.Scene = std::move(PreviousScene);
			throw;
		}
		Editor.Scene = std::move(PreviousScene);
	}
	// A ready scene can still reject persistence. The shortcut must contain Snapshot's exception.
	RequireAudit(Editor.Scene->GetStatus().bReady, "Save shortcut fixture scene is not ready");
	FSceneNode Transient;
	Transient.Name = "Unsavable audit fixture";
	Transient.Model() = FSceneModelComponent{};
	const auto Handle = Editor.Scene->AddNode(std::move(Transient));
	SendSave();
	RequireAudit(Editor.Error.find("Cannot persist source-less model") != std::string::npos,
	             "Scene shortcut did not report the snapshot error");
	Editor.Scene->RemoveSubtree(Handle);
	Editor.Error.clear();
	RequireAudit(Editor.DocumentState == State &&
	                 HashArchive(Editor.AssetWorkspace->ActiveDocument()->Snapshot()) == AssetDraft,
	             "Failed scene save modified a document");
	Log(ELogLevel::Info, "Scene save shortcuts preserve documents during loading, modal state and snapshot failure");
}

void FEditorAcceptanceHarness::CheckPendingAssetEdit()
{
	RequireAudit(Editor.AssetWorkspace->IsDirty(), "Pending texture edit was treated as a clean workspace");
	RequireAudit(!Editor.AssetWorkspace->CanUndo() && !Editor.AssetWorkspace->CanRedo(),
	             "Pending texture edit allowed another history transaction");
	Editor.AssetWorkspace->SaveAll();
	RequireAudit(!Editor.AssetWorkspace->ActiveDocument()->IsSaving(), "Pending encoding saved the old texture draft");
	{
		FAssetEditorWindow Host(Editor.Tasks, *Editor.Device, *Editor.Compiler, *Editor.Session, *Editor.AssetWorkspace,
		                        Editor.Control, {});
		Host.Initialize(*Editor.Window, *Editor.WindowGroup, Editor.IO, Editor.Gui->ApplicationScale(), true);
		Host.NativeWindow().RequestClose();
		Host.Poll(false);
		RequireAudit(!Host.ShouldClose(), "Pending texture edit bypassed the native window close prompt");
	}
	const auto PreviousSavedState = Editor.SavedState;
	Editor.SceneDocument.MarkSaved(Editor.DocumentEpoch, Editor.DocumentState);
	RequireAudit(!Editor.IsDirty(), "Pending edit exit fixture must have a clean scene");
	Editor.Window->RequestClose();
	RequireAudit(!Editor.PollClose() && Editor.Transition.IsDecisionVisible(),
	             "Pending texture edit bypassed application exit protection");
	Editor.SceneDocument.MarkSaved(Editor.DocumentEpoch, PreviousSavedState);
	Editor.CancelDiscardAction();
	Scenario.bPendingAssetEditChecked = true;
	Log(ELogLevel::Info, "Pending texture edit blocks stale saves and protects native-window/application closure");
}
} // namespace Hyperion
