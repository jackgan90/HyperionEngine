#include "EditorApplication.h"
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/ViewportRay.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
std::optional<FSceneCameraView> FEditorPlugin::PickingCamera() const
{
	if (Viewport.PreviewCamera)
	{
		FSceneNodeView View;
		if (!Scene->GetNodeView(*Viewport.PreviewCamera, View) || !View.bEffectiveEnabled || !View.Node->Camera())
		{
			return {};
		}
		return FSceneCameraView{*View.Node->Camera(), View.World};
	}
	// Render uses this value even before automatic framing finishes loading the scene.
	return Viewport.ViewCamera;
}

void FEditorPlugin::RouteViewportPicking(std::span<const FInputEvent> InEvents)
{
	const bool bInputInterrupted = std::any_of(InEvents.begin(), InEvents.end(),
	                                           [](const FInputEvent& InEvent)
	                                           {
		                                           return (InEvent.Type == EEventType::Focus && !InEvent.bDown) ||
		                                                  (InEvent.Type == EEventType::MouseButton &&
		                                                   InEvent.Button == InputButtons::Right && InEvent.bDown);
	                                           });
	const auto Pointer = Gui->PointerState();
	const auto ActiveCamera = PickingCamera();
	if (Gui->DragPayload() || Placement.IsActive() || bPlacementUsedMouse || bInputInterrupted || !ActiveCamera ||
	    !Viewport.bViewportVisible || !Viewport.ViewportRegion.bFocused || !Viewport.ViewportRegion.bHovered ||
	    bOpenDialog || bSaveDialog || bAssetMessage || Transition.PendingRoot || Transition.bDiscardDialog ||
	    bPreferencesDialog || Gui->IsEditingText() || !Pointer.bPositionValid || Pointer.bCancel ||
	    Pointer.bRightDown || Viewport.bCameraDragging || bGizmoUsedMouse || Gizmo.IsDragging())
	{
		ViewportClick.reset();
		return;
	}
	const auto& Bounds = Viewport.ViewportRegion.Bounds;
	if (Bounds.Z <= Bounds.X || Bounds.W <= Bounds.Y)
	{
		ViewportClick.reset();
		return;
	}
	if (Pointer.bPressed)
	{
		ViewportClick = FViewportClick{Pointer.Position, Pointer.bCtrl || Pointer.bShift,
		                               Bounds,           Viewport.ViewportSize,
		                               *ActiveCamera,    Scene->GetRevision()};
	}
	if (!ViewportClick)
	{
		return;
	}
	const auto& Click = *ViewportClick;
	const float DeltaX = Pointer.Position.X - Click.Start.X;
	const float DeltaY = Pointer.Position.Y - Click.Start.Y;
	if (DeltaX * DeltaX + DeltaY * DeltaY > 16 || Click.Revision != Scene->GetRevision() ||
	    Click.Bounds.X != Bounds.X || Click.Bounds.Y != Bounds.Y || Click.Bounds.Z != Bounds.Z ||
	    Click.Bounds.W != Bounds.W || Click.Size.Width != Viewport.ViewportSize.Width ||
	    Click.Size.Height != Viewport.ViewportSize.Height || Click.Camera.World.Values != ActiveCamera->World.Values ||
	    Click.Camera.Lens != ActiveCamera->Lens)
	{
		ViewportClick.reset();
		return;
	}
	if (!Pointer.bReleased)
	{
		if (!Pointer.bDown)
		{
			ViewportClick.reset();
		}
		return;
	}
	const bool bToggle = Click.bToggle;
	ViewportClick.reset();
	if (const auto Light = PickLightMarker(Pointer.Position))
	{
		ClickObject(Light, bToggle);
		return;
	}
	const FVec2 Position{(Pointer.Position.X - Bounds.X) / (Bounds.Z - Bounds.X),
	                     (Pointer.Position.Y - Bounds.Y) / (Bounds.W - Bounds.Y)};
	const auto Ray = MakeViewportRay(*ActiveCamera, Position, Viewport.ViewportSize.Width, Viewport.ViewportSize.Height,
	                                 GetDepthConvention(Rendering.bReversedZ));
	if (!Ray)
	{
		return;
	}
	HYP_PERF_SCOPE_C(Frame, PickViewportModel);
	const auto QueryOptions = MakeSceneRayOptions(MakePipelineSettings(Rendering).Pipeline);
	const auto Hit = Scene->Raycast(*Ray, QueryOptions);
	if (Hit.Status == ESceneRayStatus::Hit)
	{
		ClickObject(Hit.Handle, bToggle);
	}
	else if (Hit.Status == ESceneRayStatus::Miss)
	{
		ClickObject(std::nullopt, bToggle);
	}
}
} // namespace Hyperion
