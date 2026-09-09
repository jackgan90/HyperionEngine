#include "Hyperion/Renderer/RenderBatchStats.h"

namespace Hyperion
{
FRenderBatchStats& FRenderBatchStats::operator+=(const FRenderBatchStats& InOther)
{
	EligibleItems += InOther.EligibleItems;
	InstancedDraws += InOther.InstancedDraws;
	InstancedItems += InOther.InstancedItems;
	SingleDraws += InOther.SingleDraws;
	FailedItems += InOther.FailedItems;
	CapacitySplits += InOther.CapacitySplits;
	ReusedChunks += InOther.ReusedChunks;
	RebuiltChunks += InOther.RebuiltChunks;
	PackedBytes += InOther.PackedBytes;
	UploadBytes += InOther.UploadBytes;
	GpuReuses += InOther.GpuReuses;
	CompatibilityReuses += InOther.CompatibilityReuses;
	CompatibilityBuilds += InOther.CompatibilityBuilds;
	PlanReuses += InOther.PlanReuses;
	PacketReuses += InOther.PacketReuses;
	Evictions += InOther.Evictions;
	CachedChunks = InOther.CachedChunks; // Global cache gauges use the latest view's observation.
	CachedBytes = InOther.CachedBytes;
	PlanningMilliseconds += InOther.PlanningMilliseconds;
	PreparationMilliseconds += InOther.PreparationMilliseconds;
	for (std::size_t Index = 0; Index < Fallbacks.size(); ++Index)
	{
		Fallbacks[Index] += InOther.Fallbacks[Index];
	}
	return *this;
}
} // namespace Hyperion
