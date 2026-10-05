#include "Hyperion/RenderControls/RenderBatchStats.h"

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
	PackedRecords += InOther.PackedRecords;
	ReusedRecords += InOther.ReusedRecords;
	AssembledBlocks += InOther.AssembledBlocks;
	ReusedBlocks += InOther.ReusedBlocks;
	AssembledBytes += InOther.AssembledBytes;
	UploadBytes += InOther.UploadBytes;
	GpuReuses += InOther.GpuReuses;
	CompatibilityReuses += InOther.CompatibilityReuses;
	CompatibilityBuilds += InOther.CompatibilityBuilds;
	InstanceContractBuilds += InOther.InstanceContractBuilds;
	PreparedInputBuilds += InOther.PreparedInputBuilds;
	PreparedInputReuses += InOther.PreparedInputReuses;
	PlanReuses += InOther.PlanReuses;
	PacketReuses += InOther.PacketReuses;
	LocalPlanReuses += InOther.LocalPlanReuses;
	LocalPacketReuses += InOther.LocalPacketReuses;
	LocalInputReuses += InOther.LocalInputReuses;
	LocalCompatibilityReuses += InOther.LocalCompatibilityReuses;
	LocalRecordReuses += InOther.LocalRecordReuses;
	IncrementalPlanUpdates += InOther.IncrementalPlanUpdates;
	IncrementalItemReuses += InOther.IncrementalItemReuses;
	AffectedBatches += InOther.AffectedBatches;
	RetainedBatches += InOther.RetainedBatches;
	BatchAdmissionReuses += InOther.BatchAdmissionReuses;
	Evictions += InOther.Evictions;
	CachedPlanItems = InOther.CachedPlanItems;
	CachedPlanBlocks = InOther.CachedPlanBlocks;
	CachedInputs = InOther.CachedInputs;
	CachedInputBytes = InOther.CachedInputBytes;
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

std::string_view GetRenderBatchFallbackName(ERenderBatchFallback InReason)
{
	constexpr std::array Names{"disabled", "shader", "device", "ordering", "singleton", "preparation"};
	const auto Index = static_cast<std::size_t>(InReason);
	return Index < Names.size() ? Names[Index] : "unknown";
}
} // namespace Hyperion
