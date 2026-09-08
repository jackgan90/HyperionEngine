#include "Hyperion/DebugUI/DebugUIPlugin.h"

namespace Hyperion
{
FDebugActions DrawProfilingControls(FGui& InGui, const FProfileStatus& InStatus)
{
	FDebugActions Actions;
	InGui.Text("PROFILING");
	if (!InStatus.bCompiled)
	{
		InGui.TextWrapped("Tracy support is not compiled in.");
		return Actions;
	}
	InGui.Text(InStatus.bConnected ? "Tracy connected" : "Waiting for Tracy collector");
	auto Mask = InStatus.Mask;
	bool bCpu = (Mask & ProfileBasicMask) != 0;
	bool bDetail = (Mask & ProfileCategoryMask(EProfileCategory::Detail)) != 0;
	bool bGpu = (Mask & ProfileCategoryMask(EProfileCategory::Gpu)) != 0;
	if (InGui.Checkbox("Record CPU scopes", bCpu))
	{
		Mask = bCpu ? Mask | ProfileBasicMask : Mask & ~ProfileBasicMask;
	}
	Actions.ProfilingBounds[0] = InGui.LastItemBounds();
	if (InGui.Checkbox("Detailed hotspots", bDetail))
	{
		const auto Bit = ProfileCategoryMask(EProfileCategory::Detail);
		Mask = bDetail ? Mask | Bit : Mask & ~Bit;
	}
	Actions.ProfilingBounds[1] = InGui.LastItemBounds();
	if (InGui.Checkbox("GPU pass timings", bGpu))
	{
		const auto Bit = ProfileCategoryMask(EProfileCategory::Gpu);
		Mask = bGpu ? Mask | Bit : Mask & ~Bit;
	}
	Actions.ProfilingBounds[2] = InGui.LastItemBounds();
	if (Mask != InStatus.Mask)
	{
		Actions.ProfilingMask = Mask;
	}
	bool bSampling = InStatus.Sampling == EProfileSampling::Requested;
	if (InGui.Checkbox("Request CPU sampling", bSampling))
	{
		Actions.Sampling = bSampling;
	}
	Actions.ProfilingBounds[3] = InGui.LastItemBounds();
	if (InStatus.Sampling == EProfileSampling::Unavailable)
	{
		InGui.TextWrapped("Sampling unavailable. Enable scopes; ETW requires elevated access.");
	}
	else if (bSampling)
	{
		InGui.TextWrapped("Sampling requested; verify samples in Tracy.");
	}
	return Actions;
}
} // namespace Hyperion
