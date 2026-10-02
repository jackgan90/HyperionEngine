#pragma once
#include <cstdint>

namespace Hyperion
{
struct FLightingWireStrides
{
	std::uint32_t ClusterLight{};
	std::uint32_t ClusterHeader{};
	std::uint32_t ClusterIndex{};
	std::uint32_t DirectionalLight{};
};

const FLightingWireStrides& GetLightingWireStrides();
} // namespace Hyperion
