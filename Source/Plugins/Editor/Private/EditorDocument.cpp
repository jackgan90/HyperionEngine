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
	Selection = std::move(InSelection);
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
	if (!IsDirty() && !PendingSave)
	{
		return true;
	}
	Window->CancelClose();
	bPendingClose = true;
	bDiscardDialog = bRequestDiscard = true;
	return false;
}

void FEditorPlugin::CancelDiscardAction()
{
	bDiscardDialog = bPendingClose = false;
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
	Gui->TextWrapped(PendingSave ? "Wait for the current save to finish."
	                 : PendingRoot
	                     ? "Save changes to the current asset root before switching, discard them, or cancel."
	                     : "The scene has unsaved changes. Discard them to continue, or cancel to review and save.");
	if (PendingRoot && Gui->Button("Save and switch", !PendingSave && !bSaveThenSwitch))
	{
		try
		{
			bSaveThenSwitch = true;
			if (CurrentPath.empty())
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
	if (Gui->Button("Discard changes", !PendingSave))
	{
		if (!PendingRoot || bPendingClose)
		{
			ResetDocument();
		}
		bDiscardDialog = false;
		Gui->ClosePopup();
		if (bPendingClose)
		{
			Window->RequestClose();
		}
		else if (PendingRoot)
		{
			bCommitRoot = true;
		}
		else if (!PendingOpen.empty())
		{
			const auto Path = std::exchange(PendingOpen, {});
			OpenScene(Path);
		}
		bPendingClose = false;
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
	return DocumentState != SavedState;
}

void FEditorPlugin::ResetDocument()
{
	SetPreviewCamera({});
	History.clear();
	HistoryCursor = 0;
	DocumentState = SavedState = ++NextDocumentState;
	++DocumentEpoch;
	FinishInspectorEdit();
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
	if (!HistoryCursor)
	{
		return;
	}
	RestoreHistory(HistoryCursor - 1, false);
	--HistoryCursor;
	DocumentState = History[HistoryCursor].BeforeState;
}

void FEditorPlugin::Redo()
{
	FinishInspectorEdit();
	Error.clear();
	if (HistoryCursor == History.size())
	{
		return;
	}
	RestoreHistory(HistoryCursor, true);
	DocumentState = History[HistoryCursor].AfterState;
	++HistoryCursor;
}

void FEditorPlugin::SaveScene(const std::string& InDestination)
{
	FinishInspectorEdit();
	if (PendingSave)
	{
		throw std::runtime_error("A scene save is already in progress");
	}
	if (InDestination.empty() || std::filesystem::path(InDestination).extension() != ".hasset")
	{
		throw std::invalid_argument("Choose a .hasset scene destination");
	}
	const auto Started = ClockNanoseconds();
	auto Snapshot = std::make_shared<const FSceneManifest>(Scene->Snapshot(InDestination));
	// Reject unsupported component state before admitting IO.
	WriteRecord(RecordType<FSceneManifest>(), Snapshot.get());
	auto Result = Assets.SaveAsync(InDestination, std::move(Snapshot));
	PendingSave = FPendingSave{std::move(Result), InDestination, DocumentEpoch, DocumentState, Started};
	SaveStatus = "Saving scene...";
}

void FEditorPlugin::PollSave()
{
	if (!PendingSave || !PendingSave->Result.Ready())
	{
		return;
	}
	const bool bCurrentDocument = PendingSave->Epoch == DocumentEpoch;
	try
	{
		if (!*PendingSave->Result.GetReady())
		{
			throw std::runtime_error("Asset service did not commit the scene");
		}
		if (bCurrentDocument)
		{
			SavedState = PendingSave->State;
			CurrentPath = PendingSave->Destination;
			LastSaveMilliseconds = double(ClockNanoseconds() - PendingSave->Started) / 1e6;
			SaveStatus = "Saved: " + CurrentPath;
			RefreshContent();
			if (PendingRoot)
			{
				bSaveThenSwitch = false;
				bCommitRoot = !IsDirty();
				bDiscardDialog = !bCommitRoot;
				bRequestDiscard = bDiscardDialog;
			}
		}
	}
	catch (const std::exception& Failure)
	{
		if (bCurrentDocument)
		{
			Error = "Save failed: " + std::string(Failure.what());
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
	}
	PendingSave.reset();
}

void FEditorPlugin::DrawSaveDialog()
{
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
				SaveScene(SavePath);
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
	if (!bSaveDialog && bSaveThenSwitch && !PendingSave)
	{
		bSaveThenSwitch = false;
		PendingRoot.reset();
	}
}
} // namespace Hyperion
