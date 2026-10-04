#include "Hyperion/Renderer/RenderPassTiming.h"

namespace Hyperion
{
namespace
{
// Runtime-only domain discriminator; no Renderer category knowledge is required by RHI.
constexpr std::uint64_t RenderTimingDomain = 0x48595054494d4501;
} // namespace

FGpuTimingTag EncodeRenderPassTiming(FRenderPassTiming InTiming)
{
	if (InTiming.Category >= ERenderPassTimingCategory::Count)
	{
		throw std::invalid_argument("Unknown render pass timing category");
	}
	if (InTiming.Category == ERenderPassTimingCategory::Unclassified)
	{
		return {};
	}
	return {RenderTimingDomain, (std::uint64_t(InTiming.Instance) << 32) | std::uint32_t(InTiming.Category)};
}

std::optional<FRenderPassTiming> DecodeRenderPassTiming(FGpuTimingTag InTag)
{
	const auto Category = static_cast<ERenderPassTimingCategory>(std::uint32_t(InTag.Value));
	if (InTag.Domain != RenderTimingDomain || Category == ERenderPassTimingCategory::Unclassified ||
	    Category >= ERenderPassTimingCategory::Count)
	{
		return {};
	}
	return FRenderPassTiming{Category, std::uint32_t(InTag.Value >> 32)};
}

FRenderGpuTimings AggregateRenderGpuTimings(const FGpuFrameTiming& InTiming)
{
	FRenderGpuTimings Result;
	for (const auto& Pass : InTiming.Passes)
	{
		Result.Total += Pass.Milliseconds;
		const auto Timing = DecodeRenderPassTiming(Pass.Tag);
		if (!Timing)
		{
			continue;
		}
		switch (Timing->Category)
		{
			case ERenderPassTimingCategory::Shadow:
				Result.Shadow += Pass.Milliseconds;
				if (Timing->Instance < Result.Cascades.size())
				{
					Result.Cascades[Timing->Instance] += Pass.Milliseconds;
				}
				break;
			case ERenderPassTimingCategory::Forward:
				Result.Forward += Pass.Milliseconds;
				break;
			case ERenderPassTimingCategory::DeferredBase:
				Result.DeferredBase += Pass.Milliseconds;
				break;
			case ERenderPassTimingCategory::Lighting:
				Result.Lighting += Pass.Milliseconds;
				break;
			case ERenderPassTimingCategory::Compatibility:
				Result.Compatibility += Pass.Milliseconds;
				break;
			case ERenderPassTimingCategory::Transparent:
				Result.Transparent += Pass.Milliseconds;
				break;
			case ERenderPassTimingCategory::Tonemap:
				Result.Tonemap += Pass.Milliseconds;
				break;
			case ERenderPassTimingCategory::Sky:
				Result.Sky += Pass.Milliseconds;
				break;
			case ERenderPassTimingCategory::HierarchicalDepth:
				Result.HierarchicalDepth += Pass.Milliseconds;
				break;
			case ERenderPassTimingCategory::ContactShadow:
				Result.ContactShadow += Pass.Milliseconds;
				break;
			case ERenderPassTimingCategory::LocalLights:
				Result.LocalLights += Pass.Milliseconds;
				break;
			case ERenderPassTimingCategory::Unclassified:
			case ERenderPassTimingCategory::Count:
				break;
		}
	}
	return Result;
}
} // namespace Hyperion
