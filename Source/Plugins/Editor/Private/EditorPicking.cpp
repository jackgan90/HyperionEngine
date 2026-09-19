#include "EditorApplication.h"
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/ViewportRay.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
std::optional<FSceneCameraView> FEditorPlugin::PickingCamera() const
{
	if (PreviewCamera)
	{
		FSceneNodeView View;
		if (!Scene->GetNodeView(*PreviewCamera, View) || !View.bEffectiveEnabled || !View.Node->Camera())
		{
			return {};
		}
		return FSceneCameraView{*View.Node->Camera(), View.World};
	}
	// Render uses this value even before automatic framing finishes loading the scene.
	return ViewCamera;
}

void FEditorPlugin::RouteViewportPicking(std::span<const FInputEvent> InEvents)
{
	const bool bInputInterrupted =
	    std::any_of(InEvents.begin(), InEvents.end(),
	                [](const FInputEvent& InEvent)
	                {
		                return (InEvent.Type == EEventType::Focus && !InEvent.bDown) ||
		                       (InEvent.Type == EEventType::MouseButton && InEvent.Button == 1 && InEvent.bDown);
	                });
	const auto Pointer = Gui->PointerState();
	const auto ActiveCamera = PickingCamera();
	if (bInputInterrupted || !ActiveCamera || !bViewportVisible || !ViewportRegion.bFocused ||
	    !ViewportRegion.bHovered || bOpenDialog || bSaveDialog || bDiscardDialog || Gui->IsEditingText() ||
	    !Pointer.bPositionValid || Pointer.bCancel || Pointer.bRightDown || bCameraDragging || bGizmoUsedMouse ||
	    Gizmo.IsDragging())
	{
		ViewportClick.reset();
		return;
	}
	const auto& Bounds = ViewportRegion.Bounds;
	if (Bounds.Z <= Bounds.X || Bounds.W <= Bounds.Y)
	{
		ViewportClick.reset();
		return;
	}
	if (Pointer.bPressed)
	{
		ViewportClick = FViewportClick{Pointer.Position, Bounds, ViewportSize, *ActiveCamera, Scene->GetRevision()};
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
	    Click.Bounds.W != Bounds.W || Click.Size.Width != ViewportSize.Width ||
	    Click.Size.Height != ViewportSize.Height || Click.Camera.World.Values != ActiveCamera->World.Values ||
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
	ViewportClick.reset();
	const FVec2 Position{(Pointer.Position.X - Bounds.X) / (Bounds.Z - Bounds.X),
	                     (Pointer.Position.Y - Bounds.Y) / (Bounds.W - Bounds.Y)};
	const auto Ray =
	    MakeViewportRay(*ActiveCamera, Position, ViewportSize.Width, ViewportSize.Height, EDepthConvention::Reversed);
	if (!Ray)
	{
		return;
	}
	HYP_PERF_SCOPE_C(Frame, PickViewportModel);
	const auto QueryOptions = MakeSceneRayOptions(ESceneRenderPipeline::Deferred);
	const auto Hit = Scene->Raycast(*Ray, QueryOptions);
	if (Hit.Status == ESceneRayStatus::Hit)
	{
		SelectObject(Hit.Handle);
	}
	else if (Hit.Status == ESceneRayStatus::Miss)
	{
		SelectObject(std::nullopt);
	}
}
} // namespace Hyperion
