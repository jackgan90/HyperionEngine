#pragma once
#include "Hyperion/Reflection/RecordValue.h"
#include "Hyperion/Renderer/RenderPipelineFrame.h"
#include "Hyperion/Renderer/SceneDiagnostics.h"

namespace Hyperion
{
struct FRenderDiagnostics
{
	std::uint64_t Frame{};
	bool bReady{};
	std::string SceneError;
	FForwardPipelineStatistics Pipeline;
	std::string Adapter;
	std::map<std::string, std::uint64_t> Device;
	std::map<std::string, double> GpuPassMilliseconds;
	std::uint64_t GpuTimingFrame{};
};

class IRenderDiagnostics
{
public:
	virtual ~IRenderDiagnostics() = default;
	virtual FRenderDiagnostics RenderDiagnostics() = 0;
	virtual FSceneComponentDiagnostics ComponentDiagnostics(FSceneHandle InHandle, std::string_view InComponent) = 0;
};

void SetDeviceDiagnostics(FRenderDiagnostics& InResult, const FDeviceStats& InDevice);
template<> std::span<const ESceneCameraStatus> RecordEnumValues<ESceneCameraStatus>();
template<> const FRecordDescriptor& RecordType<FRenderBatchStats>();
template<> const FRecordDescriptor& RecordType<FSceneVisibilityStats>();
template<> const FRecordDescriptor& RecordType<FLocalLightStatistics>();
template<> const FRecordDescriptor& RecordType<FSelectionOutlineStatistics>();
template<> const FRecordDescriptor& RecordType<FHierarchicalDepthStats>();
template<> const FRecordDescriptor& RecordType<FRenderViewStatistics>();
template<> const FRecordDescriptor& RecordType<FForwardPipelineStatistics>();
template<> const FRecordDescriptor& RecordType<FRenderDiagnostics>();
} // namespace Hyperion
