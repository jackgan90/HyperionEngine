#include "EditorApplication.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include "Hyperion/Renderer/ViewportChoices.h"
#include "Hyperion/Renderer/ViewportRay.h"
#include <algorithm>

namespace Hyperion
{
FSceneComponentDiagnostics FEditorPlugin::ComponentDiagnostics(FSceneHandle InHandle, std::string_view InComponent)
{
	if (!Scene->FindNode(InHandle))
	{
		throw FSceneEditError(SceneEditErrors::StaleHandle, "Object is no longer in this scene");
	}
	const auto Value = Scene->GetComponentDiagnostics(InHandle, InComponent);
	if (!Value)
	{
		throw FSceneEditError(SceneEditErrors::NotFound, "Component has no published rendering diagnostics");
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
	Result.Pipeline = static_cast<const FForwardPipelineStatistics&>(RenderStats);
	Result.FrameIntervalStatistics = FrameTiming.Snapshot();
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
	ViewOptions.OutlineMode = ToOutlineWireValue(OutlineSettings.Overlap);
	ViewOptions.SmoothOutlines = OutlineSettings.bSupersample;
	ViewOptions.Culling = ToCullingWireValue(CullingMode);
	ViewOptions.Frozen = FrozenCullingView.has_value();
	ViewOptions.InstanceBatching = bInstanceBatching;
	ViewOptions.ModelBounds = bModelBounds;
	ViewOptions.LightBounds = bLightBounds;
	ViewOptions.StatusHud = bShowStatusHud;
	ViewOptions.ProfilingHud = bShowProfilingHud;
	ViewOptions.ProfilingCategories = ToProfilingHudWireValue(ProfilingCategories);
	ViewOptions.Visualizer = ToVisualizerWireValue(Rendering.DebugMode);
	return {Viewport.ViewCamera,
	        Viewport.PreviewCamera,
	        ViewOptions,
	        Camera.GetMovementSpeed(Viewport.ViewCamera),
	        Viewport.bViewportCameraInitialized,
	        true};
}

void FEditorPlugin::SetViewportCamera(const FSceneCameraView& InCamera)
{
	ValidateSceneCameraView(InCamera);
	SetPreviewCamera({});
	Viewport.ViewCamera = InCamera;
	Viewport.bViewportCameraInitialized = true;
}

void FEditorPlugin::FrameScene()
{
	Viewport.FrameScene(*Scene);
}

void FEditorPlugin::FrameSelection(const FSceneMutationRequest& InRequest)
{
	UpdateDocumentInteraction();
	SceneDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	if (Viewport.PreviewCamera)
	{
		throw FSceneEditError(SceneEditErrors::Unavailable, "Selection framing requires the editor browsing view");
	}
	if (Viewport.bCameraDragging || Gui->PointerState().bRightDown || Gui->HasOpenPopup())
	{
		throw FSceneEditError(SceneEditErrors::Busy,
		                      "Selection framing is blocked by camera navigation or popup interaction");
	}
	if (!Viewport.bViewportCameraInitialized || !Viewport.ViewportSize.Width || !Viewport.ViewportSize.Height)
	{
		throw FSceneEditError(SceneEditErrors::Busy,
		                      "Selection framing requires a ready scene and initialized viewport");
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
			throw FSceneEditError(SceneEditErrors::StaleHandle, "Selected object is no longer in this scene");
		}
	}
	const auto Bounds = SceneSelectionBounds(*Scene, SelectedRoots());
	auto Candidate = Viewport.ViewCamera;
	FitSceneCamera(Candidate, Bounds, float(Viewport.ViewportSize.Width) / Viewport.ViewportSize.Height, true);
	// Keep distant scene geometry visible when framing small objects.
	Candidate.Lens.Far = std::max(Viewport.ViewCamera.Lens.Far, Candidate.Lens.Far);
	Viewport.ViewCamera = Candidate;
	ViewportClick.reset();
	Camera.Reset();
	Viewport.bCameraDragging = false;
}

void FEditorPlugin::SetViewportOptions(const FSceneViewportOptions& InOptions)
{
	ValidateViewportOptions(InOptions, ViewportState().Options);
	if (InOptions.Visualizer && ParseGBufferVisualizer(*InOptions.Visualizer) != EGBufferVisualizer::Lit &&
	    *InOptions.Visualizer != ToVisualizerWireValue(Rendering.DebugMode) &&
	    Rendering.Pipeline != ESceneRenderPipeline::Deferred)
	{
		throw FSceneEditError(SceneEditErrors::Unavailable, "GBuffer visualizers require the Deferred pipeline");
	}
	if (InOptions.Frozen.value_or(false) && !FrozenCullingView)
	{
		const auto CameraView = PickingCamera();
		if (!CameraView || !Viewport.bViewportCameraInitialized || !Viewport.ViewportSize.Height)
		{
			throw FSceneEditError(SceneEditErrors::Unavailable,
			                      "A ready viewport camera is required before freezing culling");
		}
		FrozenCullingView = SceneCameraViewProjection(ExtractScenePose(CameraView->World), CameraView->Lens,
		                                              float(Viewport.ViewportSize.Width) / Viewport.ViewportSize.Height,
		                                              GetDepthConvention(Rendering.bReversedZ));
	}
	if (InOptions.Frozen && !*InOptions.Frozen)
	{
		FrozenCullingView.reset();
	}
	CullingMode = ParseSceneCullingMode(InOptions.Culling.value_or(ToCullingWireValue(CullingMode)));
	bInstanceBatching = InOptions.InstanceBatching.value_or(bInstanceBatching);
	bModelBounds = InOptions.ModelBounds.value_or(bModelBounds);
	bLightBounds = InOptions.LightBounds.value_or(bLightBounds);
	bShowStatusHud = InOptions.StatusHud.value_or(bShowStatusHud);
	bShowProfilingHud = InOptions.ProfilingHud.value_or(bShowProfilingHud);
	ProfilingCategories = ParseProfilingHudCategories(
	    InOptions.ProfilingCategories.value_or(ToProfilingHudWireValue(ProfilingCategories)));
	if ((InOptions.Exposure && *InOptions.Exposure != Exposure) ||
	    (InOptions.Visualizer && *InOptions.Visualizer != ToVisualizerWireValue(Rendering.DebugMode)))
	{
		++RenderSettingsRevision;
	}
	Exposure = InOptions.Exposure.value_or(Exposure);
	Rendering.Exposure = Exposure;
	Rendering.DebugMode =
	    ParseGBufferVisualizer(InOptions.Visualizer.value_or(ToVisualizerWireValue(Rendering.DebugMode)));
	bShowLightMarkers = InOptions.LightMarkers.value_or(bShowLightMarkers);
	OutlineSettings.Overlap =
	    ParseOutlineOverlapMode(InOptions.OutlineMode.value_or(ToOutlineWireValue(OutlineSettings.Overlap)));
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
			throw FSceneEditError(SceneEditErrors::InvalidArguments,
			                      "Preview requires an enabled camera in the current scene");
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
