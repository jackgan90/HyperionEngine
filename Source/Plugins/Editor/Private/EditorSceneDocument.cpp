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
	if (Window->SupportsTypedClipboard())
	{
		SceneDocument.SetClipboardProvider({[this]
		                                    {
			                                    return Window->TypedClipboard(SceneClipboardFormat);
		                                    },
		                                    [this](const std::string& InToken, const std::string& InText)
		                                    {
			                                    Window->SetTypedClipboard(SceneClipboardFormat, InToken, InText);
		                                    }});
	}
	SceneDocument.SetSelectionObserver(
	    [this]
	    {
		    bSelectionInitialized = true;
		    ViewportClick.reset();
		    OutlinerSelection.Reset();
	    });
	SceneDocument.SetHistoryObserver(
	    [this](const FSceneHandleMap& InMapping)
	    {
		    RemapSceneHandle(Viewport.PreviewCamera, InMapping);
		    ViewportClick.reset();
		    OutlinerSelection.Reset();
		    if (Viewport.PreviewCamera && !Scene->FindNode(*Viewport.PreviewCamera))
		    {
			    SetPreviewCamera(std::nullopt);
		    }
	    });
}

bool FEditorPlugin::IsDocumentInteractionBusy() const
{
	return !Options.Benchmark.empty() || GizmoEdit.has_value() || InspectorInteraction || PendingInspectorEdit ||
	       ReparentGesture.has_value() || Placement.IsActive() || Gui->DragPayload() || Gui->IsEditingText() ||
	       bOpenDialog || bSaveDialog || Transition.bDiscardDialog || bAssetMessage || Transition.PendingRoot ||
	       bPreferencesDialog || bFinished;
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
