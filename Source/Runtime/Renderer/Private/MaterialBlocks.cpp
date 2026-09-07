#include "Hyperion/Renderer/MaterialBlocks.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
namespace
{
struct FBlockMember
{
	std::string_view Name;
	std::string_view Semantic;
	std::uint32_t Offset{};
	std::uint32_t Columns = 1;
	std::uint32_t Rows = 1;
	EShaderScalar Scalar = EShaderScalar::Float;
};

struct FBlock
{
	std::uint32_t Size{};
	std::vector<FBlockMember> Members;
};

FBlock DescribeBlock(std::string_view InName)
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
		         {"BaseColorUv", "Pbr.BaseColorUvSet", 48, 1, 1, EShaderScalar::Uint},
		         {"MetallicRoughnessUv", "Pbr.MetallicRoughnessUvSet", 52, 1, 1, EShaderScalar::Uint},
		         {"NormalUv", "Pbr.NormalUvSet", 56, 1, 1, EShaderScalar::Uint},
		         {"OcclusionUv", "Pbr.OcclusionUvSet", 60, 1, 1, EShaderScalar::Uint},
		         {"EmissiveUv", "Pbr.EmissiveUvSet", 64, 1, 1, EShaderScalar::Uint},
		         {"AlphaMode", "Pbr.AlphaMode", 68, 1, 1, EShaderScalar::Uint},
		         {"bDoubleSided", "Pbr.DoubleSided", 72, 1, 1, EShaderScalar::Bool},
		         {"bUnlit", "Pbr.Unlit", 76, 1, 1, EShaderScalar::Bool},
		         {"bHasNormal", "Pbr.HasNormal", 80, 1, 1, EShaderScalar::Bool}}};
	}
	return {};
}

FShaderMember Layout(const FBlockMember& InMember)
{
	FShaderMember Result;
	Result.Name = InMember.Name;
	Result.Offset = InMember.Offset;
	Result.Columns = InMember.Columns;
	Result.Rows = InMember.Rows;
	Result.Scalar = InMember.Scalar;
	Result.MatrixStride = InMember.Rows > 1 ? 16 : 0;
	Result.Size = InMember.Rows > 1 ? InMember.Columns * 16 : InMember.Columns * 4;
	return Result;
}
} // namespace

bool NormalizeStandardMaterialBlock(FShaderBinding& InBinding)
{
	const FBlock Block = DescribeBlock(InBinding.Name);
	if (Block.Size == 0)
	{
		return false;
	}
	if (InBinding.Kind != EBindingKind::UniformBuffer || InBinding.Count != 1 || InBinding.ByteSize > Block.Size)
	{
		throw std::invalid_argument("Invalid standard material block extent: " + InBinding.Name);
	}
	for (const auto& Reflected : InBinding.Members)
	{
		if (!Reflected.bActive)
		{
			continue;
		}
		const auto Expected = std::find_if(Block.Members.begin(), Block.Members.end(),
		                                   [&](const FBlockMember& InMember)
		                                   {
			                                   return InMember.Name == Reflected.Name;
		                                   });
		if (Expected == Block.Members.end())
		{
			throw std::invalid_argument("Unknown active standard block member: " + Reflected.Name);
		}
		const FShaderMember Required = Layout(*Expected);
		if (Reflected.Kind != Required.Kind || Reflected.Scalar != Required.Scalar ||
		    Reflected.Offset != Required.Offset || Reflected.Rows != Required.Rows ||
		    Reflected.Columns != Required.Columns || Reflected.MatrixStride != Required.MatrixStride ||
		    Reflected.bRowMajor != Required.bRowMajor)
		{
			throw std::invalid_argument("Standard material block ABI mismatch: " + InBinding.Name + "." +
			                            Reflected.Name);
		}
	}
	InBinding.ByteSize = Block.Size;
	InBinding.Members.clear();
	// A standard block is one complete input contract, including fields unused by a particular stage.
	for (const auto& Member : Block.Members)
	{
		InBinding.Members.push_back(Layout(Member));
	}
	return true;
}

std::vector<FMaterialParameterDeclaration> GetStandardMaterialBlockParameters(
    std::string_view InBlock, const FMaterialSemanticRegistry& InRegistry)
{
	std::vector<FMaterialParameterDeclaration> Result;
	for (const auto& Member : DescribeBlock(InBlock).Members)
	{
		auto Parameter = DeclareMaterialSemantic(std::string(Member.Semantic), Member.Semantic, InRegistry);
		Parameter.Targets = {std::string(InBlock) + "." + std::string(Member.Name)};
		Result.push_back(std::move(Parameter));
	}
	return Result;
}
} // namespace Hyperion
