#include "EditorApplication.h"

namespace Hyperion
{
void FEditorPlugin::InitializeSceneDocument()
{
	SceneDocument.Detach(Tasks);
	SceneTarget.reset();
	Scene = std::make_unique<FSceneInstance>(*Session, Tasks, Assets, true);
	SceneTarget = std::make_unique<FSceneInstanceEditTarget>(*Scene, Assets);
	SceneDocument.Attach(*SceneTarget);
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
	const bool bBusy = GizmoEdit.has_value() || InspectorInteraction || PendingInspectorEdit || Placement.IsActive() ||
	                   Gui->DragPayload() || Gui->IsEditingText() || bOpenDialog || bSaveDialog || bDiscardDialog ||
	                   bAssetMessage || PendingRoot || bPreferencesDialog || bFinished;
	SceneDocument.SetInteractionState(bBusy, bPreviewDirty);
}
} // namespace Hyperion
