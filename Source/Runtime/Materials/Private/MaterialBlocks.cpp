#include "Hyperion/Materials/MaterialBlocks.h"

namespace Hyperion
{
FStandardMaterialBlock GetStandardMaterialBlock(std::string_view InName)
{
	if (InName == "HyperionViewV1")
	{
		return {80,
		        {{"ViewProjection", "Engine.View.ViewProjection", 0, 4, 4},
		         {"CameraPosition", "Engine.View.CameraPosition", 64, 3}}};
	}
	if (InName == "HyperionObjectV1")
	{
		return {144,
		        {{"World", "Engine.Object.World", 0, 4, 4},
		         {"Normal", "Engine.Object.Normal", 64, 4, 4},
		         {"OrientationSign", "Engine.Object.OrientationSign", 128}}};
	}
	if (InName == "HyperionSceneV1")
	{
		return {48,
		        {{"MainLightDirection", "Engine.Scene.MainDirectionalLightDirection", 0, 3},
		         {"MainLightColor", "Engine.Scene.MainDirectionalLightColor", 16, 3},
		         {"AmbientColor", "Engine.Scene.AmbientColor", 32, 3}}};
	}
	if (InName == "HyperionMaterialV1")
	{
		return {96,
		        {{"BaseColor", "Pbr.BaseColorFactor", 0, 4},
		         {"Emissive", "Pbr.EmissiveFactor", 16, 3},
		         {"NormalScale", "Pbr.NormalScale", 28},
		         {"Metallic", "Pbr.MetallicFactor", 32},
		         {"Roughness", "Pbr.RoughnessFactor", 36},
		         {"OcclusionStrength", "Pbr.OcclusionStrength", 40},
		         {"AlphaCutoff", "Pbr.AlphaCutoff", 44},
		         {"BaseColorUv", "Pbr.BaseColorUvSet", 48, 1, 1, EMaterialScalar::Uint},
		         {"MetallicRoughnessUv", "Pbr.MetallicRoughnessUvSet", 52, 1, 1, EMaterialScalar::Uint},
		         {"NormalUv", "Pbr.NormalUvSet", 56, 1, 1, EMaterialScalar::Uint},
		         {"OcclusionUv", "Pbr.OcclusionUvSet", 60, 1, 1, EMaterialScalar::Uint},
		         {"EmissiveUv", "Pbr.EmissiveUvSet", 64, 1, 1, EMaterialScalar::Uint},
		         {"AlphaMode", "Pbr.AlphaMode", 68, 1, 1, EMaterialScalar::Uint},
		         {"bDoubleSided", "Pbr.DoubleSided", 72, 1, 1, EMaterialScalar::Bool},
		         {"bUnlit", "Pbr.Unlit", 76, 1, 1, EMaterialScalar::Bool},
		         {"bHasNormal", "Pbr.HasNormal", 80, 1, 1, EMaterialScalar::Bool}}};
	}
	return {};
}

std::vector<FMaterialParameterDeclaration> GetStandardMaterialBlockParameters(
    std::string_view InBlock, const FMaterialSemanticRegistry& InRegistry)
{
	std::vector<FMaterialParameterDeclaration> Result;
	for (const auto& Member : GetStandardMaterialBlock(InBlock).Members)
	{
		auto Parameter = DeclareMaterialSemantic(std::string(Member.Semantic), Member.Semantic, InRegistry);
		Parameter.Targets = {std::string(InBlock) + "." + std::string(Member.Name)};
		Result.push_back(std::move(Parameter));
	}
	return Result;
}
} // namespace Hyperion
