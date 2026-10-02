#include "LightingWireContracts.h"
#include "DirectionalLighting.h"
#include "Hyperion/Materials/Lighting/ClusterParameters.h"
#include "Hyperion/Materials/ShaderWireLayout.h"
#include "Hyperion/Renderer/ClusteredLights.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
namespace
{
const FEngineMaterialResource& FindResource(const FShaderParameterContractSet& InContracts,
                                            const FMaterialSemanticId& InSemantic)
{
	const auto Found = std::find_if(InContracts.Resources.begin(), InContracts.Resources.end(),
	                                [&](const auto& InResource)
	                                {
		                                return InResource.second.Semantic == InSemantic;
	                                });
	if (Found == InContracts.Resources.end())
	{
		throw std::invalid_argument("Missing lighting wire resource: " + std::string(InSemantic.GetName()));
	}
	return Found->second;
}
} // namespace

const FLightingWireStrides& GetLightingWireStrides()
{
	static const FLightingWireStrides Strides = []
	{
		const auto Cluster = GetClusterShaderContracts();
		const auto Directional = GetSceneLightingShaderContracts();
		const std::array ClusterMembers{HYP_SHADER_WIRE_MEMBER(FClusterLightData, PositionRange),
		                                HYP_SHADER_WIRE_MEMBER(FClusterLightData, RadianceType),
		                                HYP_SHADER_WIRE_MEMBER(FClusterLightData, DirectionInner),
		                                HYP_SHADER_WIRE_MEMBER(FClusterLightData, Outer)};
		const std::array DirectionalMembers{HYP_SHADER_WIRE_MEMBER(FDirectionalLightData, Direction),
		                                    HYP_SHADER_WIRE_MEMBER(FDirectionalLightData, Radiance)};
		return FLightingWireStrides{
		    ValidateShaderWireRecord(FindResource(*Cluster, EClusterSemantic::ClusterLights), ClusterMembers),
		    ValidateShaderWireUintPair(FindResource(*Cluster, EClusterSemantic::ClusterHeaders),
		                               HYP_SHADER_WIRE_MEMBER(FClusterHeader, Offset),
		                               HYP_SHADER_WIRE_MEMBER(FClusterHeader, Count)),
		    ValidateShaderWireScalar<std::uint32_t>(FindResource(*Cluster, EClusterSemantic::ClusterIndices)),
		    ValidateShaderWireRecord(FindResource(*Directional, ESceneLightingSemantic::DirectionalLights),
		                             DirectionalMembers)};
	}();
	return Strides;
}
} // namespace Hyperion
