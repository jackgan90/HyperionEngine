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
	if (!ApplicationCloseState().bDirty && !PendingSave && !AssetWorkspace->IsSaving())
	{
		return true;
	}
	Window->CancelClose();
	Window->Restore();
	if (!Options.bHidden)
	{
		Window->Raise();
	}
	Transition.bPendingClose = true;
	Transition.bDiscardDialog = Transition.bRequestDiscard = true;
	return false;
}

void FEditorPlugin::CancelDiscardAction()
{
	Transition.Cancel();
}

void FEditorPlugin::DrawDiscardDialog()
{
	if (Transition.bRequestDiscard)
	{
		Gui->OpenPopup("Unsaved changes");
		Transition.bRequestDiscard = false;
	}
	if (!Transition.bDiscardDialog)
	{
		return;
	}
	if (!Gui->BeginModal("Unsaved changes", Transition.bDiscardDialog))
	{
		if (!Transition.bDiscardDialog)
		{
			CancelDiscardAction();
		}
		return;
	}
	DiscardTitleBounds = Gui->LastItemBounds();
	const auto* Imports = Context.Find<FAssetImportWorkspace>();
	const bool bImportDirty = Imports && Imports->ContentRootState().bDirty;
	if (bImportDirty)
	{
		Gui->TextWrapped("Import drafts have unpublished edits. Cancel to publish them, or explicitly discard changes. "
		                 "Save does not import assets.");
	}
	if (AssetWorkspace->HasPendingEdits())
	{
		Gui->TextWrapped("Wait for pending texture edits to finish before saving, or discard them explicitly.");
	}
	Gui->TextWrapped(PendingSave ? "Wait for the current save to finish."
	                 : Transition.PendingRoot
	                     ? "Save changes to the current asset root before switching, discard them, or cancel."
	                     : "Open documents have unsaved changes. Save, discard, or cancel to keep editing.");
	if (Transition.bPendingClose)
	{
		if (Gui->Button("Save all and exit", !bImportDirty && !PendingSave && !AssetWorkspace->IsSaving() &&
		                                         !AssetWorkspace->HasPendingEdits()))
		{
			SaveBeforeClose();
		}
		Gui->SameLine();
	}
	if (Transition.PendingRoot &&
	    Gui->Button("Save and switch",
	                !bImportDirty && !PendingSave && !Transition.bSaveThenSwitch && !AssetWorkspace->HasPendingEdits()))
	{
		SaveBeforeRootSwitch();
	}
	if (Transition.PendingRoot)
	{
		SaveSwitchBounds = Gui->LastItemBounds();
		Gui->SameLine();
	}
	if (Gui->Button("Discard changes", !PendingSave && !AssetWorkspace->IsSaving()))
	{
		ConfirmDiscardAction();
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

void FEditorPlugin::SaveBeforeRootSwitch()
{
	try
	{
		Transition.bSaveThenSwitch = true;
		AssetWorkspace->SaveAll();
		if (!IsDirty())
		{
			Transition.bDiscardDialog = false;
			Gui->ClosePopup();
		}
		else if (CurrentPath.empty())
		{
			SavePath = "/Game/Scenes/Untitled.hasset";
			bSaveDialog = bRequestSaveDialog = true;
			Transition.bDiscardDialog = false;
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
		Transition.bSaveThenSwitch = false;
	}
}

void FEditorPlugin::ConfirmDiscardAction()
{
	const auto Action = Transition.DiscardAction();
	if (Action == EEditorDiscardAction::Document || Action == EEditorDiscardAction::Open)
	{
		ResetDocument();
	}
	Transition.bDiscardDialog = false;
	Gui->ClosePopup();
	switch (Action)
	{
		case EEditorDiscardAction::Close:
			DiscardBeforeClose();
			break;
		case EEditorDiscardAction::Root:
			Transition.ConfirmRootDiscard();
			break;
		case EEditorDiscardAction::Open:
			OpenScene(std::exchange(Transition.PendingOpen, {}));
			break;
		case EEditorDiscardAction::Document:
			break;
	}
	Transition.FinishDiscard();
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
	CancelReparentGesture();
	OutlinerSelection.Reset();
	OutlinerRows.clear();
	ReparentOpenNodes.clear();
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
	if (Transition.bSaveThenClose)
	{
		Transition.CloseError = Error;
	}
	SaveStatus = Error;
	if (Transition.bSaveThenSwitch || Transition.PendingRoot || !Transition.RequestedRoot.empty())
	{
		Transition.RequestedRoot.clear();
		Transition.PendingRoot.reset();
		Transition.bSaveThenSwitch = Transition.bCommitRoot = Transition.bDiscardDialog = false;
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
		// Save consumes the current draft even when the user clicks it without pressing Enter.
		Gui->InputText("Scene path", SavePath, false, true);
		InspectionBounds["document/save-path"] = Gui->LastItemBounds();
		if (Gui->Button("Save", !PendingSave && Scene->GetStatus().bReady))
		{
			try
			{
				if (Transition.bSaveThenClose)
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
		InspectionBounds["document/save-confirm"] = Gui->LastItemBounds();
		Gui->SameLine();
		if (Gui->Button("Cancel"))
		{
			bSaveDialog = false;
			Transition.bSaveThenClose = Transition.bPendingClose = false;
			if (Transition.bSaveThenSwitch)
			{
				Transition.bSaveThenSwitch = false;
				Transition.PendingRoot.reset();
			}
			Gui->ClosePopup();
		}
		Gui->TextWrapped(Error);
		Gui->EndModal();
	}
	if (bWasSaveDialog && !bSaveDialog && Transition.bSaveThenSwitch && !PendingSave)
	{
		Transition.bSaveThenSwitch = false;
		Transition.PendingRoot.reset();
	}
	if (bWasSaveDialog && !bSaveDialog && Transition.bSaveThenClose && !PendingSave)
	{
		Transition.bSaveThenClose = Transition.bPendingClose = false;
	}
}

void FEditorPlugin::SaveBeforeClose()
{
	try
	{
		if (IsDirty() && CurrentPath.empty())
		{
			Transition.bSaveThenClose = true;
			SavePath = "/Game/Scenes/Untitled.hasset";
			bSaveDialog = bRequestSaveDialog = true;
			Transition.bDiscardDialog = false;
			Gui->ClosePopup();
			return;
		}
		StartSaveBeforeClose(CurrentPath);
	}
	catch (const std::exception& Failure)
	{
		Transition.bSaveThenClose = false;
		Error = Failure.what();
	}
}

void FEditorPlugin::PollSavedClose()
{
	if (!Transition.bSaveThenClose || bSaveDialog || PendingSave || AssetWorkspace->IsSaving() ||
	    AssetWorkspace->HasPendingEdits())
	{
		return;
	}
	Transition.bSaveThenClose = false;
	if (!IsDirty() && !AssetWorkspace->IsDirty())
	{
		Transition.bPendingClose = false;
		Transition.CloseState = "closing";
		Window->RequestClose();
	}
	else
	{
		Transition.CloseState = "failed";
		if (Transition.CloseError.empty())
		{
			Transition.CloseError =
			    "Documents remain dirty after save; inspect asset.info or scene.status before retrying";
		}
		Transition.bDiscardDialog = Transition.bRequestDiscard = true;
	}
}
} // namespace Hyperion
