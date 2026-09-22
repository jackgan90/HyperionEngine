#include "EditorApplication.h"
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
	Event.Modifiers = 1;
	Event.bDown = true;
	return {Event};
}
} // namespace

void FEditorPlugin::CheckAssetSaveShortcut()
{
	const auto State = DocumentState;
	const auto AssetDraft = HashArchive(AssetWorkspace->ActiveDocument()->Snapshot());
	const auto SendSave = [&]
	{
		auto Events = SaveShortcut();
		RouteHistoryShortcuts(Events);
		RequireAudit(!PendingSave, "Unavailable scene shortcut admitted a save");
	};
	for (bool* bModal :
	     {&bOpenDialog, &bSaveDialog, &bDiscardDialog, &bAssetMessage, &bPreferencesDialog, &bSaveThenClose})
	{
		*bModal = true;
		SendSave();
		*bModal = false;
	}
	{
		FRenderSession TemporarySession(Tasks, Session->GetResources(), Device->GetCapabilities());
		auto TemporaryScene = std::make_unique<FSceneInstance>(TemporarySession, Tasks, Assets, true);
		auto PreviousScene = std::move(Scene);
		Scene = std::move(TemporaryScene);
		try
		{
			Scene->Load("/Game/MissingSaveShortcutFixture.hasset");
			SendSave();
			const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
			while (Scene->GetStatus().Error.empty())
			{
				Scene->Tick();
				RequireAudit(std::chrono::steady_clock::now() < Deadline,
				             "Missing scene did not report a load failure");
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			SendSave();
		}
		catch (...)
		{
			Scene = std::move(PreviousScene);
			throw;
		}
		Scene = std::move(PreviousScene);
	}
	// A ready scene can still reject persistence. The shortcut must contain Snapshot's exception.
	RequireAudit(Scene->GetStatus().bReady, "Save shortcut fixture scene is not ready");
	FSceneNode Transient;
	Transient.Name = "Unsavable audit fixture";
	Transient.Model() = FSceneModelComponent{};
	const auto Handle = Scene->AddNode(std::move(Transient));
	SendSave();
	RequireAudit(Error.find("Cannot persist source-less model") != std::string::npos,
	             "Scene shortcut did not report the snapshot error");
	Scene->RemoveSubtree(Handle);
	Error.clear();
	RequireAudit(DocumentState == State && HashArchive(AssetWorkspace->ActiveDocument()->Snapshot()) == AssetDraft,
	             "Failed scene save modified a document");
	Log(ELogLevel::Info, "Scene save shortcuts preserve documents during loading, modal state and snapshot failure");
}

void FEditorPlugin::CheckPendingAssetEdit()
{
	RequireAudit(AssetWorkspace->IsDirty(), "Pending texture edit was treated as a clean workspace");
	RequireAudit(!AssetWorkspace->CanUndo() && !AssetWorkspace->CanRedo(),
	             "Pending texture edit allowed another history transaction");
	AssetWorkspace->SaveAll();
	RequireAudit(!AssetWorkspace->ActiveDocument()->IsSaving(), "Pending encoding saved the old texture draft");
	{
		FAssetEditorWindow Host(Tasks, *Device, *Compiler, *Session, *AssetWorkspace, Control, {});
		Host.Initialize(*Window, IO, Gui->ApplicationScale(), true);
		Host.NativeWindow().RequestClose();
		Host.Poll(false);
		RequireAudit(!Host.ShouldClose(), "Pending texture edit bypassed the native window close prompt");
	}
	const auto PreviousSavedState = SavedState;
	SavedState = DocumentState;
	RequireAudit(!IsDirty(), "Pending edit exit fixture must have a clean scene");
	Window->RequestClose();
	RequireAudit(!PollClose() && bDiscardDialog, "Pending texture edit bypassed application exit protection");
	SavedState = PreviousSavedState;
	CancelDiscardAction();
	bRequestDiscard = false;
	bPendingAssetEditChecked = true;
	Log(ELogLevel::Info, "Pending texture edit blocks stale saves and protects native-window/application closure");
}
} // namespace Hyperion
