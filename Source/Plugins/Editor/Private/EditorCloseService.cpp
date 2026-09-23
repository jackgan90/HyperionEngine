#include "EditorApplication.h"

namespace Hyperion
{
FApplicationCloseState FEditorPlugin::ApplicationCloseState() const
{
	return {CloseState, CloseError, IsDirty() || AssetWorkspace->IsDirty(),
	        bool(PendingSave) || AssetWorkspace->IsSaving() || AssetWorkspace->HasPendingEdits() ||
	            GizmoEdit.has_value() || InspectorInteraction || PendingInspectorEdit.has_value() ||
	            Placement.IsActive(),
	        CurrentPath};
}

void FEditorPlugin::StartSaveBeforeClose(const std::string& InPath)
{
	if (IsDirty() && InPath.empty())
	{
		throw std::invalid_argument("Supply scenePath to save an untitled scene before closing");
	}
	try
	{
		CloseError.clear();
		CloseState = "saving";
		bPendingClose = true;
		bSaveThenClose = true;
		AssetWorkspace->SaveAll();
		if (IsDirty())
		{
			SaveScene(InPath);
		}
		bDiscardDialog = false;
		Gui->ClosePopups();
	}
	catch (const std::exception& Failure)
	{
		bSaveThenClose = false;
		CloseState = "failed";
		CloseError = Failure.what();
		throw;
	}
}

void FEditorPlugin::DiscardBeforeClose()
{
	ResetDocument();
	AssetWorkspace->CloseAll();
	bDiscardDialog = bRequestDiscard = bPendingClose = bSaveThenClose = false;
	Gui->ClosePopups();
	CloseError.clear();
	CloseState = "closing";
	Window->RequestClose();
}

FApplicationCloseState FEditorPlugin::RequestApplicationClose(const FApplicationCloseRequest& InRequest)
{
	if (InRequest.Action == EApplicationCloseAction::Cancel)
	{
		if (CloseState == "idle" && !bPendingClose && !bSaveThenClose && !Window->ShouldClose())
		{
			return ApplicationCloseState();
		}
		if (bPendingClose || bSaveThenClose)
		{
			bSaveDialog = bRequestSaveDialog = false;
		}
		CancelDiscardAction();
		Window->CancelClose();
		Gui->ClosePopups();
		CloseState = "idle";
		CloseError.clear();
		return ApplicationCloseState();
	}
	const auto State = ApplicationCloseState();
	if (bFinished || State.bBusy || bOpenDialog || bSaveDialog || PendingRoot || bPreferencesDialog)
	{
		throw FSceneEditError("busy", "Finish active edits, transitions and saves before closing");
	}
	switch (InRequest.Action)
	{
		case EApplicationCloseAction::Save:
			StartSaveBeforeClose(InRequest.ScenePath.empty() ? CurrentPath : InRequest.ScenePath);
			break;
		case EApplicationCloseAction::Discard:
			DiscardBeforeClose();
			break;
		case EApplicationCloseAction::RejectDirty:
			if (State.bDirty)
			{
				throw FSceneEditError("dirty_document", "Save first or explicitly choose save/discard close action");
			}
			CloseError.clear();
			CloseState = "closing";
			Window->RequestClose();
			break;
		default:
			throw std::invalid_argument("Invalid close action");
	}
	return ApplicationCloseState();
}
} // namespace Hyperion
