#pragma once
#include "Hyperion/Config/AppSettings.h"
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Gui/Gui.h"
#include "Hyperion/RHI/RHITypes.h"
#include "Hyperion/Renderer/FramePipeline.h"
#include <array>
#include <optional>

namespace Hyperion
{
struct FFrameCaptureMetrics
{
	bool bCompiled = false;
	bool bAvailable = false;
	bool bBusy = false;
	std::string Status = "RenderDoc support is not compiled in";
	std::string LastCapture;
	std::string OpenStatus;
};

struct FDebugMetrics
{
	FDeviceStats Device;
	FFramePipelineProgress FramePipeline;
	FFramePipelineLimits FrameLimits;
	std::uint64_t ResultFrame{};
	std::size_t LegacyDisplayItems{};
	std::uint64_t SceneTargetBytes{};
	std::uint32_t HierarchicalDepthConsumers{};
	std::uint32_t HierarchicalDepthDispatches{};
	std::uint64_t HierarchicalDepthBytes{};
	bool bContactShadows{};
	std::vector<FExecutionStats> Threads;
	std::vector<float> FrameMilliseconds;
	std::string AssetStatus;
	bool bSceneViewer{};
	bool bActiveReversedZ{};
	FFrameCaptureMetrics FrameCapture;
	FProfileStatus Profiling;
};

struct FDebugActions
{
	bool bSave{};
	bool bCapture{};
	bool bCaptureRdc{};
	bool bOpenRdc{};
	std::optional<std::uint32_t> ProfilingMask;
	std::optional<bool> Sampling;
	// Logical pixel bounds allow normalized input acceptance without OS input injection.
	FVec4 CaptureRdcBounds;
	FVec4 OpenRdcBounds;
	FVec4 AutoOpenRdcBounds;
	FVec4 ContactShadowBounds;
	std::array<FVec4, 4> ProfilingBounds;
};

struct FDebugPanelEvent
{
	FGui& Gui;
	FAppSettings& Settings;
	const FDebugMetrics& Metrics;
	FSize LogicalSize;
	FDebugActions& Actions;
};
} // namespace Hyperion
