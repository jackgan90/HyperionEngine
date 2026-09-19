#include "EditorApplication.h"
#include "Hyperion/Core/Core.h"

namespace Hyperion
{
void FEditorPlugin::SelectObject(std::optional<FSceneHandle> InHandle)
{
	if (Selection != InHandle)
	{
		FinishInspectorEdit();
	}
	Selection = InHandle;
	bSelectionInitialized = true;
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
	const auto* Node = GizmoEdit ? Scene->FindNode(GizmoEdit->Handle) : nullptr;
	return DocumentState != SavedState || (Node && Node->Local().Values != GizmoEdit->Initial.Values);
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
	if (!InInteraction)
	{
		FinishInspectorEdit();
	}
	const auto* Before = Scene->FindNode(InHandle);
	if (!Before || Scene->GetRevision() != InExpectedRevision)
	{
		throw std::runtime_error("The object changed while editing. Try again with its current values.");
	}
	if (!InInteraction && *Before == InCandidate)
	{
		return;
	}
	const bool bMerge = InInteraction && InspectorTransaction && InspectorTransaction->Interaction == InInteraction &&
	                    InspectorTransaction->Handle == InHandle &&
	                    InspectorTransaction->Revision == InExpectedRevision && HistoryCursor == History.size() &&
	                    InspectorTransaction->HistoryIndex + 1 == HistoryCursor;
	FHistoryEntry Entry{InHandle, *Before, InCandidate, Scene->GetSettings(), {}, DocumentState, ++NextDocumentState};
	History.reserve(HistoryCursor + 1);
	if (!Scene->EditNode(InHandle, std::move(InCandidate), InExpectedRevision))
	{
		throw std::runtime_error("The edit target is no longer current");
	}
	Entry.AfterSettings = Scene->GetSettings();
	DocumentState = Entry.AfterState;
	if (bMerge)
	{
		auto& Previous = History.back();
		Previous.After = std::move(Entry.After);
		Previous.AfterSettings = std::move(Entry.AfterSettings);
		Previous.AfterState = Entry.AfterState;
	}
	else
	{
		History.resize(HistoryCursor);
		History.push_back(std::move(Entry));
		++HistoryCursor;
	}
	if (InInteraction)
	{
		InspectorTransaction = FInspectorTransaction{InInteraction, InHandle, HistoryCursor - 1, Scene->GetRevision()};
	}
	Error.clear();
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
