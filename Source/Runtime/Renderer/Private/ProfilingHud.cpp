#include "Hyperion/Renderer/ProfilingHud.h"
#include <array>
#include <stdexcept>

namespace Hyperion
{
namespace
{
constexpr std::array Options{FProfilingHudOption{EProfilingHudCategory::Overview, "Overview"},
                             FProfilingHudOption{EProfilingHudCategory::Tasks, "Tasks"},
                             FProfilingHudOption{EProfilingHudCategory::GpuPasses, "GPU passes"},
                             FProfilingHudOption{EProfilingHudCategory::DeviceCounters, "Device counters"},
                             FProfilingHudOption{EProfilingHudCategory::RenderViews, "Render views"},
                             FProfilingHudOption{EProfilingHudCategory::LightingHzb, "Lighting / HZB"},
                             FProfilingHudOption{EProfilingHudCategory::Visibility, "Visibility"},
                             FProfilingHudOption{EProfilingHudCategory::Batching, "Batching"}};
constexpr std::uint32_t SupportedMask = []
{
	std::uint32_t Result{};
	for (const auto& Option : Options)
	{
		Result |= static_cast<std::uint32_t>(Option.Id);
	}
	return Result;
}();
} // namespace

std::span<const FProfilingHudOption> ProfilingHudOptions()
{
	return Options;
}

bool IsProfilingHudWireValue(std::uint32_t InValue)
{
	return (InValue & ~SupportedMask) == 0;
}

EProfilingHudCategory ParseProfilingHudCategories(std::uint32_t InValue)
{
	if (!IsProfilingHudWireValue(InValue))
	{
		throw std::invalid_argument("Unknown profiling HUD category bits");
	}
	return static_cast<EProfilingHudCategory>(InValue);
}

std::uint32_t ToProfilingHudWireValue(EProfilingHudCategory InCategories)
{
	return static_cast<std::uint32_t>(ParseProfilingHudCategories(static_cast<std::uint32_t>(InCategories)));
}

bool HasProfilingHudCategory(EProfilingHudCategory InCategories, EProfilingHudCategory InCategory)
{
	return (ToProfilingHudWireValue(InCategories) & ToProfilingHudWireValue(InCategory)) != 0;
}

EProfilingHudCategory WithProfilingHudCategory(EProfilingHudCategory InCategories, EProfilingHudCategory InCategory,
                                               bool bInEnabled)
{
	const auto Mask = ToProfilingHudWireValue(InCategories);
	const auto Bit = ToProfilingHudWireValue(InCategory);
	return ParseProfilingHudCategories(bInEnabled ? Mask | Bit : Mask & ~Bit);
}
} // namespace Hyperion
