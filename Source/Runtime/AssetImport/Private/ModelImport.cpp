#include "Hyperion/Materials/PbrMaterial.h"
#include "Hyperion/Materials/PbrParameters.h"
#include "ModelImportInternal.h"
#include <algorithm>
#include <map>

namespace Hyperion
{
namespace
{
std::string MaterialParameterName(const FMaterialAsset& InAsset, FMaterialSemanticId InSemantic)
{
	const auto Found = std::find_if(InAsset.Parameters.begin(), InAsset.Parameters.end(),
	                                [&](const auto& InParameter)
	                                {
		                                return FMaterialSemanticId(InParameter.Semantic) == InSemantic;
	                                });
	if (Found == InAsset.Parameters.end())
	{
		throw std::logic_error("PBR asset lacks a builtin parameter");
	}
	return Found->Name;
}

EMaterialAddressMode MaterialAddress(EWrapMode InMode)
{
	switch (InMode)
	{
		case EWrapMode::Repeat:
			return EMaterialAddressMode::Repeat;
		case EWrapMode::Clamp:
			return EMaterialAddressMode::Clamp;
		case EWrapMode::Mirror:
			return EMaterialAddressMode::Mirror;
	}
	throw std::invalid_argument("Invalid model sampler address mode");
}

bool MaterialMagLinear(ESamplerFilter InFilter)
{
	switch (InFilter)
	{
		case ESamplerFilter::Nearest:
			return false;
		case ESamplerFilter::Linear:
			return true;
		default:
			throw std::invalid_argument("Invalid model sampler magnification filter");
	}
}

void SetMaterialMinFilter(FMaterialSampler& OutSampler, ESamplerFilter InFilter)
{
	switch (InFilter)
	{
		case ESamplerFilter::Nearest:
			OutSampler.bMinLinear = false;
			OutSampler.bMipLinear = false;
			OutSampler.MaxLod = 0;
			break;
		case ESamplerFilter::Linear:
			OutSampler.bMinLinear = true;
			OutSampler.bMipLinear = false;
			OutSampler.MaxLod = 0;
			break;
		case ESamplerFilter::NearestMipNearest:
			OutSampler.bMinLinear = false;
			OutSampler.bMipLinear = false;
			break;
		case ESamplerFilter::LinearMipNearest:
			OutSampler.bMinLinear = true;
			OutSampler.bMipLinear = false;
			break;
		case ESamplerFilter::NearestMipLinear:
			OutSampler.bMinLinear = false;
			OutSampler.bMipLinear = true;
			break;
		case ESamplerFilter::LinearMipLinear:
			OutSampler.bMinLinear = true;
			OutSampler.bMipLinear = true;
			break;
		default:
			throw std::invalid_argument("Invalid model sampler minification filter");
	}
}

FMaterialSampler MaterialSampler(const FModelSampler& InSampler)
{
	FMaterialSampler Result;
	Result.U = MaterialAddress(InSampler.WrapU);
	Result.V = MaterialAddress(InSampler.WrapV);
	Result.bMagLinear = MaterialMagLinear(InSampler.Mag);
	SetMaterialMinFilter(Result, InSampler.Min);
	return Result;
}

void SetFactors(FMaterialAsset& InAsset, const FModelMaterial& InMaterial)
{
	const auto Set = [&](FMaterialSemanticId InSemantic, FMaterialValue InValue)
	{
		InAsset.Values.push_back({MaterialParameterName(InAsset, InSemantic), PersistMaterialValue(InValue)});
	};
	Set(EHyperionMaterialV1Field::BaseColor, FMaterialValue::Float(InMaterial.BaseColor));
	Set(EHyperionMaterialV1Field::Emissive, FMaterialValue::Float(InMaterial.Emissive));
	Set(EHyperionMaterialV1Field::NormalScale, FMaterialValue::Float(InMaterial.NormalScale));
	Set(EHyperionMaterialV1Field::Metallic, FMaterialValue::Float(InMaterial.Metallic));
	Set(EHyperionMaterialV1Field::Roughness, FMaterialValue::Float(InMaterial.Roughness));
	Set(EHyperionMaterialV1Field::OcclusionStrength, FMaterialValue::Float(InMaterial.OcclusionStrength));
	Set(EHyperionMaterialV1Field::AlphaCutoff, FMaterialValue::Float(InMaterial.AlphaCutoff));
	Set(EHyperionMaterialV1Field::AlphaMode, FMaterialValue::Uint(static_cast<std::uint32_t>(InMaterial.AlphaMode)));
	Set(EHyperionMaterialV1Field::bDoubleSided, FMaterialValue::Bool(InMaterial.bDoubleSided));
	Set(EHyperionMaterialV1Field::bUnlit, FMaterialValue::Bool(InMaterial.bUnlit));
	Set(EHyperionMaterialV1Field::bHasNormal, FMaterialValue::Bool(InMaterial.NormalTexture.Image >= 0));
}

struct FModelSplitter
{
	const FModelSource& Source;
	const Private::FModelTextureRoles& Roles;
	FModelSourceAssets Result;
	std::map<std::pair<std::int32_t, bool>, FAssetRef> Images;

	FAssetRef Texture(std::int32_t InImage, bool bInSrgb)
	{
		const auto Key = std::make_pair(InImage, InImage >= 0 && bInSrgb);
		if (const auto It = Images.find(Key); It != Images.end())
		{
			return It->second;
		}
		const auto Name = InImage < 0 ? "white" : "image-" + std::to_string(InImage) + (bInSrgb ? "-srgb" : "-linear");
		const FModelImage White{"White", 1, 1, {255, 255, 255, 255}};
		const auto& Image = InImage < 0 ? White : Source.Images.at(InImage);
		auto Asset = BuildTextureAsset(Image.Name,
		                               Key.second ? EMaterialTextureEncoding::Srgb : EMaterialTextureEncoding::Linear,
		                               {Image.Width, Image.Height, Image.Rgba});
		std::string SharedKey = InImage < 0 ? "builtin/white-rgba8-v1" : "";
		if (!Image.Source.empty())
		{
			SharedKey = "image/" + Image.Source + (bInSrgb ? "/srgb" : "/linear") + "/rgba8-full-mips-v1";
			// External source identity must not depend on the importing model's display name.
			Asset.Name = Image.Source.substr(Image.Source.find_last_of("/\\") + 1);
		}
		Result.Products.push_back({Name, std::make_shared<const FRecordDescriptor>(RecordType<FTextureAsset>()),
		                           std::make_shared<const FTextureAsset>(std::move(Asset)), std::move(SharedKey)});
		FAssetRef Reference{"", "@" + Name, RecordType<FTextureAsset>().Id, ""};
		Images.emplace(Key, Reference);
		return Reference;
	}

	void Material(const FModelMaterial& InMaterial, std::size_t InIndex)
	{
		const auto Queue = InMaterial.AlphaMode == EAlphaMode::Blend  ? EMaterialQueue::Transparent
		                   : InMaterial.AlphaMode == EAlphaMode::Mask ? EMaterialQueue::Masked
		                                                              : EMaterialQueue::Opaque;
		auto Asset = MakePbrMaterialAsset(InMaterial.Name, Queue, InMaterial.bDoubleSided, InMaterial.bUnlit);
		SetFactors(Asset, InMaterial);
		for (const auto& Role : Roles)
		{
			const auto& View = InMaterial.*Role.SourceMember;
			FMaterialAssetValue Value;
			Value.Type = FMaterialParameterType::Resource(EMaterialValueKind::Texture2D);
			Value.Texture = Texture(View.Image, Role.Encoding == EMaterialTextureEncoding::Srgb);
			Asset.Values.push_back({MaterialParameterName(Asset, Role.TextureSemantic), std::move(Value)});
			const auto Sampler = View.Sampler < 0 ? FModelSampler{} : Source.Samplers.at(View.Sampler);
			Asset.Values.push_back({MaterialParameterName(Asset, Role.SamplerSemantic),
			                        PersistMaterialValue(FMaterialValue::FromSampler(MaterialSampler(Sampler)))});
			Asset.Values.push_back({MaterialParameterName(Asset, Role.UvSemantic),
			                        PersistMaterialValue(FMaterialValue::Uint(View.TexCoord))});
		}
		ValidateMaterialAsset(Asset);
		const auto Key = "material-" + std::to_string(InIndex);
		Result.Products.push_back({Key, std::make_shared<const FRecordDescriptor>(RecordType<FMaterialAsset>()),
		                           std::make_shared<const FMaterialAsset>(std::move(Asset))});
		Result.Model.MaterialSlots.push_back({"", "@" + Key, RecordType<FMaterialAsset>().Id, ""});
	}
};
} // namespace

FModelSourceAssets Private::SplitModelSourceWithRoles(const FModelSource& InSource, FModelTextureRoles InRoles)
{
	ValidateModelSource(InSource);
	const auto Roles = OrderModelTextureRoles(std::move(InRoles));
	FModelSplitter Splitter{InSource, Roles};
	auto& Model = Splitter.Result.Model;
	Model.Name = InSource.Name;
	Model.Primitives = InSource.Primitives;
	Model.Nodes = InSource.Nodes;
	Model.Roots = InSource.Roots;
	Model.Diagnostics = InSource.Diagnostics;
	for (std::size_t Index = 0; Index < InSource.Materials.size(); ++Index)
	{
		Splitter.Material(InSource.Materials[Index], Index);
	}
	std::optional<std::int32_t> Default;
	for (auto& Primitive : Model.Primitives)
	{
		if (Primitive.Material < 0)
		{
			if (!Default)
			{
				Default = static_cast<std::int32_t>(Model.MaterialSlots.size());
				Splitter.Material({}, *Default);
			}
			Primitive.Material = *Default;
		}
	}
	AssignModelSubresourceIds(Model);
	ValidateModel(Model);
	return std::move(Splitter.Result);
}

FModelSourceAssets SplitModelSource(const FModelSource& InSource)
{
	return Private::SplitModelSourceWithRoles(InSource, Private::OrderedModelTextureRoles);
}

std::shared_ptr<FModelAsset> EmitModelSource(FAssetImportContext& InContext, const FModelSource& InSource)
{
	auto Split = SplitModelSource(InSource);
	for (auto& Product : Split.Products)
	{
		InContext.Emit(std::move(Product));
	}
	return std::make_shared<FModelAsset>(std::move(Split.Model));
}

} // namespace Hyperion
