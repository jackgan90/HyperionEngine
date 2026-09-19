#pragma once
#include "Hyperion/Math/Ray.h"
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace Hyperion
{
// Immutable after construction; compact leaves contain ranges of original item indices.
class FStaticBoundsBvh
{
public:
	explicit FStaticBoundsBvh(std::span<const FBounds> InBounds, const std::function<void()>& InCheckCancellation = {});
	void Raycast(FRay InRay, const std::function<void(std::uint32_t, float&)>& InVisit,
	             std::size_t& OutVisitedNodes) const;
	std::size_t GetStorageBytes() const;

private:
	struct FNode
	{
		FBounds Bounds;
		std::uint32_t Begin{};
		std::uint32_t Count{};
		std::uint32_t Left{};
		std::uint32_t Right{};
	};

	std::uint32_t Build(std::span<const FBounds> InBounds, std::uint32_t InBegin, std::uint32_t InEnd,
	                    const std::function<void()>& InCheckCancellation);
	std::vector<FNode> Nodes;
	std::vector<std::uint32_t> Items;
};
} // namespace Hyperion
