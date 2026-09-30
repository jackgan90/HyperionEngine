#include "EditorApplication.h"
#include <algorithm>

namespace Hyperion
{
void FEditorPlugin::RouteFrameSelectionShortcut(std::span<const FInputEvent> InEvents)
{
	const bool bSceneFocus =
	    Gui->IsWindowFocused("Viewport") || Gui->IsWindowFocused("Outliner") || Gui->IsWindowFocused("Details");
	const bool bInterrupted = std::any_of(InEvents.begin(), InEvents.end(),
	                                      [](const FInputEvent& InEvent)
	                                      {
		                                      return InEvent.Type == EEventType::Focus && !InEvent.bDown;
	                                      });
	if (!bSceneFocus || bInterrupted || !bViewportVisible || !bViewportCameraInitialized || PreviewCamera ||
	    Gui->IsTextInputOwnedThisFrame() || Gui->HasOpenPopup() || ReparentGesture || Gizmo.IsDragging() ||
	    bGizmoUsedMouse || bPlacementUsedMouse || bCameraDragging || Gui->PointerState().bRightDown ||
	    Gui->PointerState().bCancel || IsDocumentInteractionBusy() || IsAssetWindowBlocked())
	{
		return;
	}
	const bool bFrame = std::any_of(InEvents.begin(), InEvents.end(),
	                                [](const FInputEvent& InEvent)
	                                {
		                                return InEvent.Type == EEventType::Key && InEvent.Key == EKey::F &&
		                                       InEvent.bDown && !InEvent.bRepeat && !InEvent.Modifiers;
	                                });
	if (bFrame)
	{
		try
		{
			FrameSelection({SceneDocument.Id(), Scene->GetRevision()});
			Error.clear();
		}
		catch (const std::exception& Failure)
		{
			Error = Failure.what();
		}
	}
}

void FEditorPlugin::DrawViewportOverlays()
{
	if (!bViewportVisible)
	{
		return;
	}
	// Append to the viewport after scene-panel input so overlays match the final camera and selection.
	if (Gui->BeginWindow("Viewport", bShowViewport))
	{
		DrawLightMarkers();
		DrawSelectionMarkers();
		DrawDebugBounds();
		DrawGizmoOverlay();
		DrawViewportHud();
	}
	Gui->EndWindow();
}
} // namespace Hyperion
