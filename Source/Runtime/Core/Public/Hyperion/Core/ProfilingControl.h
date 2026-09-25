#pragma once
#include <cstdint>
#include <optional>

namespace Hyperion
{
class IProfilingControl
{
public:
	virtual ~IProfilingControl() = default;
	virtual void ChangeProfiling(std::optional<std::uint32_t> InMask, std::optional<bool> InSampling) = 0;
};
} // namespace Hyperion
