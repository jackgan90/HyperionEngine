#include "EditorApplication.h"
#include "Hyperion/Renderer/DebugGeometry.h"

namespace Hyperion
{
void FEditorPlugin::DrawDebugBounds()
{
	const auto CameraView = PickingCamera();
	if ((!bModelBounds && !bLightBounds) || !CameraView || !ViewportSize.Height)
	{
		return;
	}
	std::vector<FSceneNodeView> Nodes;
	for (const auto Handle : Scene->GetNodes())
	{
		FSceneNodeView View;
		if (Scene->GetNodeView(Handle, View))
		{
			Nodes.push_back(View);
		}
	}
	const auto Matrix = SceneCameraViewProjection(ExtractScenePose(CameraView->World), CameraView->Lens,
	                                              float(ViewportSize.Width) / ViewportSize.Height,
	                                              GetDepthConvention(Rendering.bReversedZ));
	for (const auto& Line : BuildSceneDebugLines(Nodes, bModelBounds, bLightBounds))
	{
		if (const auto Points = ProjectDebugLine(Line, Matrix, ViewportRegion.Bounds))
		{
			Gui->DrawImageOverlay(ViewportRegion.Bounds, *Points, Line.Color, 1);
		}
	}
}
} // namespace Hyperion
