#include "EditorApplication.h"
#include "Hyperion/Core/Core.h"

namespace Hyperion
{
bool FEditorApplication::HasDrafts() const
{
	for (const auto& [Id, Draft] : InspectorDrafts)
	{
		if (Draft.bModified)
		{
			return true;
		}
	}
	return false;
}

void FEditorApplication::SelectObject(FSceneHandle InHandle)
{
	if (Selection != InHandle && HasDrafts())
	{
		Error = "Apply or revert component drafts before changing selection";
		return;
	}
	Selection = InHandle;
}

bool FEditorApplication::PollClose()
{
	if (!Window->ShouldClose())
	{
		return false;
	}
	if (!IsDirty() && !HasDrafts() && !PendingSave)
	{
		return true;
	}
	Window->CancelClose();
	bPendingClose = true;
	bDiscardDialog = bRequestDiscard = true;
	return false;
}

void FEditorApplication::DrawDiscardDialog()
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
	                             : "The scene or component drafts have unsaved changes. Discard them to continue, or "
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

bool FEditorApplication::IsDirty() const
{
	return DocumentState != SavedState;
}

void FEditorApplication::ResetDocument()
{
	SetPreviewCamera({});
	History.clear();
	HistoryCursor = 0;
	DocumentState = SavedState = ++NextDocumentState;
	++DocumentEpoch;
	InspectorDrafts.clear();
	InspectedObject.reset();
	SaveStatus.clear();
}

void FEditorApplication::CommitEdit(FSceneHandle InHandle, FSceneNode InCandidate, std::uint64_t InExpectedRevision)
{
	const auto* Before = Scene->FindNode(InHandle);
	if (!Before || Scene->GetRevision() != InExpectedRevision)
	{
		throw std::runtime_error("The object changed while editing. Revert the draft and try again.");
	}
	if (*Before == InCandidate)
	{
		return;
	}
	FHistoryEntry Entry{InHandle, *Before, InCandidate, Scene->GetSettings(), {}, DocumentState, ++NextDocumentState};
	History.reserve(HistoryCursor + 1);
	if (!Scene->EditNode(InHandle, std::move(InCandidate), InExpectedRevision))
	{
		throw std::runtime_error("The edit target is no longer current");
	}
	Entry.AfterSettings = Scene->GetSettings();
	History.resize(HistoryCursor);
	DocumentState = Entry.AfterState;
	History.push_back(std::move(Entry));
	++HistoryCursor;
	Error.clear();
}

void FEditorApplication::Undo()
{
	if (HasDrafts())
	{
		throw std::runtime_error("Apply or revert component drafts before undoing");
	}
	if (!HistoryCursor)
	{
		return;
	}
	RestoreHistory(HistoryCursor - 1, false);
	--HistoryCursor;
	DocumentState = History[HistoryCursor].BeforeState;
	InspectorDrafts.clear();
}

void FEditorApplication::Redo()
{
	if (HasDrafts())
	{
		throw std::runtime_error("Apply or revert component drafts before redoing");
	}
	if (HistoryCursor == History.size())
	{
		return;
	}
	RestoreHistory(HistoryCursor, true);
	DocumentState = History[HistoryCursor].AfterState;
	++HistoryCursor;
	InspectorDrafts.clear();
}

void FEditorApplication::SaveScene(const std::string& InDestination)
{
	if (HasDrafts())
	{
		throw std::runtime_error("Apply or revert component drafts before saving");
	}
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

void FEditorApplication::PollSave()
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

void FEditorApplication::DrawSaveDialog()
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
