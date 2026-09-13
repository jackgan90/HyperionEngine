#pragma once
#include "Hyperion/Renderer/LocalLights.h"
#include "Hyperion/Renderer/RenderPrimitive.h"

namespace Hyperion
{
// Matches FClusterLight in ClusteredLighting.hlsli. Four float4 records avoid ABI-specific bool packing.
struct FClusterLightData
{
	FVec4 PositionRange;
	FVec4 RadianceType;
	FVec4 DirectionInner;
	FVec4 Outer;
};

struct FClusterHeader
{
	std::uint32_t Offset{};
	std::uint32_t Count{};
};

struct FClusterStatistics
{
	std::size_t Cells{};
	std::size_t Occupied{};
	std::size_t References{};
	std::size_t MaximumLights{};
	std::size_t BufferBytes{};
	double BuildMilliseconds{};
	bool bRebuilt{};
};

struct FClusterLightFrame
{
	FMaterialParameterValues Parameters;
	FClusterStatistics Statistics;
};

// Render-owned single-view cache. Frames retain immutable sources independently of this cache.
class FClusteredLights
{
public:
	FClusterLightFrame Build(const FRenderView& InView, std::span<const FLocalLight> InLights);
	void Reset();

private:
	std::vector<float> AssignmentKey;
	std::vector<FClusterLightData> Attributes;
	std::shared_ptr<const FMaterialReadBufferSource> LightBuffer;
	std::shared_ptr<const FMaterialReadBufferSource> HeaderBuffer;
	std::shared_ptr<const FMaterialReadBufferSource> IndexBuffer;
	FClusterStatistics Statistics;
};

FMaterialParameterValues DefaultClusterParameters();
} // namespace Hyperion
