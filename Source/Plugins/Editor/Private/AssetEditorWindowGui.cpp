#include "AssetEditorWindow.h"
#include <algorithm>
#include <utility>

namespace Hyperion
{
void FAssetEditorWindow::Poll(bool bInBlocked)
{
	Window->Poll();
	if (bSaveThenClose && !Workspace.IsSaving() && !Workspace.HasPendingEdits())
	{
		bSaveThenClose = false;
		bClosePending = !Workspace.IsDirty();
		if (!bClosePending)
		{
			Error = "Some assets could not be saved. Cancel to review their errors, retry, or discard changes.";
		}
	}
	if (!Window->ShouldClose())
	{
		return;
	}
	Window->CancelClose();
	if (bInBlocked)
	{
		return;
	}
	bClosePending = !Workspace.IsDirty() && !Workspace.IsSaving();
	if (!bClosePending)
	{
		if (!bHidden && (Owner->Minimized() || Window->Minimized()))
		{
			Activate();
		}
		Workspace.CancelCloseDialog();
		Gui->FinishEditing();
		bRequestClose = !bCloseDialog;
		bCloseDialog = true;
	}
}

bool FAssetEditorWindow::ShouldClose() const
{
	return bClosePending;
}

void FAssetEditorWindow::RouteShortcuts(std::vector<FInputEvent>& InEvents)
{
	std::erase_if(InEvents,
	              [&](const FInputEvent& InEvent)
	              {
		              if (InEvent.Type != EEventType::Key || !InEvent.bDown || InEvent.bRepeat ||
		                  !(InEvent.Modifiers & 1))
		              {
			              return false;
		              }
		              if (InEvent.Key == EKey::S)
		              {
			              Gui->FinishEditing();
			              Workspace.SaveActive();
			              return true;
		              }
		              if ((InEvent.Key == EKey::Z || InEvent.Key == EKey::Y) &&
		                  (!Gui->IsEditingText() || Workspace.HasActiveInteraction()) && !Gui->DragPayload())
		              {
			              Gui->FinishEditing();
			              if (InEvent.Key == EKey::Y || (InEvent.Modifiers & 2))
			              {
				              Workspace.Redo();
			              }
			              else
			              {
				              Workspace.Undo();
			              }
			              return true;
		              }
		              return false;
	              });
}

void FAssetEditorWindow::Advance(float InDelta, std::vector<FInputEvent> InEvents, float InScale, bool bInBlocked,
                                 const std::filesystem::path& InCapture, bool bInVsync)
{
	Workspace.BeginFrame();
	if (!IsDrawable())
	{
		Gui->ResetInput();
		Workspace.SuspendInput(InEvents);
		return;
	}
	Gui->SetApplicationScale(InScale);
	if (!bInBlocked && !bCloseDialog && !Workspace.IsClosePending())
	{
		RouteShortcuts(InEvents);
	}
	auto Data = Draw(InDelta, InEvents, bInBlocked);
	Render(std::move(Data), InCapture, bInVsync);
}

void FAssetEditorWindow::DrawMenus()
{
	if (!Gui->BeginMenuBar())
	{
		return;
	}
	if (Gui->BeginMenu("File"))
	{
		if (Gui->MenuItem("Save Asset", "Ctrl+S"))
		{
			Workspace.SaveActive();
		}
		Gui->BeginDisabled(Workspace.HasPendingEdits());
		if (Gui->MenuItem("Save All Assets"))
		{
			Workspace.SaveAll();
		}
		Gui->EndDisabled();
		if (Gui->MenuItem("Close Asset Editor"))
		{
			Window->RequestClose();
		}
		Gui->EndMenu();
	}
	if (Gui->BeginMenu("Window"))
	{
		if (Gui->MenuItem("Preview", nullptr, bShowPreview))
		{
			bShowPreview = true;
		}
		if (Gui->MenuItem("Properties", nullptr, bShowProperties))
		{
			bShowProperties = true;
		}
		if (Gui->MenuItem("Reset Layout"))
		{
			bResetLayout = bShowPreview = bShowProperties = true;
		}
		Gui->EndMenu();
	}
	Gui->EndMenuBar();
}

FGuiDrawData FAssetEditorWindow::Draw(float InDelta, std::span<const FInputEvent> InEvents, bool bInBlocked)
{
	Gui->BeginFrame(Window->LogicalSize(), Window->PixelSize(), std::clamp(InDelta, .001f, .1f), InEvents);
	const bool bBlockEditing = bInBlocked || bCloseDialog;
	if (bBlockEditing)
	{
		Workspace.SuspendInput(InEvents);
	}
	Gui->BeginDisabled(bBlockEditing);
	DrawMenus();
	// Readiness changes during live edits must not resize the preview canvas.
	Gui->StatusBar(Workspace.ActiveStatus());
	Gui->DockSpace({"Asset Preview", {}, "Asset Properties", {}, {}, .35f}, std::exchange(bResetLayout, false));
	if (bShowPreview)
	{
		if (Gui->BeginWindow("Asset Preview", bShowPreview))
		{
			Workspace.DrawTabs(*Gui, InDelta, bBlockEditing ? std::span<const FInputEvent>{} : InEvents);
		}
		Gui->EndWindow();
	}
	if (bShowProperties)
	{
		if (Gui->BeginWindow("Asset Properties", bShowProperties))
		{
			Workspace.DrawProperties(*Gui);
		}
		Gui->EndWindow();
	}
	Gui->EndDisabled();
	if (!bInBlocked)
	{
		Workspace.DrawCloseDialog(*Gui);
		DrawCloseDialog();
	}
	return Gui->Render();
}

void FAssetEditorWindow::DrawCloseDialog()
{
	Bounds.clear();
	if (std::exchange(bRequestClose, false))
	{
		Gui->OpenPopup("Close Asset Editor");
	}
	if (!bCloseDialog)
	{
		return;
	}
	if (!Gui->BeginModal("Close Asset Editor", bCloseDialog))
	{
		if (!bCloseDialog)
		{
			bSaveThenClose = false;
			Error.clear();
		}
		return;
	}
	Gui->TextWrapped("Save changes to all open assets before closing this window?");
	if (Workspace.HasPendingEdits())
	{
		Gui->TextWrapped("Wait for pending edits to finish before saving, or discard them explicitly.");
	}
	Gui->TextWrapped(Error);
	if (Gui->Button("Save All and Close", !Workspace.HasPendingEdits() && !Workspace.IsSaving() && !bSaveThenClose))
	{
		Workspace.SaveAll();
		bSaveThenClose = true;
		Error.clear();
	}
	Bounds["save"] = Gui->LastItemBounds();
	Gui->SameLine();
	if (Gui->Button("Discard", !Workspace.IsSaving()))
	{
		bClosePending = true;
		Gui->ClosePopup();
	}
	Bounds["discard"] = Gui->LastItemBounds();
	Gui->SameLine();
	if (Gui->Button("Cancel"))
	{
		bCloseDialog = bSaveThenClose = false;
		Error.clear();
		Gui->ClosePopup();
	}
	Bounds["cancel"] = Gui->LastItemBounds();
	Gui->EndModal();
}
} // namespace Hyperion
