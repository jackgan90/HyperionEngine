#include "EditorApplication.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include "Hyperion/Renderer/ViewportRay.h"

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

FRenderHealth FEditorPlugin::RenderHealth()
{
	return {FrameCount, Scene->GetStatus().bReady, ScenePreparationError(*Scene)};
}

FRenderDiagnostics FEditorPlugin::RenderDiagnostics()
{
	const auto Health = RenderHealth();
	FRenderDiagnostics Result;
	Result.Frame = Health.Frame;
	Result.bReady = Health.bReady;
	Result.SceneError = Health.Error;
	Result.Pipeline = RenderStats;
	FDeviceStats Snapshot;
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          Snapshot = Device->Statistics();
	                          }));
	SetDeviceDiagnostics(Result, Snapshot);
	SetExecutionDiagnostics(Result, Tasks, FrameIntervalMilliseconds);
	return Result;
}

FSceneViewportState FEditorPlugin::ViewportState() const
{
	FSceneViewportOptions ViewOptions;
	ViewOptions.Exposure = Exposure;
	ViewOptions.LightMarkers = bShowLightMarkers;
	ViewOptions.OutlineMode = static_cast<std::uint32_t>(OutlineSettings.Overlap);
	ViewOptions.SmoothOutlines = OutlineSettings.bSupersample;
	ViewOptions.Culling = static_cast<std::uint32_t>(CullingMode);
	ViewOptions.Frozen = FrozenCullingView.has_value();
	ViewOptions.InstanceBatching = bInstanceBatching;
	ViewOptions.ModelBounds = bModelBounds;
	ViewOptions.LightBounds = bLightBounds;
	ViewOptions.StatusHud = bShowStatusHud;
	ViewOptions.ProfilingHud = bShowProfilingHud;
	ViewOptions.ProfilingCategories = ProfilingCategories;
	ViewOptions.Visualizer = Rendering.DebugMode;
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
	FitSceneCamera(ViewCamera, *Scene, ViewportSize.Height ? float(ViewportSize.Width) / ViewportSize.Height : 1, true);
	Camera.Reset();
}

void FEditorPlugin::FrameSelection(const FSceneMutationRequest& InRequest)
{
	UpdateDocumentInteraction();
	SceneDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	if (PreviewCamera)
	{
		throw FSceneEditError("unavailable", "Selection framing requires the editor browsing view");
	}
	if (bCameraDragging || Gui->PointerState().bRightDown || Gui->HasOpenPopup())
	{
		throw FSceneEditError("busy", "Selection framing is blocked by camera navigation or popup interaction");
	}
	if (!bViewportCameraInitialized || !ViewportSize.Width || !ViewportSize.Height)
	{
		throw FSceneEditError("busy", "Selection framing requires a ready scene and initialized viewport");
	}
	if (!Selection)
	{
		return;
	}
	// Validate every selection handle before SelectedRoots can discard stale entries.
	for (const auto Handle : Selection.All())
	{
		if (!Scene->FindNode(Handle))
		{
			throw FSceneEditError("stale_handle", "Selected object is no longer in this scene");
		}
	}
	const auto Bounds = SceneSelectionBounds(*Scene, SelectedRoots());
	auto Candidate = ViewCamera;
	FitSceneCamera(Candidate, Bounds, float(ViewportSize.Width) / ViewportSize.Height, true);
	ViewCamera = Candidate;
	ViewportClick.reset();
	Camera.Reset();
	bCameraDragging = false;
}

void FEditorPlugin::SetViewportOptions(const FSceneViewportOptions& InOptions)
{
	ValidateViewportOptions(InOptions, ViewportState().Options);
	if (InOptions.Visualizer.value_or(0) && InOptions.Visualizer != Rendering.DebugMode &&
	    Rendering.Pipeline != "deferred")
	{
		throw FSceneEditError("unavailable", "GBuffer visualizers require the Deferred pipeline");
	}
	if (InOptions.Frozen.value_or(false) && !FrozenCullingView)
	{
		const auto CameraView = PickingCamera();
		if (!CameraView || !bViewportCameraInitialized || !ViewportSize.Height)
		{
			throw FSceneEditError("unavailable", "A ready viewport camera is required before freezing culling");
		}
		FrozenCullingView = SceneCameraViewProjection(ExtractScenePose(CameraView->World), CameraView->Lens,
		                                              float(ViewportSize.Width) / ViewportSize.Height,
		                                              GetDepthConvention(Rendering.bReversedZ));
	}
	if (InOptions.Frozen && !*InOptions.Frozen)
	{
		FrozenCullingView.reset();
	}
	CullingMode = static_cast<ESceneCullingMode>(InOptions.Culling.value_or(static_cast<std::uint32_t>(CullingMode)));
	bInstanceBatching = InOptions.InstanceBatching.value_or(bInstanceBatching);
	bModelBounds = InOptions.ModelBounds.value_or(bModelBounds);
	bLightBounds = InOptions.LightBounds.value_or(bLightBounds);
	bShowStatusHud = InOptions.StatusHud.value_or(bShowStatusHud);
	bShowProfilingHud = InOptions.ProfilingHud.value_or(bShowProfilingHud);
	ProfilingCategories = InOptions.ProfilingCategories.value_or(ProfilingCategories);
	if ((InOptions.Exposure && *InOptions.Exposure != Exposure) ||
	    (InOptions.Visualizer && *InOptions.Visualizer != Rendering.DebugMode))
	{
		++RenderSettingsRevision;
	}
	Exposure = InOptions.Exposure.value_or(Exposure);
	Rendering.Exposure = Exposure;
	Rendering.DebugMode = InOptions.Visualizer.value_or(Rendering.DebugMode);
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
