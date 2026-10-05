#include "Hyperion/Renderer/RenderDiagnostics.h"
#include "Hyperion/Core/Core.h"

namespace Hyperion
{
void SetExecutionDiagnostics(FRenderDiagnostics& InResult, const FTaskSystem& InTasks, double InFrameMilliseconds)
{
	InResult.FrameIntervalMilliseconds = InFrameMilliseconds;
	InResult.TrackedCpuBytes = 0;
	for (int Index = 0; Index < static_cast<int>(EMemoryTag::Count); ++Index)
	{
		InResult.TrackedCpuBytes += MemoryStats(static_cast<EMemoryTag>(Index)).LiveBytes;
	}
	InResult.ExecutedTasks.clear();
	for (const auto& Thread : InTasks.Statistics())
	{
		InResult.ExecutedTasks[Thread.Name] = Thread.Executed;
	}
}

void SetDeviceDiagnostics(FRenderDiagnostics& InResult, const FDeviceStats& InDevice)
{
	InResult.Adapter = InDevice.Adapter;
	InResult.Device = {{"validationErrors", InDevice.ValidationErrors},
	                   {"submittedFrames", InDevice.SubmittedFrames},
	                   {"gpuAllocationBytes", InDevice.GpuAllocationBytes},
	                   {"descriptorAllocations", InDevice.DescriptorAllocations},
	                   {"descriptorCopies", InDevice.DescriptorCopies},
	                   {"bindingSetsCreated", InDevice.BindingSetsCreated},
	                   {"graphicsRootBinds", InDevice.GraphicsRootBinds},
	                   {"graphicsHeapBinds", InDevice.GraphicsHeapBinds},
	                   {"graphicsConstantBinds", InDevice.GraphicsConstantBinds},
	                   {"graphicsTableBinds", InDevice.GraphicsTableBinds},
	                   {"pipelinesCreated", InDevice.PipelinesCreated},
	                   {"constantBytesWritten", InDevice.ConstantBytesWritten},
	                   {"graphicsPipelineBinds", InDevice.GraphicsPipelineBinds},
	                   {"graphicsGeometryBinds", InDevice.GraphicsGeometryBinds},
	                   {"graphicsDynamicBinds", InDevice.GraphicsDynamicBinds},
	                   {"commandListsCreated", InDevice.CommandListsCreated},
	                   {"commandListResets", InDevice.CommandListResets}};
	InResult.GpuTimingFrame = InDevice.GpuTiming.Frame;
	for (const auto& Pass : InDevice.GpuTiming.Passes)
	{
		InResult.GpuPassMilliseconds[Pass.Name] = Pass.Milliseconds;
	}
}
} // namespace Hyperion
