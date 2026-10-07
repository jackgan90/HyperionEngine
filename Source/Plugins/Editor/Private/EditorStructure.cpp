#include "EditorApplication.h"

namespace Hyperion
{
void FEditorPlugin::CancelReparentGesture()
{
	Reparent.Cancel(Gui);
}

void FEditorPlugin::BeginReparentDrag()
{
	ViewportClick.reset();
	Camera.Reset();
	Viewport.bCameraDragging = false;
}

void FEditorPlugin::UpdateReparentGesture(std::span<const FInputEvent> InEvents)
{
	Reparent.Update(*Gui, SceneDocument, *this, InEvents, CaptureInteractionPolicy().AllowsReparentContinue(),
	                bShowOutliner);
}

void FEditorPlugin::RouteReparentRow(FSceneHandle InHandle, bool bInActivated)
{
	const auto Feedback = Reparent.RouteRow(*Gui, SceneDocument, *this, InHandle, bInActivated,
	                                        CaptureInteractionPolicy().AllowsReparentBegin());
	if (Feedback.Error)
	{
		Error = *Feedback.Error;
	}
}

void FEditorPlugin::RouteReparentRoot()
{
	const auto Feedback = Reparent.RouteRoot(*Gui, SceneDocument);
	if (Feedback.RootBounds)
	{
		Acceptance.ObserveWidget(EEditorWidget::HierarchyRoot, *Feedback.RootBounds);
	}
	if (Feedback.Error)
	{
		Error = *Feedback.Error;
	}
}

void FEditorPlugin::FinishReparentGesture()
{
	if (const auto Result = Reparent.Finish(*Gui, SceneDocument, *this))
	{
		Error = *Result;
	}
}
} // namespace Hyperion
