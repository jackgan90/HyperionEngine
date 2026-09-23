#include "EditorApplication.h"
#include "Hyperion/Renderer/SceneNavigation.h"

namespace Hyperion
{
FSceneComponentDiagnostics FEditorPlugin::ComponentDiagnostics(FSceneHandle InHandle, std::string_view InComponent)
{
	if (!Scene->FindNode(InHandle))
	{
		throw FSceneEditError("stale_handle", "Object is no longer in this scene");
	}
	const auto Value = Scene->GetComponentDiagnostics(InHandle, InComponent);
	if (!Value)
	{
		throw FSceneEditError("not_found", "Component has no published rendering diagnostics");
	}
	return *Value;
}

FRenderDiagnostics FEditorPlugin::RenderDiagnostics()
{
	FRenderDiagnostics Result;
	Result.Frame = FrameCount;
	Result.bReady = Scene->GetStatus().bReady;
	Result.SceneError = ScenePreparationError(*Scene);
	Result.Pipeline = RenderStats;
	FDeviceStats Snapshot;
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          Snapshot = Device->Statistics();
	                          }));
	SetDeviceDiagnostics(Result, Snapshot);
	return Result;
}

FSceneViewportState FEditorPlugin::ViewportState() const
{
	FSceneViewportOptions ViewOptions;
	ViewOptions.Exposure = Exposure;
	ViewOptions.LightMarkers = bShowLightMarkers;
	ViewOptions.OutlineMode = static_cast<std::uint32_t>(OutlineSettings.Overlap);
	ViewOptions.SmoothOutlines = OutlineSettings.bSupersample;
	return {ViewCamera, PreviewCamera, ViewOptions, Camera.GetMovementSpeed(ViewCamera), bViewportCameraInitialized,
	        true};
}

void FEditorPlugin::SetViewportCamera(const FSceneCameraView& InCamera)
{
	ValidateSceneCameraView(InCamera);
	SetPreviewCamera({});
	ViewCamera = InCamera;
	bViewportCameraInitialized = true;
}

void FEditorPlugin::FrameScene()
{
	if (!Scene->GetStatus().bReady || PreviewCamera)
	{
		throw FSceneEditError("busy", "Frame scene requires a ready scene and the editor browsing view");
	}
	FitSceneCamera(ViewCamera, *Scene, ViewportSize.Height ? float(ViewportSize.Width) / ViewportSize.Height : 1);
	Camera.Reset();
}

void FEditorPlugin::SetViewportOptions(const FSceneViewportOptions& InOptions)
{
	ValidateViewportOptions(InOptions, ViewportState().Options);
	Exposure = InOptions.Exposure.value_or(Exposure);
	bShowLightMarkers = InOptions.LightMarkers.value_or(bShowLightMarkers);
	OutlineSettings.Overlap = static_cast<EOutlineOverlapMode>(
	    InOptions.OutlineMode.value_or(static_cast<std::uint32_t>(OutlineSettings.Overlap)));
	OutlineSettings.bSupersample = InOptions.SmoothOutlines.value_or(OutlineSettings.bSupersample);
}

void FEditorPlugin::SetViewportSpeed(float InSpeed)
{
	Camera.SetMovementSpeed(InSpeed);
}

void FEditorPlugin::PreviewSceneCamera(std::optional<FSceneHandle> InHandle)
{
	if (InHandle)
	{
		FSceneNodeView View;
		if (!Scene->GetNodeView(*InHandle, View) || !View.bEffectiveEnabled || !View.Node->Camera())
		{
			throw FSceneEditError("invalid_arguments", "Preview requires an enabled camera in the current scene");
		}
	}
	SetPreviewCamera(InHandle);
}

void FEditorPlugin::SaveInitialView()
{
	SetInitialView();
}

void FEditorPlugin::CreateViewCamera()
{
	CreateCameraFromView();
}

void FEditorPlugin::ApplyViewToCamera(FSceneHandle InHandle)
{
	ApplyEditorView(InHandle);
}
} // namespace Hyperion
