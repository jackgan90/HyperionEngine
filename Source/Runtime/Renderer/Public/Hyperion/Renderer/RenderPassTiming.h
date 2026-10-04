#pragma once
#include "Hyperion/RHI/RHITypes.h"

namespace Hyperion
{
enum class ERenderPassTimingCategory : std::uint32_t
{
	Unclassified,
	Shadow,
	Forward,
	DeferredBase,
	Lighting,
	Compatibility,
	Transparent,
	Tonemap,
	Sky,
	HierarchicalDepth,
	ContactShadow,
	LocalLights,
	Count
};

struct FRenderPassTiming
{
	ERenderPassTimingCategory Category = ERenderPassTimingCategory::Unclassified;
	std::uint32_t Instance{}; // Category-local instance, e.g. shadow cascade or HZB mip.
	bool operator==(const FRenderPassTiming&) const = default;
};

struct FRenderGpuTimings
{
	double Total{};
	double Shadow{};
	std::array<double, 4> Cascades{};
	double Forward{};
	double DeferredBase{};
	double Lighting{};
	double Compatibility{};
	double Transparent{};
	double Tonemap{};
	double Sky{};
	double HierarchicalDepth{};
	double ContactShadow{};
	double LocalLights{};
};

FGpuTimingTag EncodeRenderPassTiming(FRenderPassTiming InTiming);
std::optional<FRenderPassTiming> DecodeRenderPassTiming(FGpuTimingTag InTag);
FRenderGpuTimings AggregateRenderGpuTimings(const FGpuFrameTiming& InTiming);
} // namespace Hyperion
