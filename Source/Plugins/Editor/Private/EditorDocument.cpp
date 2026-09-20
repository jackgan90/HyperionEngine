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

void FEditorPlugin::DrawDiscardDialog()
{
	if (bRequestDiscard)
	{
		Gui->OpenPopup("Unsaved changes");
		bRequestDiscard = false;
	}
	if (!bDiscardDialog || !Gui->BeginModal("Unsaved changes", bDiscardDialog))
	{
		return;
	}
	Gui->TextWrapped(PendingSave ? "Wait for the current save to finish."
	                             : "The scene has unsaved changes. Discard them to continue, or "
	                               "cancel to review and save.");
	if (Gui->Button("Discard changes", !PendingSave))
	{
		ResetDocument();
		bDiscardDialog = false;
		Gui->ClosePopup();
		if (bPendingClose)
		{
			Window->RequestClose();
		}
		else if (!PendingOpen.empty())
		{
			const auto Path = std::exchange(PendingOpen, {});
			OpenScene(Path);
		}
		bPendingClose = false;
	}
	Gui->SameLine();
	if (Gui->Button("Cancel"))
	{
		bDiscardDialog = bPendingClose = false;
		PendingOpen.clear();
		Gui->ClosePopup();
	}
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
		}
	}
	catch (const std::exception& Failure)
	{
		if (bCurrentDocument)
		{
			Error = "Save failed: " + std::string(Failure.what());
			SaveStatus = Error;
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
		Gui->InputText("Scene path", SavePath);
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
			Gui->ClosePopup();
		}
		Gui->TextWrapped(Error);
		Gui->EndModal();
	}
}
} // namespace Hyperion
