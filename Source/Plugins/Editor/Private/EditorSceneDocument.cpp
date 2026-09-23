#include "EditorApplication.h"

namespace Hyperion
{
void FEditorPlugin::OpenDocument(const FSceneOpenRequest& InRequest)
{
	UpdateDocumentInteraction();
	const auto Current = DescribeSceneDocument(SceneDocument);
	if (InRequest.Document != Current.Document)
	{
		throw FSceneEditError("stale_document", "Query the current document before opening a scene");
	}
	if (InRequest.Revision != Current.Revision)
	{
		throw FSceneEditError("stale_revision", "Scene changed before document replacement");
	}
	if (Current.bBusy || Current.bSaving || (Current.bLoaded && SceneTarget->IsPreparing()))
	{
		throw FSceneEditError("busy", "Wait for the active interaction, save or scene preparation");
	}
	LoadSceneDocument(InRequest.Path, InRequest.bDiscard);
}

FSceneHostStatus FEditorPlugin::DocumentStatus() const
{
	return {DescribeSceneDocument(SceneDocument), ScenePreparationError(*Scene)};
}

bool FEditorPlugin::SupportsContentTransitions() const
{
	return true;
}

FSceneHostStatus FEditorPlugin::PollDocument()
{
	Scene->Tick();
	return DocumentStatus();
}

void FEditorPlugin::InitializeSceneDocument()
{
	SceneDocument.Detach(Tasks);
	SceneTarget.reset();
	Scene = std::make_unique<FSceneInstance>(*Session, Tasks, Assets, true);
	SceneTarget = std::make_unique<FSceneInstanceEditTarget>(*Scene, Assets);
	SceneDocument.Attach(*SceneTarget);
	SceneDocument.SetSelectionObserver(
	    [this]
	    {
		    bSelectionInitialized = true;
		    ViewportClick.reset();
	    });
	SceneDocument.SetHistoryObserver(
	    [this](const FSceneHandleMap& InMapping)
	    {
		    RemapSceneHandle(PreviewCamera, InMapping);
		    ViewportClick.reset();
		    if (PreviewCamera && !Scene->FindNode(*PreviewCamera))
		    {
			    SetPreviewCamera(std::nullopt);
		    }
	    });
}

bool FEditorPlugin::IsDocumentInteractionBusy() const
{
	return GizmoEdit.has_value() || InspectorInteraction || PendingInspectorEdit || Placement.IsActive() ||
	       Gui->DragPayload() || Gui->IsEditingText() || bOpenDialog || bSaveDialog || bDiscardDialog ||
	       bAssetMessage || PendingRoot || bPreferencesDialog || bFinished;
}

void FEditorPlugin::UpdateDocumentInteraction()
{
	bool bPreviewDirty{};
	if (GizmoEdit)
	{
		for (const auto& Target : GizmoEdit->Targets)
		{
			const auto* Node = Scene->FindNode(Target.Handle);
			bPreviewDirty |= Node && Node->Local().Values != Target.Initial.Values;
		}
	}
	SceneDocument.SetInteractionState(IsDocumentInteractionBusy(), bPreviewDirty);
}
} // namespace Hyperion
