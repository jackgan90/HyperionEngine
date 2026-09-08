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
	std::uint64_t UploadBytes{};
	std::uint64_t GpuReuses{};
	std::uint64_t CompatibilityReuses{};
	std::uint64_t CompatibilityBuilds{};
	std::uint64_t Evictions{};
	std::size_t CachedChunks{};
	std::size_t CachedBytes{};
	double PlanningMilliseconds{};
	double PreparationMilliseconds{};
	std::array<std::uint64_t, static_cast<std::size_t>(ERenderBatchFallback::Count)> Fallbacks{};
	FRenderBatchStats& operator+=(const FRenderBatchStats& InOther);
};

std::string_view GetRenderBatchFallbackName(ERenderBatchFallback InReason);
} // namespace Hyperion
