#include "Hyperion/Renderer/SceneViewport.h"
#include "Hyperion/RasterOptions/RasterOptions.h"
#include "Hyperion/Renderer/ViewportChoices.h"
#include <cmath>

namespace Hyperion
{
namespace
{
std::string VisualizerDescription()
{
	std::string Result;
	for (const auto& Option : GBufferVisualizerOptions())
	{
		if (!Result.empty())
		{
			Result += ", ";
		}
		Result += std::to_string(ToVisualizerWireValue(Option.Id)) + " " + std::string(Option.Label);
	}
	return Result + ". GBuffer modes require Deferred.";
}

void Unsupported()
{
	throw FSceneEditError("unavailable", "This viewport does not support the requested control");
}

template<class T> void RequireSupported(const std::optional<T>& InPatch, const std::optional<T>& InSupported)
{
	if (InPatch && !InSupported)
	{
		Unsupported();
	}
}
} // namespace

void ISceneViewport::SetViewportSpeed(float)
{
	Unsupported();
}

void ISceneViewport::FrameSelection(const FSceneMutationRequest&)
{
	Unsupported();
}

void ISceneViewport::PreviewSceneCamera(std::optional<FSceneHandle>)
{
	Unsupported();
}

void ISceneViewport::SaveInitialView()
{
	Unsupported();
}

void ISceneViewport::CreateViewCamera()
{
	Unsupported();
}

void ISceneViewport::ApplyViewToCamera(FSceneHandle)
{
	Unsupported();
}

void ValidateViewportOptions(const FSceneViewportOptions& InPatch, const FSceneViewportOptions& InSupported)
{
	RequireSupported(InPatch.Exposure, InSupported.Exposure);
	RequireSupported(InPatch.LightMarkers, InSupported.LightMarkers);
	RequireSupported(InPatch.OutlineMode, InSupported.OutlineMode);
	RequireSupported(InPatch.SmoothOutlines, InSupported.SmoothOutlines);
	RequireSupported(InPatch.Culling, InSupported.Culling);
	RequireSupported(InPatch.Frozen, InSupported.Frozen);
	RequireSupported(InPatch.InstanceBatching, InSupported.InstanceBatching);
	RequireSupported(InPatch.ModelBounds, InSupported.ModelBounds);
	RequireSupported(InPatch.LightBounds, InSupported.LightBounds);
	RequireSupported(InPatch.Animate, InSupported.Animate);
	RequireSupported(InPatch.StatusHud, InSupported.StatusHud);
	RequireSupported(InPatch.ProfilingHud, InSupported.ProfilingHud);
	RequireSupported(InPatch.ProfilingCategories, InSupported.ProfilingCategories);
	RequireSupported(InPatch.Visualizer, InSupported.Visualizer);
	if (InPatch.Visualizer)
	{
		(void)ParseGBufferVisualizer(*InPatch.Visualizer);
	}
	if ((InPatch.Exposure &&
	     (!std::isfinite(*InPatch.Exposure) || *InPatch.Exposure < .05f || *InPatch.Exposure > 8)) ||
	    (InPatch.OutlineMode && !IsOutlineOverlapWireValue(*InPatch.OutlineMode)) ||
	    (InPatch.Culling && !IsSceneCullingWireValue(*InPatch.Culling)) ||
	    (InPatch.ProfilingCategories && (*InPatch.ProfilingCategories & ~255u)))
	{
		throw std::invalid_argument("Exposure must be 0.05-8; outlineMode 0-1; culling 0-2; profiling mask 0-255");
	}
}

template<> const FRecordDescriptor& RecordType<FSceneViewportOptions>()
{
	static const auto Type = MakeRecord<FSceneViewportOptions>(
	    "hyperion.viewport.options",
	    {Member("exposure", &FSceneViewportOptions::Exposure),
	     Member("lightMarkers", &FSceneViewportOptions::LightMarkers),
	     Member("outlineMode", &FSceneViewportOptions::OutlineMode, {.Description = "0 union, 1 per-object."}),
	     Member("smoothOutlines", &FSceneViewportOptions::SmoothOutlines),
	     Member("culling", &FSceneViewportOptions::Culling,
	            {.Description = "0 none, 1 linear frustum, 2 BVH frustum."}),
	     Member("frozen", &FSceneViewportOptions::Frozen),
	     Member("instanceBatching", &FSceneViewportOptions::InstanceBatching),
	     Member("modelBounds", &FSceneViewportOptions::ModelBounds),
	     Member("lightBounds", &FSceneViewportOptions::LightBounds), Member("animate", &FSceneViewportOptions::Animate),
	     Member("statusHud", &FSceneViewportOptions::StatusHud),
	     Member("profilingHud", &FSceneViewportOptions::ProfilingHud),
	     Member("profilingCategories", &FSceneViewportOptions::ProfilingCategories,
	            {.Description = "Bit mask: 1 overview, 2 tasks, 4 GPU passes, 8 device, 16 views, 32 lighting, "
	                            "64 visibility, 128 batching."}),
	     Member("visualizer", &FSceneViewportOptions::Visualizer, {.Description = VisualizerDescription()})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneViewportState>()
{
	static const auto Type = MakeRecord<FSceneViewportState>(
	    "hyperion.viewport.state",
	    {Member("camera", &FSceneViewportState::Camera), Member("previewCamera", &FSceneViewportState::PreviewCamera),
	     Member("options", &FSceneViewportState::Options,
	            {.Description = "Null members identify controls unsupported by this viewport."}),
	     Member("movementSpeed", &FSceneViewportState::MovementSpeed), Member("ready", &FSceneViewportState::bReady),
	     Member("cameraAuthoring", &FSceneViewportState::bCameraAuthoring)});
	return Type;
}
} // namespace Hyperion
