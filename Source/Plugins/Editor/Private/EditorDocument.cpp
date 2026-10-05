#include "EditorApplication.h"
#include "Hyperion/Content/ContentPaths.h"
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
	if (Transition.RequestWindowClose(DocumentSaveProgress()))
	{
		return true;
	}
	Window->CancelClose();
	if (Window->Minimized())
	{
		Window->Restore();
	}
	if (!Options.bHidden)
	{
		Window->Raise();
	}
	return false;
}

void FEditorPlugin::CancelDiscardAction()
{
	Transition.Cancel();
}

void FEditorPlugin::DrawDiscardDialog()
{
	if (Transition.TakeDecisionRequest())
	{
		Gui->OpenPopup("Unsaved changes");
	}
	if (!Transition.IsDecisionVisible())
	{
		return;
	}
	bool bOpen = true;
	if (!Gui->BeginModal("Unsaved changes", bOpen, true))
	{
		if (!bOpen)
		{
			CancelDiscardAction();
		}
		return;
	}
	Acceptance.ObserveWidget(EEditorWidget::DiscardTitle, Gui->LastItemBounds());
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
	                 : Transition.HasPendingRoot()
	                     ? "Save changes to the current asset root before switching, discard them, or cancel."
	                     : "Open documents have unsaved changes. Save, discard, or cancel to keep editing.");
	const bool bPendingClose = Transition.HasPendingClose();
	const bool bPendingRoot = Transition.HasPendingRoot();
	std::vector<const char*> ButtonLabels;
	if (bPendingClose)
	{
		ButtonLabels.push_back("Save all and exit");
	}
	if (bPendingRoot)
	{
		ButtonLabels.push_back("Save and switch");
	}
	ButtonLabels.push_back("Discard changes");
	ButtonLabels.push_back("Cancel");
	std::size_t ButtonIndex = 0;
	if (bPendingClose)
	{
		if (Gui->ButtonInCenteredRow(ButtonLabels, ButtonIndex++,
		                             !bImportDirty && !PendingSave && !AssetWorkspace->IsSaving() &&
		                                 !AssetWorkspace->HasPendingEdits()))
		{
			SaveBeforeClose();
		}
	}
	if (bPendingRoot && Gui->ButtonInCenteredRow(ButtonLabels, ButtonIndex++,
	                                             !bImportDirty && !PendingSave && !Transition.IsSavingRoot() &&
	                                                 !AssetWorkspace->HasPendingEdits()))
	{
		SaveBeforeRootSwitch();
	}
	if (bPendingRoot)
	{
		Acceptance.ObserveWidget(EEditorWidget::SaveSwitch, Gui->LastItemBounds());
	}
	if (Gui->ButtonInCenteredRow(ButtonLabels, ButtonIndex++, !PendingSave && !AssetWorkspace->IsSaving()))
	{
		ConfirmDiscardAction();
	}
	Acceptance.ObserveWidget(EEditorWidget::DiscardChanges, Gui->LastItemBounds());
	if (Gui->ButtonInCenteredRow(ButtonLabels, ButtonIndex))
	{
		CancelDiscardAction();
		Gui->ClosePopup();
	}
	Acceptance.ObserveWidget(EEditorWidget::CancelChanges, Gui->LastItemBounds());
	Gui->EndModal();
}

void FEditorPlugin::SaveBeforeRootSwitch()
{
	try
	{
		Transition.BeginSave(EEditorTransitionTarget::Root);
		AssetWorkspace->SaveAll();
		if (!IsDirty())
		{
			Transition.SaveAdmitted(EEditorTransitionTarget::Root, false);
			Gui->ClosePopup();
		}
		else if (CurrentPath.empty())
		{
			SavePath = std::string(GameContentRoot) + "/Scenes/Untitled.hasset";
			bSaveDialog = bRequestSaveDialog = true;
			Transition.AwaitSavePath(EEditorTransitionTarget::Root);
			Gui->ClosePopup();
		}
		else
		{
			SaveScene(CurrentPath);
			Transition.SaveAdmitted(EEditorTransitionTarget::Root, true);
		}
	}
	catch (const std::exception& Failure)
	{
		Error = Failure.what();
		Transition.SaveRejected(EEditorTransitionTarget::Root, Error);
	}
}

void FEditorPlugin::ConfirmDiscardAction()
{
	const auto Action = Transition.ConfirmDiscard();
	if (Action.Target == EEditorTransitionTarget::Document || Action.Target == EEditorTransitionTarget::Open)
	{
		ResetDocument();
	}
	Gui->ClosePopup();
	switch (Action.Target)
	{
		case EEditorTransitionTarget::Close:
			DiscardBeforeClose();
			break;
		case EEditorTransitionTarget::Root:
			break;
		case EEditorTransitionTarget::Open:
			OpenScene(Action.OpenPath);
			break;
		case EEditorTransitionTarget::Document:
			break;
	}
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
	Reparent.Reset(Gui);
	OutlinerSelection.Reset();
	OutlinerRows.clear();
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
	SaveStatus = Error;
	if (Transition.SceneSaveFailed(Error))
	{
		Gui->ClosePopups();
		AssetMessage = Error;
		bAssetMessage = bRequestAssetMessage = true;
	}
}

void FEditorPlugin::DrawSaveDialog()
{
	const bool bWasSaveDialog = bSaveDialog;
	bool bSaveSubmitted{};
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
		Acceptance.ObserveWidget(EEditorWidget::SavePath, Gui->LastItemBounds());
		if (Gui->Button("Save", !PendingSave && Scene->GetStatus().bReady))
		{
			try
			{
				if (Transition.IsSavingClose())
				{
					StartSaveBeforeClose(SavePath);
				}
				else
				{
					SaveScene(SavePath);
					if (Transition.IsSavingRoot())
					{
						Transition.SaveAdmitted(EEditorTransitionTarget::Root, true);
					}
				}
				bSaveSubmitted = true;
				bSaveDialog = false;
				Gui->ClosePopup();
			}
			catch (const std::exception& Failure)
			{
				Error = Failure.what();
			}
		}
		Acceptance.ObserveWidget(EEditorWidget::SaveConfirm, Gui->LastItemBounds());
		Gui->SameLine();
		if (Gui->Button("Cancel"))
		{
			bSaveDialog = false;
			Gui->ClosePopup();
		}
		Gui->TextWrapped(Error);
		Gui->EndModal();
	}
	if (bWasSaveDialog && !bSaveDialog && !bSaveSubmitted)
	{
		Transition.CancelSaveDialog();
	}
}

void FEditorPlugin::SaveBeforeClose()
{
	try
	{
		if (IsDirty() && CurrentPath.empty())
		{
			Transition.AwaitSavePath(EEditorTransitionTarget::Close);
			SavePath = std::string(GameContentRoot) + "/Scenes/Untitled.hasset";
			bSaveDialog = bRequestSaveDialog = true;
			Gui->ClosePopup();
			return;
		}
		StartSaveBeforeClose(CurrentPath);
	}
	catch (const std::exception& Failure)
	{
		Error = Failure.what();
	}
}

FEditorSaveProgress FEditorPlugin::DocumentSaveProgress() const
{
	const auto* Imports = Context.Find<FAssetImportWorkspace>();
	return {.bSceneDirty = IsDirty(),
	        .bAssetsDirty = AssetWorkspace->IsDirty(),
	        .bImportDirty = Imports && Imports->ContentRootState().bDirty,
	        .bSceneSaving = PendingSave.has_value(),
	        .bAssetsSaving = AssetWorkspace->IsSaving(),
	        .bPendingAssetEdits = AssetWorkspace->HasPendingEdits(),
	        .bSaveDialog = bSaveDialog};
}

void FEditorPlugin::PollSavedClose()
{
	if (Transition.IsSavingClose() && Transition.AdvanceCloseSave(DocumentSaveProgress()))
	{
		Window->RequestClose();
	}
}
} // namespace Hyperion
