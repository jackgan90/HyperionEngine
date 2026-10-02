#include "Hyperion/AssetImport/ModelImport.h"
#include "Hyperion/Assets/NativeAsset.h"
#include "Hyperion/Materials/PbrParameters.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <iostream>
#include <limits>

namespace Hyperion::Private
{
void CheckModelTextureRoleDescriptions(const FModelSource& InSource);
}

namespace
{
using namespace Hyperion;

FModelSource RoleSource()
{
	FModelSource Source;
	Source.Name = "Role fixture";
	FModelPrimitive Primitive;
	Primitive.Name = "Triangle";
	Primitive.Positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
	Primitive.Indices = {0, 1, 2};
	Primitive.Material = 0;
	GenerateMeshDirections(Primitive);
	Source.Primitives.push_back(std::move(Primitive));
	Source.Nodes = {{"Root", Identity(), {0}, {}}};
	Source.Roots = {0};
	for (std::uint8_t Index = 0; Index < 5; ++Index)
	{
		Source.Images.push_back({"Image " + std::to_string(Index),
		                         2,
		                         1,
		                         {static_cast<std::uint8_t>(Index * 20), 32, 64, 255, 255, 128, 0, 255},
		                         "roles/image-" + std::to_string(Index) + ".png"});
	}
	Source.Samplers = {
	    {EWrapMode::Repeat, EWrapMode::Clamp, ESamplerFilter::Nearest, ESamplerFilter::Linear},
	    {EWrapMode::Clamp, EWrapMode::Mirror, ESamplerFilter::Linear, ESamplerFilter::Nearest},
	    {EWrapMode::Mirror, EWrapMode::Repeat, ESamplerFilter::NearestMipNearest, ESamplerFilter::Linear},
	    {EWrapMode::Repeat, EWrapMode::Mirror, ESamplerFilter::LinearMipNearest, ESamplerFilter::Nearest},
	    {EWrapMode::Clamp, EWrapMode::Repeat, ESamplerFilter::NearestMipLinear, ESamplerFilter::Linear}};
	FModelMaterial Material;
	Material.Name = "Five roles";
	Material.BaseColor = {.25f, .5f, .75f, 1};
	Material.Emissive = {.1f, .2f, .3f};
	Material.Metallic = .4f;
	Material.Roughness = .6f;
	Material.NormalScale = .8f;
	Material.OcclusionStrength = .7f;
	Material.AlphaCutoff = .45f;
	Material.AlphaMode = EAlphaMode::Mask;
	Material.bDoubleSided = true;
	Material.bUnlit = true;
	Material.BaseColorTexture = {0, 0, 0};
	Material.MetallicRoughnessTexture = {1, 1, 1};
	Material.NormalTexture = {2, 2, 0};
	Material.OcclusionTexture = {3, 3, 1};
	Material.EmissiveTexture = {4, 4, 0};
	Source.Materials.push_back(std::move(Material));
	return Source;
}

const FAssetImportProduct& Product(const FModelSourceAssets& InSplit, std::string_view InKey)
{
	const auto Found = std::find_if(InSplit.Products.begin(), InSplit.Products.end(),
	                                [&](const auto& InProduct)
	                                {
		                                return InProduct.Key == InKey;
	                                });
	HYP_CHECK(Found != InSplit.Products.end());
	return *Found;
}

const FMaterialAsset& Material(const FModelSourceAssets& InSplit)
{
	return *std::static_pointer_cast<const FMaterialAsset>(Product(InSplit, "material-0").Object);
}

const FMaterialAssetValue& Value(const FMaterialAsset& InMaterial, FMaterialSemanticId InSemantic)
{
	const auto Parameter = std::find_if(InMaterial.Parameters.begin(), InMaterial.Parameters.end(),
	                                    [&](const auto& InParameter)
	                                    {
		                                    return FMaterialSemanticId(InParameter.Semantic) == InSemantic;
	                                    });
	HYP_CHECK(Parameter != InMaterial.Parameters.end());
	const auto Found = std::find_if(InMaterial.Values.begin(), InMaterial.Values.end(),
	                                [&](const auto& InValue)
	                                {
		                                return InValue.Name == Parameter->Name;
	                                });
	HYP_CHECK(Found != InMaterial.Values.end());
	return Found->Value;
}

void CheckRole(const FModelSourceAssets& InSplit, EPbrSemantic InTexture, EPbrSemantic InSampler,
               EHyperionMaterialV1Field InUv, std::string_view InKey, std::uint32_t InUvSet,
               EMaterialTextureEncoding InEncoding, const FMaterialSampler& InExpectedSampler)
{
	const auto& Asset = Material(InSplit);
	HYP_CHECK(Value(Asset, InTexture).Texture->Path == "@" + std::string(InKey));
	HYP_CHECK(Value(Asset, InSampler).Sampler == InExpectedSampler);
	HYP_CHECK(Value(Asset, InUv).Words == std::vector<std::uint32_t>{InUvSet});
	const auto& Texture = *std::static_pointer_cast<const FTextureAsset>(Product(InSplit, InKey).Object);
	HYP_CHECK(Texture.Encoding == InEncoding && Texture.Mips.size() == 2);
}

void CheckRoles(const FModelSource& InSource)
{
	const auto Split = SplitModelSource(InSource);
	HYP_CHECK(Split.Products.size() == 6 && Material(Split).Values.size() == 26);
	FMaterialSampler Expected;
	Expected.V = EMaterialAddressMode::Clamp;
	Expected.bMinLinear = false;
	Expected.bMipLinear = false;
	Expected.MaxLod = 0;
	CheckRole(Split, EPbrSemantic::BaseColorTexture, EPbrSemantic::BaseColorSampler,
	          EHyperionMaterialV1Field::BaseColorUv, "image-0-srgb", 0, EMaterialTextureEncoding::Srgb, Expected);
	Expected.U = EMaterialAddressMode::Clamp;
	Expected.V = EMaterialAddressMode::Mirror;
	Expected.bMinLinear = true;
	Expected.bMagLinear = false;
	CheckRole(Split, EPbrSemantic::MetallicRoughnessTexture, EPbrSemantic::MetallicRoughnessSampler,
	          EHyperionMaterialV1Field::MetallicRoughnessUv, "image-1-linear", 1, EMaterialTextureEncoding::Linear,
	          Expected);
	Expected.U = EMaterialAddressMode::Mirror;
	Expected.V = EMaterialAddressMode::Repeat;
	Expected.bMinLinear = false;
	Expected.bMagLinear = true;
	Expected.MaxLod = std::numeric_limits<float>::max();
	CheckRole(Split, EPbrSemantic::NormalTexture, EPbrSemantic::NormalSampler, EHyperionMaterialV1Field::NormalUv,
	          "image-2-linear", 0, EMaterialTextureEncoding::Linear, Expected);
	Expected.U = EMaterialAddressMode::Repeat;
	Expected.V = EMaterialAddressMode::Mirror;
	Expected.bMinLinear = true;
	Expected.bMagLinear = false;
	CheckRole(Split, EPbrSemantic::OcclusionTexture, EPbrSemantic::OcclusionSampler,
	          EHyperionMaterialV1Field::OcclusionUv, "image-3-linear", 1, EMaterialTextureEncoding::Linear, Expected);
	Expected.U = EMaterialAddressMode::Clamp;
	Expected.V = EMaterialAddressMode::Repeat;
	Expected.bMinLinear = false;
	Expected.bMagLinear = true;
	Expected.bMipLinear = true;
	CheckRole(Split, EPbrSemantic::EmissiveTexture, EPbrSemantic::EmissiveSampler, EHyperionMaterialV1Field::EmissiveUv,
	          "image-4-srgb", 0, EMaterialTextureEncoding::Srgb, Expected);
	HYP_CHECK(Value(Material(Split), EHyperionMaterialV1Field::bHasNormal).Words[0] == 1);
	for (std::size_t Index = 0; Index < InSource.Images.size(); ++Index)
	{
		const auto& Texture = *std::static_pointer_cast<const FTextureAsset>(Split.Products[Index].Object);
		HYP_CHECK(Texture.Mips[0].Bytes == InSource.Images[Index].Rgba);
		HYP_CHECK(Split.Products[Index].SharedKey ==
		          "image/roles/image-" + std::to_string(Index) + ".png/" +
		              (Texture.Encoding == EMaterialTextureEncoding::Srgb ? "srgb" : "linear") + "/rgba8-full-mips-v1");
	}
}

struct FFilterExpectation
{
	ESamplerFilter Filter;
	bool bMinLinear;
	bool bMipLinear;
	float MaxLod;
};

void CheckUvSelection(const FModelSource& InSource, FTextureBinding FModelMaterial::* InMember,
                      EHyperionMaterialV1Field InSemantic)
{
	for (const std::uint32_t UvSet : {0U, 1U})
	{
		auto Source = InSource;
		(Source.Materials[0].*InMember).TexCoord = UvSet;
		const auto Split = SplitModelSource(Source);
		HYP_CHECK(Value(Material(Split), InSemantic).Words == std::vector<std::uint32_t>{UvSet});
	}
}

void CheckUvs(const FModelSource& InSource)
{
	CheckUvSelection(InSource, &FModelMaterial::BaseColorTexture, EHyperionMaterialV1Field::BaseColorUv);
	CheckUvSelection(InSource, &FModelMaterial::MetallicRoughnessTexture,
	                 EHyperionMaterialV1Field::MetallicRoughnessUv);
	CheckUvSelection(InSource, &FModelMaterial::NormalTexture, EHyperionMaterialV1Field::NormalUv);
	CheckUvSelection(InSource, &FModelMaterial::OcclusionTexture, EHyperionMaterialV1Field::OcclusionUv);
	CheckUvSelection(InSource, &FModelMaterial::EmissiveTexture, EHyperionMaterialV1Field::EmissiveUv);
}

void CheckSamplers(const FModelSource& InSource)
{
	const std::array Filters{
	    FFilterExpectation{ESamplerFilter::Nearest, false, false, 0},
	    FFilterExpectation{ESamplerFilter::Linear, true, false, 0},
	    FFilterExpectation{ESamplerFilter::NearestMipNearest, false, false, std::numeric_limits<float>::max()},
	    FFilterExpectation{ESamplerFilter::LinearMipNearest, true, false, std::numeric_limits<float>::max()},
	    FFilterExpectation{ESamplerFilter::NearestMipLinear, false, true, std::numeric_limits<float>::max()},
	    FFilterExpectation{ESamplerFilter::LinearMipLinear, true, true, std::numeric_limits<float>::max()}};
	const std::array Addresses{std::pair{EWrapMode::Repeat, EMaterialAddressMode::Repeat},
	                           std::pair{EWrapMode::Clamp, EMaterialAddressMode::Clamp},
	                           std::pair{EWrapMode::Mirror, EMaterialAddressMode::Mirror}};
	for (const auto& Filter : Filters)
	{
		for (const auto Mag : {ESamplerFilter::Nearest, ESamplerFilter::Linear})
		{
			for (const auto& U : Addresses)
			{
				for (const auto& V : Addresses)
				{
					auto Source = InSource;
					Source.Samplers[0] = {U.first, V.first, Filter.Filter, Mag};
					FMaterialSampler Expected;
					Expected.U = U.second;
					Expected.V = V.second;
					Expected.bMinLinear = Filter.bMinLinear;
					Expected.bMagLinear = Mag == ESamplerFilter::Linear;
					Expected.bMipLinear = Filter.bMipLinear;
					Expected.MaxLod = Filter.MaxLod;
					const auto Split = SplitModelSource(Source);
					HYP_CHECK(Value(Material(Split), EPbrSemantic::BaseColorSampler).Sampler == Expected);
				}
			}
		}
	}
}

void CheckSharingAndFallback(const FModelSource& InSource)
{
	auto Source = InSource;
	auto& Authored = Source.Materials[0];
	Authored.MetallicRoughnessTexture.Image = 0;
	Authored.NormalTexture.Image = 0;
	Authored.OcclusionTexture.Image = 0;
	Authored.EmissiveTexture.Image = 0;
	Source.Materials.push_back(Authored);
	const auto Shared = SplitModelSource(Source);
	HYP_CHECK(Shared.Products.size() == 4);
	HYP_CHECK(Value(Material(Shared), EPbrSemantic::BaseColorTexture).Texture ==
	          Value(Material(Shared), EPbrSemantic::EmissiveTexture).Texture);
	HYP_CHECK(Value(Material(Shared), EPbrSemantic::NormalTexture).Texture ==
	          Value(Material(Shared), EPbrSemantic::OcclusionTexture).Texture);
	HYP_CHECK(Value(Material(Shared), EPbrSemantic::NormalTexture).Texture !=
	          Value(Material(Shared), EPbrSemantic::BaseColorTexture).Texture);
	Source.Materials.clear();
	Source.Primitives[0].Material = -1;
	const auto Fallback = SplitModelSource(Source);
	HYP_CHECK(Fallback.Products.size() == 2 && Fallback.Model.Primitives[0].Material == 0);
	HYP_CHECK(Fallback.Model.MaterialSlots[0].Path == "@material-0");
	HYP_CHECK(Product(Fallback, "white").SharedKey == "builtin/white-rgba8-v1");
	const auto& White = *std::static_pointer_cast<const FTextureAsset>(Product(Fallback, "white").Object);
	HYP_CHECK(White.Encoding == EMaterialTextureEncoding::Linear &&
	          White.Mips[0].Bytes == std::vector<std::uint8_t>({255, 255, 255, 255}));
	const auto& Asset = Material(Fallback);
	for (const auto Texture :
	     {EPbrSemantic::BaseColorTexture, EPbrSemantic::MetallicRoughnessTexture, EPbrSemantic::NormalTexture,
	      EPbrSemantic::OcclusionTexture, EPbrSemantic::EmissiveTexture})
	{
		HYP_CHECK(Value(Asset, Texture).Texture->Path == "@white");
	}
	for (const auto Sampler :
	     {EPbrSemantic::BaseColorSampler, EPbrSemantic::MetallicRoughnessSampler, EPbrSemantic::NormalSampler,
	      EPbrSemantic::OcclusionSampler, EPbrSemantic::EmissiveSampler})
	{
		HYP_CHECK(Value(Asset, Sampler).Sampler == FMaterialSampler{});
	}
	HYP_CHECK(Value(Asset, EHyperionMaterialV1Field::bHasNormal).Words[0] == 0);
}

void Rejects(const FModelSource& InSource)
{
	bool bRejected{};
	try
	{
		(void)SplitModelSource(InSource);
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

void CheckPartialFallback(const FModelSource& InSource)
{
	const auto Original = SplitModelSource(InSource);
	auto Source = InSource;
	Source.Materials[0].NormalTexture.Image = -1;
	Source.Materials[0].NormalTexture.TexCoord = 1;
	Source.Materials[0].EmissiveTexture.Sampler = -1;
	const auto Split = SplitModelSource(Source);
	const auto& Asset = Material(Split);
	HYP_CHECK(Value(Asset, EPbrSemantic::NormalTexture).Texture->Path == "@white");
	HYP_CHECK(Value(Asset, EPbrSemantic::NormalSampler).Sampler ==
	          Value(Material(Original), EPbrSemantic::NormalSampler).Sampler);
	HYP_CHECK(Value(Asset, EHyperionMaterialV1Field::NormalUv).Words[0] == 1);
	HYP_CHECK(Value(Asset, EHyperionMaterialV1Field::bHasNormal).Words[0] == 0);
	HYP_CHECK(Value(Asset, EPbrSemantic::EmissiveTexture).Texture->Path == "@image-4-srgb");
	HYP_CHECK(Value(Asset, EPbrSemantic::EmissiveSampler).Sampler == FMaterialSampler{});
}

void CheckRejections(const FModelSource& InSource)
{
	for (const auto Invalid : {-1, 6})
	{
		auto Source = InSource;
		Source.Samplers[0].Min = static_cast<ESamplerFilter>(Invalid);
		Rejects(Source);
	}
	for (const auto Invalid : {-1, 2})
	{
		auto Source = InSource;
		Source.Samplers[0].Mag = static_cast<ESamplerFilter>(Invalid);
		Rejects(Source);
	}
	for (const auto Invalid : {-1, 3})
	{
		auto Source = InSource;
		Source.Samplers[0].WrapU = static_cast<EWrapMode>(Invalid);
		Rejects(Source);
		Source.Samplers[0] = InSource.Samplers[0];
		Source.Samplers[0].WrapV = static_cast<EWrapMode>(Invalid);
		Rejects(Source);
	}
	for (const auto Invalid : {-2, 99})
	{
		auto Source = InSource;
		Source.Materials[0].BaseColorTexture.Image = Invalid;
		Rejects(Source);
		Source.Materials[0] = InSource.Materials[0];
		Source.Materials[0].BaseColorTexture.Sampler = Invalid;
		Rejects(Source);
	}
	auto Source = InSource;
	Source.Materials[0].EmissiveTexture.TexCoord = 2;
	Rejects(Source);
}

void CheckHistoricalOutput(const FModelSource& InSource)
{
	const auto Split = SplitModelSource(InSource);
	// Captured on the pre-refactor converter; these do not derive from role descriptions.
	HYP_CHECK(HashArchive(WriteValue(Split.Model)) ==
	          "7c04b4240fd935f75dbbb18c5062c615c9dd8d51f16b4a174e350e1eea2a1b80");
	const std::array Expected{
	    std::pair{"image-0-srgb", "0bddc0baf104225c92c62f8bf151587ee944d2a72b15d3c5a6920d450f86a852"},
	    std::pair{"image-1-linear", "4c87b6ce34ce3a4a85e79843fe3c2853e252c3b32a758f663a0c1f07b42704ab"},
	    std::pair{"image-2-linear", "b8df988487712f3b077fecd93cacc234ff1c1e9fbaaa54152f4aff9ed0d6a577"},
	    std::pair{"image-3-linear", "5ed2a6d5d10d88159defb8458c98abefdea409b9293202d32451d25d6ccb1a5f"},
	    std::pair{"image-4-srgb", "5cb783c08cfb5b8d6ee1701ea6366af034ac02ebfb429ce470b15a19d5f4d867"},
	    std::pair{"material-0", "99249f972c97f6b19c5f90d7d37e5b0ae72aaa365abb3588082e53dc02131bca"}};
	HYP_CHECK(Split.Products.size() == Expected.size());
	for (std::size_t Index = 0; Index < Expected.size(); ++Index)
	{
		const auto& Entry = Split.Products[Index];
		HYP_CHECK(Entry.Key == Expected[Index].first);
		HYP_CHECK(HashArchive(WriteRecord(*Entry.Type, Entry.Object.get())) == Expected[Index].second);
	}
}
} // namespace

int main()
{
	try
	{
		const auto Source = RoleSource();
		CheckRoles(Source);
		CheckUvs(Source);
		CheckSamplers(Source);
		CheckSharingAndFallback(Source);
		CheckPartialFallback(Source);
		CheckRejections(Source);
		CheckHistoricalOutput(Source);
		Hyperion::Private::CheckModelTextureRoleDescriptions(Source);
		std::cout << "Model roles, 108 sampler combinations, 120 role permutations, sharing and fallback passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
