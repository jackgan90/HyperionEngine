#include "EditorApplication.h"

namespace Hyperion
{
FApplicationCloseState FEditorPlugin::ApplicationCloseState() const
{
	const auto* Imports = Context.Find<FAssetImportWorkspace>();
	const auto ImportState = Imports ? Imports->ContentRootState() : FContentRootParticipantState{};
	return {std::string(Transition.CloseStatus()), Transition.CloseError(),
	        IsDirty() || AssetWorkspace->IsDirty() || ImportState.bDirty,
	        bool(PendingSave) || AssetWorkspace->IsSaving() || AssetWorkspace->HasPendingEdits() ||
	            GizmoEdit.has_value() || InspectorInteraction || PendingInspectorEdit.has_value() ||
	            Placement.IsActive() || ImportState.bBusy,
	        CurrentPath};
}

void FEditorPlugin::StartSaveBeforeClose(const std::string& InPath)
{
	if (const auto* Imports = Context.Find<FAssetImportWorkspace>(); Imports && Imports->ContentRootState().bDirty)
	{
		throw FSceneEditError("dirty_document", "Publish or explicitly discard import drafts before Save and Close");
	}
	if (IsDirty() && InPath.empty())
	{
		throw std::invalid_argument("Supply scenePath to save an untitled scene before closing");
	}
	try
	{
		Transition.BeginSave(EEditorTransitionTarget::Close);
		AssetWorkspace->SaveAll();
		if (IsDirty())
		{
			SaveScene(InPath);
		}
		Transition.SaveAdmitted(EEditorTransitionTarget::Close, PendingSave.has_value());
		Gui->ClosePopups();
	}
	catch (const std::exception& Failure)
	{
		Transition.SaveRejected(EEditorTransitionTarget::Close, Failure.what());
		throw;
	}
}

void FEditorPlugin::DiscardBeforeClose()
{
	if (auto* Imports = Context.Find<FAssetImportWorkspace>())
	{
		for (const auto& Id : Imports->DraftList().Drafts)
		{
			const auto Draft = Imports->Draft({Id});
			if (Draft.Status != EImportDraftState::Preparing && Draft.Status != EImportDraftState::Publishing)
			{
				Imports->DiscardDraft({Id, Draft.Generation, true});
			}
		}
	}
	ResetDocument();
	AssetWorkspace->CloseAll();
	Gui->ClosePopups();
	Transition.CompleteClose();
	Window->RequestClose();
}

FApplicationCloseState FEditorPlugin::RequestApplicationClose(const FApplicationCloseRequest& InRequest)
{
	if (InRequest.Action == EApplicationCloseAction::Cancel)
	{
		if (!Transition.HasCloseRequest() && !Window->ShouldClose())
		{
			return ApplicationCloseState();
		}
		if (Transition.HasPendingClose())
		{
			bSaveDialog = bRequestSaveDialog = false;
		}
		CancelDiscardAction();
		Window->CancelClose();
		Gui->ClosePopups();
		return ApplicationCloseState();
	}
	const auto State = ApplicationCloseState();
	if (bFinished || State.bBusy || bOpenDialog || bSaveDialog || Transition.HasPendingRoot() || bPreferencesDialog)
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
			Transition.CompleteClose();
			Window->RequestClose();
			break;
		default:
			throw std::invalid_argument("Invalid close action");
	}
	return ApplicationCloseState();
}
} // namespace Hyperion
