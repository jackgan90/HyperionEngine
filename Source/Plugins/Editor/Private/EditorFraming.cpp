#include "EditorApplication.h"
#include <algorithm>

namespace Hyperion
{
void FEditorPlugin::RouteFrameSelectionShortcut(std::span<const FInputEvent> InEvents)
{
	if (!Viewport.bViewportVisible || !Viewport.bViewportCameraInitialized || Viewport.PreviewCamera ||
	    !CaptureShortcutInteraction(InEvents).Allows(EEditorShortcut::FrameSelection))
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
	if (!Viewport.bViewportVisible)
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
