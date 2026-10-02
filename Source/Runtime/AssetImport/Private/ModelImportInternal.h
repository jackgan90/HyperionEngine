#pragma once
#include "Hyperion/AssetImport/ModelImport.h"
#include "Hyperion/Materials/PbrParameters.h"
#include <algorithm>
#include <array>

namespace Hyperion::Private
{
struct FModelTextureRole
{
	FTextureBinding FModelMaterial::* SourceMember;
	EPbrSemantic TextureSemantic;
	EPbrSemantic SamplerSemantic;
	EHyperionMaterialV1Field UvSemantic;
	EMaterialTextureEncoding Encoding;
	std::size_t OutputOrder;
};

using FModelTextureRoles = std::array<FModelTextureRole, 5>;

inline constexpr FModelTextureRoles ModelTextureRoles{
    {{&FModelMaterial::BaseColorTexture, EPbrSemantic::BaseColorTexture, EPbrSemantic::BaseColorSampler,
      EHyperionMaterialV1Field::BaseColorUv, EMaterialTextureEncoding::Srgb, 0},
     {&FModelMaterial::MetallicRoughnessTexture, EPbrSemantic::MetallicRoughnessTexture,
      EPbrSemantic::MetallicRoughnessSampler, EHyperionMaterialV1Field::MetallicRoughnessUv,
      EMaterialTextureEncoding::Linear, 1},
     {&FModelMaterial::NormalTexture, EPbrSemantic::NormalTexture, EPbrSemantic::NormalSampler,
      EHyperionMaterialV1Field::NormalUv, EMaterialTextureEncoding::Linear, 2},
     {&FModelMaterial::OcclusionTexture, EPbrSemantic::OcclusionTexture, EPbrSemantic::OcclusionSampler,
      EHyperionMaterialV1Field::OcclusionUv, EMaterialTextureEncoding::Linear, 3},
     {&FModelMaterial::EmissiveTexture, EPbrSemantic::EmissiveTexture, EPbrSemantic::EmissiveSampler,
      EHyperionMaterialV1Field::EmissiveUv, EMaterialTextureEncoding::Srgb, 4}}};

constexpr FModelTextureRoles OrderModelTextureRoles(FModelTextureRoles InRoles)
{
	std::sort(InRoles.begin(), InRoles.end(),
	          [](const auto& InLeft, const auto& InRight)
	          {
		          return InLeft.OutputOrder < InRight.OutputOrder;
	          });
	for (std::size_t Index = 0; Index < InRoles.size(); ++Index)
	{
		const auto& Role = InRoles[Index];
		if (Role.OutputOrder != Index || !Role.SourceMember ||
		    (Role.Encoding != EMaterialTextureEncoding::Linear && Role.Encoding != EMaterialTextureEncoding::Srgb))
		{
			throw std::invalid_argument("Invalid model texture role order, member or encoding");
		}
		for (std::size_t Previous = 0; Previous < Index; ++Previous)
		{
			const auto& Other = InRoles[Previous];
			if (Role.SourceMember == Other.SourceMember || Role.TextureSemantic == Other.TextureSemantic ||
			    Role.SamplerSemantic == Other.SamplerSemantic || Role.UvSemantic == Other.UvSemantic)
			{
				throw std::invalid_argument("Duplicate model texture role member or semantic");
			}
		}
	}
	return InRoles;
}

// Constant evaluation rejects invalid built-in rows before compiling conversion consumers.
inline constexpr auto OrderedModelTextureRoles = OrderModelTextureRoles(ModelTextureRoles);

FModelSourceAssets SplitModelSourceWithRoles(const FModelSource& InSource, FModelTextureRoles InRoles);
} // namespace Hyperion::Private
