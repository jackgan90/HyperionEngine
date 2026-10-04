#pragma once
#include <cstdint>
#include <span>
#include <string_view>

namespace Hyperion
{
enum class EProfilingHudCategory : std::uint32_t
{
	None = 0,
	Overview = 1u << 0,
	Tasks = 1u << 1,
	GpuPasses = 1u << 2,
	DeviceCounters = 1u << 3,
	RenderViews = 1u << 4,
	LightingHzb = 1u << 5,
	Visibility = 1u << 6,
	Batching = 1u << 7
};

struct FProfilingHudOption
{
	EProfilingHudCategory Id;
	std::string_view Label;
};

std::span<const FProfilingHudOption> ProfilingHudOptions();
bool IsProfilingHudWireValue(std::uint32_t InValue);
EProfilingHudCategory ParseProfilingHudCategories(std::uint32_t InValue);
std::uint32_t ToProfilingHudWireValue(EProfilingHudCategory InCategories);
bool HasProfilingHudCategory(EProfilingHudCategory InCategories, EProfilingHudCategory InCategory);
EProfilingHudCategory WithProfilingHudCategory(EProfilingHudCategory InCategories, EProfilingHudCategory InCategory,
                                               bool bInEnabled);
} // namespace Hyperion
