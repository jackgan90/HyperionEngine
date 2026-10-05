#pragma once
#include "Hyperion/RHI/RHIDevice.h"
#include "Hyperion/RenderControls/RenderDiagnostics.h"
#include "Hyperion/Tasks/TaskSystem.h"

namespace Hyperion
{
void SetExecutionDiagnostics(FRenderDiagnostics& InResult, const FTaskSystem& InTasks, double InFrameMilliseconds);
void SetDeviceDiagnostics(FRenderDiagnostics& InResult, const FDeviceStats& InDevice);
} // namespace Hyperion
