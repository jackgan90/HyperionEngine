#include "EditorApplication.h"
#include "Hyperion/SceneEditing/SceneAuthoring.h"

namespace Hyperion
{
namespace
{
bool HasVisibleGeometry(const FSceneModel* InModel)
{
	if (!InModel || !InModel->bVisible || !InModel->Data)
	{
		return false;
	}
	for (const auto& Instance : SceneModelInstances(*InModel))
	{
		const auto Id = ModelPrimitiveId(*InModel->Data->Asset, Instance.Primitive);
		const auto Section = std::find_if(InModel->Sections.begin(), InModel->Sections.end(),
		                                  [&](const auto& InSection)
		                                  {
			                                  return InSection.Primitive == Id;
		                                  });
		if (Section == InModel->Sections.end() || Section->bVisible)
		{
			return true;
		}
	}
	return false;
}
} // namespace

void FEditorPlugin::ClickOutlinerObject(FSceneHandle InHandle, bool bInToggle, bool bInRange)
{
	if (std::find(OutlinerRows.begin(), OutlinerRows.end(), InHandle) == OutlinerRows.end())
	{
		return;
	}
	try
	{
		auto PendingState = OutlinerSelection;
		const auto Updated = PendingState.Click(Selection, OutlinerRows, InHandle, bInRange, bInToggle);
		FinishInspectorEdit();
		UpdateDocumentInteraction();
		ApplySceneSelection(SceneDocument, {SceneDocument.Id(), Scene->GetRevision(), Updated.All()});
		// Selection observers invalidate external anchors; publish this gesture's anchor after admission.
		OutlinerSelection = std::move(PendingState);
		Error.clear();
	}
	catch (const std::exception& Failure)
	{
		Error = Failure.what();
	}
}

void FEditorPlugin::RouteSelectAllShortcut(std::span<const FInputEvent> InEvents)
{
	const bool bSceneFocus =
	    (bViewportVisible && Gui->IsWindowFocused("Viewport")) || (bShowOutliner && Gui->IsWindowFocused("Outliner"));
	// A later navigation release must not expose an earlier key from this event batch to selection.
	const bool bInterrupted = std::any_of(InEvents.begin(), InEvents.end(),
	                                      [](const FInputEvent& InEvent)
	                                      {
		                                      return (InEvent.Type == EEventType::Focus && !InEvent.bDown) ||
		                                             (InEvent.Type == EEventType::MouseButton && InEvent.Button == 1);
	                                      });
	const auto Pointer = Gui->PointerState();
	if (!bSceneFocus || bInterrupted || !Scene->GetStatus().bReady || Gui->IsTextInputOwnedThisFrame() ||
	    Gui->HasOpenPopup() || ReparentGesture || Gizmo.IsDragging() || bGizmoUsedMouse || bPlacementUsedMouse ||
	    bCameraDragging || Pointer.bRightDown || Pointer.bDown || Pointer.bCancel || IsDocumentInteractionBusy() ||
	    IsAssetWindowBlocked())
	{
		return;
	}
	const bool bSelectAll = std::any_of(InEvents.begin(), InEvents.end(),
	                                    [](const FInputEvent& InEvent)
	                                    {
		                                    return InEvent.Type == EEventType::Key && InEvent.Key == EKey::A &&
		                                           InEvent.bDown && !InEvent.bRepeat && InEvent.Modifiers == 1;
	                                    });
	if (bSelectAll)
	{
		try
		{
			FinishInspectorEdit();
			UpdateDocumentInteraction();
			SelectAllSceneNodes(SceneDocument, {SceneDocument.Id(), Scene->GetRevision()});
			Error.clear();
		}
		catch (const std::exception& Failure)
		{
			Error = Failure.what();
		}
	}
}

void FEditorPlugin::PruneSelection()
{
	auto Updated = Selection;
	for (const auto Handle : Selection.All())
	{
		if (!Scene->FindNode(Handle))
		{
			Updated.Toggle(Handle);
		}
	}
	if (Updated != Selection)
	{
		SetSelection(std::move(Updated));
	}
}

std::vector<FSceneHandle> FEditorPlugin::SelectedRoots() const
{
	return SceneDocument.SelectedRoots();
}

void FEditorPlugin::DrawSelectionMarkers()
{
	const auto ActiveCamera = PickingCamera();
	if (!ActiveCamera)
	{
		return;
	}
	for (const auto Handle : Selection.All())
	{
		FSceneNodeView View;
		if (!Scene->GetNodeView(Handle, View) || (View.bEffectiveEnabled && HasVisibleGeometry(Scene->Find(Handle))) ||
		    (bShowLightMarkers && LightTexture(*View.Node) && View.bEffectiveEnabled))
		{
			continue;
		}
		const FVec3 Origin{View.World.Values[12], View.World.Values[13], View.World.Values[14]};
		if (const auto Point = ProjectViewportPoint(*ActiveCamera, ViewportRegion.Bounds, Origin))
		{
			const float Radius = Gui->Scale(6);
			const std::array<FVec2, 5> Points{{{Point->X, Point->Y - Radius},
			                                   {Point->X + Radius, Point->Y},
			                                   {Point->X, Point->Y + Radius},
			                                   {Point->X - Radius, Point->Y},
			                                   {Point->X, Point->Y - Radius}}};
			Gui->DrawImageOverlay(ViewportRegion.Bounds, Points, {1, .7f, .1f, 1}, 2);
		}
	}
}
} // namespace Hyperion
