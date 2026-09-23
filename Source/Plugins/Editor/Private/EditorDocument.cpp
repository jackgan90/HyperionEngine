#include "EditorApplication.h"
#include "Hyperion/Core/Core.h"

namespace Hyperion
{
void FEditorPlugin::SelectObject(std::optional<FSceneHandle> InHandle)
{
	SetSelection(FEditorSelection(InHandle));
}

void FEditorPlugin::SetSelection(FEditorSelection InSelection)
{
	if (Selection != InSelection)
	{
		FinishInspectorEdit();
	}
	SceneDocument.ReplaceSelection(std::move(InSelection));
	bSelectionInitialized = true;
}

void FEditorPlugin::ClickObject(std::optional<FSceneHandle> InHandle, bool bInToggle)
{
	if (!bInToggle)
	{
		SelectObject(InHandle);
	}
	else if (InHandle)
	{
		auto Updated = Selection;
		Updated.Toggle(*InHandle);
		SetSelection(std::move(Updated));
	}
}

bool FEditorPlugin::PollClose()
{
	if (!Window->ShouldClose())
	{
		return false;
	}
	if (!IsDirty() && !PendingSave && !AssetWorkspace->IsDirty() && !AssetWorkspace->IsSaving())
	{
		return true;
	}
	Window->CancelClose();
	Window->Restore();
	if (!Options.bHidden)
	{
		Window->Raise();
	}
	bPendingClose = true;
	bDiscardDialog = bRequestDiscard = true;
	return false;
}

void FEditorPlugin::CancelDiscardAction()
{
	CloseState = "idle";
	CloseError.clear();
	bDiscardDialog = bPendingClose = bSaveThenClose = false;
	PendingOpen.clear();
	PendingRoot.reset();
	bCommitRoot = bSaveThenSwitch = false;
}

void FEditorPlugin::DrawDiscardDialog()
{
	if (bRequestDiscard)
	{
		Gui->OpenPopup("Unsaved changes");
		bRequestDiscard = false;
	}
	if (!bDiscardDialog)
	{
		return;
	}
	if (!Gui->BeginModal("Unsaved changes", bDiscardDialog))
	{
		if (!bDiscardDialog)
		{
			CancelDiscardAction();
		}
		return;
	}
	DiscardTitleBounds = Gui->LastItemBounds();
	if (AssetWorkspace->HasPendingEdits())
	{
		Gui->TextWrapped("Wait for pending texture edits to finish before saving, or discard them explicitly.");
	}
	Gui->TextWrapped(PendingSave   ? "Wait for the current save to finish."
	                 : PendingRoot ? "Save changes to the current asset root before switching, discard them, or cancel."
	                               : "Open documents have unsaved changes. Save, discard, or cancel to keep editing.");
	if (bPendingClose)
	{
		if (Gui->Button("Save all and exit",
		                !PendingSave && !AssetWorkspace->IsSaving() && !AssetWorkspace->HasPendingEdits()))
		{
			SaveBeforeClose();
		}
		Gui->SameLine();
	}
	if (PendingRoot &&
	    Gui->Button("Save and switch", !PendingSave && !bSaveThenSwitch && !AssetWorkspace->HasPendingEdits()))
	{
		try
		{
			bSaveThenSwitch = true;
			AssetWorkspace->SaveAll();
			if (!IsDirty())
			{
				bDiscardDialog = false;
				Gui->ClosePopup();
			}
			else if (CurrentPath.empty())
			{
				SavePath = "/Game/Scenes/Untitled.hasset";
				bSaveDialog = bRequestSaveDialog = true;
				bDiscardDialog = false;
				Gui->ClosePopup();
			}
			else
			{
				SaveScene(CurrentPath);
			}
		}
		catch (const std::exception& Failure)
		{
			Error = Failure.what();
			bSaveThenSwitch = false;
		}
	}
	if (PendingRoot)
	{
		SaveSwitchBounds = Gui->LastItemBounds();
		Gui->SameLine();
	}
	if (Gui->Button("Discard changes", !PendingSave && !AssetWorkspace->IsSaving()))
	{
		if (!PendingRoot && !bPendingClose)
		{
			ResetDocument();
		}
		bDiscardDialog = false;
		Gui->ClosePopup();
		if (bPendingClose)
		{
			DiscardBeforeClose();
		}
		else if (PendingRoot)
		{
			bDiscardRoot = true;
			bCommitRoot = true;
		}
		else if (!PendingOpen.empty())
		{
			const auto Path = std::exchange(PendingOpen, {});
			OpenScene(Path);
		}
		bPendingClose = false;
		bSaveThenClose = false;
	}
	DiscardChangesBounds = Gui->LastItemBounds();
	Gui->SameLine();
	if (Gui->Button("Cancel"))
	{
		CancelDiscardAction();
		Gui->ClosePopup();
	}
	CancelChangesBounds = Gui->LastItemBounds();
	Gui->EndModal();
}

bool FEditorPlugin::IsDirty() const
{
	if (GizmoEdit)
	{
		for (const auto& Target : GizmoEdit->Targets)
		{
			const auto* Node = Scene->FindNode(Target.Handle);
			if (Node && Node->Local().Values != Target.Initial.Values)
			{
				return true;
			}
		}
	}
	return SceneDocument.IsDirty();
}

void FEditorPlugin::ResetDocument()
{
	SetPreviewCamera({});
	FinishInspectorEdit();
	SceneDocument.Reset();
	SaveStatus.clear();
}

void FEditorPlugin::CommitEdit(FSceneHandle InHandle, FSceneNode InCandidate, std::uint64_t InExpectedRevision,
                               std::uint64_t InInteraction)
{
	CommitEdits({{InHandle, std::move(InCandidate)}}, InExpectedRevision, InInteraction);
}

void FEditorPlugin::Undo()
{
	FinishInspectorEdit();
	Error.clear();
	SceneDocument.Undo();
}

void FEditorPlugin::Redo()
{
	FinishInspectorEdit();
	Error.clear();
	SceneDocument.Redo();
}

void FEditorPlugin::SaveScene(const std::string& InDestination)
{
	FinishInspectorEdit();
	SceneDocument.Save(InDestination);
	SaveStatus = "Saving scene...";
}

void FEditorPlugin::PollSave()
{
	SceneDocument.PollSave();
	const auto Outcome = SceneDocument.TakeSaveOutcome();
	if (!Outcome || !Outcome->bCurrentDocument)
	{
		return;
	}
	if (Outcome->bSucceeded)
	{
		LastSaveMilliseconds = Outcome->Milliseconds;
		SaveStatus = "Saved: " + CurrentPath;
		RefreshContent();
		return;
	}
	Error = "Save failed: " + Outcome->Error;
	if (bSaveThenClose)
	{
		CloseError = Error;
	}
	SaveStatus = Error;
	if (bSaveThenSwitch || PendingRoot || !RequestedRoot.empty())
	{
		RequestedRoot.clear();
		PendingRoot.reset();
		bSaveThenSwitch = bCommitRoot = bDiscardDialog = false;
		Gui->ClosePopups();
		AssetMessage = Error;
		bAssetMessage = bRequestAssetMessage = true;
	}
}

void FEditorPlugin::DrawSaveDialog()
{
	const bool bWasSaveDialog = bSaveDialog;
	if (bRequestSaveDialog)
	{
		Gui->OpenPopup("Save Scene As");
		bRequestSaveDialog = false;
	}
	if (bSaveDialog && Gui->BeginModal("Save Scene As", bSaveDialog))
	{
		Gui->TextWrapped("Save the scene document. Shared model and material assets keep their references.");
		Gui->InputText("Scene path", SavePath, true, true);
		if (Gui->Button("Save", !PendingSave && Scene->GetStatus().bReady))
		{
			try
			{
				if (bSaveThenClose)
				{
					StartSaveBeforeClose(SavePath);
				}
				else
				{
					SaveScene(SavePath);
				}
				bSaveDialog = false;
				Gui->ClosePopup();
			}
			catch (const std::exception& Failure)
			{
				Error = Failure.what();
			}
		}
		Gui->SameLine();
		if (Gui->Button("Cancel"))
		{
			bSaveDialog = false;
			bSaveThenClose = bPendingClose = false;
			if (bSaveThenSwitch)
			{
				bSaveThenSwitch = false;
				PendingRoot.reset();
			}
			Gui->ClosePopup();
		}
		Gui->TextWrapped(Error);
		Gui->EndModal();
	}
	if (bWasSaveDialog && !bSaveDialog && bSaveThenSwitch && !PendingSave)
	{
		bSaveThenSwitch = false;
		PendingRoot.reset();
	}
	if (bWasSaveDialog && !bSaveDialog && bSaveThenClose && !PendingSave)
	{
		bSaveThenClose = bPendingClose = false;
	}
}

void FEditorPlugin::SaveBeforeClose()
{
	try
	{
		if (IsDirty() && CurrentPath.empty())
		{
			bSaveThenClose = true;
			SavePath = "/Game/Scenes/Untitled.hasset";
			bSaveDialog = bRequestSaveDialog = true;
			bDiscardDialog = false;
			Gui->ClosePopup();
			return;
		}
		StartSaveBeforeClose(CurrentPath);
	}
	catch (const std::exception& Failure)
	{
		bSaveThenClose = false;
		Error = Failure.what();
	}
}

void FEditorPlugin::PollSavedClose()
{
	if (!bSaveThenClose || bSaveDialog || PendingSave || AssetWorkspace->IsSaving() ||
	    AssetWorkspace->HasPendingEdits())
	{
		return;
	}
	bSaveThenClose = false;
	if (!IsDirty() && !AssetWorkspace->IsDirty())
	{
		bPendingClose = false;
		CloseState = "closing";
		Window->RequestClose();
	}
	else
	{
		CloseState = "failed";
		if (CloseError.empty())
		{
			CloseError = "Documents remain dirty after save; inspect asset.info or scene.status before retrying";
		}
		bDiscardDialog = bRequestDiscard = true;
	}
}
} // namespace Hyperion
