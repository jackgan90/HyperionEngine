#pragma once
#include <atomic>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace Hyperion::MaterialsPrivate
{
inline std::uint64_t NextIdentity()
{
	static std::atomic<std::uint64_t> Next{1};
	std::uint64_t Result = Next.load(std::memory_order_relaxed);
	for (;;)
	{
		if (Result == std::numeric_limits<std::uint64_t>::max())
		{
			throw std::overflow_error("Material identity exhausted");
		}
		if (Next.compare_exchange_weak(Result, Result + 1, std::memory_order_relaxed))
		{
			return Result;
		}
	}
}
} // namespace Hyperion::MaterialsPrivate
