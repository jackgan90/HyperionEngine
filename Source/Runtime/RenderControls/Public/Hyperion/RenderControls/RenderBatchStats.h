#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Hyperion
{
enum class ERenderBatchFallback
{
	Disabled,
	Shader,
	Device,
	Order,
	Singleton,
	Preparation,
	Count
};

struct FRenderBatchStats
{
	std::uint64_t EligibleItems{};
	std::uint64_t InstancedDraws{};
	std::uint64_t InstancedItems{};
	std::uint64_t SingleDraws{};
	std::uint64_t FailedItems{};
	std::uint64_t CapacitySplits{};
	std::uint64_t ReusedChunks{};
	std::uint64_t RebuiltChunks{};
	std::uint64_t PackedBytes{};
	std::uint64_t PackedRecords{};
	std::uint64_t ReusedRecords{};
	std::uint64_t AssembledBlocks{};
	std::uint64_t ReusedBlocks{};
	std::uint64_t AssembledBytes{};
	std::uint64_t UploadBytes{};
	std::uint64_t GpuReuses{};
	std::uint64_t CompatibilityReuses{};
	std::uint64_t CompatibilityBuilds{};
	std::uint64_t InstanceContractBuilds{};
	std::uint64_t PreparedInputBuilds{};
	std::uint64_t PreparedInputReuses{};
	std::uint64_t PlanReuses{};
	std::uint64_t PacketReuses{};
	std::uint64_t LocalPlanReuses{};
	std::uint64_t LocalPacketReuses{};
	std::uint64_t LocalInputReuses{};
	std::uint64_t LocalCompatibilityReuses{};
	std::uint64_t LocalRecordReuses{};
	std::uint64_t IncrementalPlanUpdates{};
	std::uint64_t IncrementalItemReuses{};
	std::uint64_t AffectedBatches{};
	std::uint64_t RetainedBatches{};
	std::uint64_t BatchAdmissionReuses{};
	std::uint64_t Evictions{};
	std::size_t CachedPlanItems{};
	std::size_t CachedPlanBlocks{};
	std::size_t CachedInputs{};
	std::size_t CachedInputBytes{}; // Estimate of count-bounded weak planning metadata, separate from payload bytes.
	std::size_t CachedChunks{};
	std::size_t CachedBytes{};
	double PlanningMilliseconds{};
	double PreparationMilliseconds{};
	std::array<std::uint64_t, static_cast<std::size_t>(ERenderBatchFallback::Count)> Fallbacks{};
	FRenderBatchStats& operator+=(const FRenderBatchStats& InOther);
};

std::string_view GetRenderBatchFallbackName(ERenderBatchFallback InReason);
} // namespace Hyperion
