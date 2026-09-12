#include "Hyperion/AssetImport/ModelImport.h"
#include "Hyperion/Materials/PbrMaterial.h"
#include <map>

namespace Hyperion
{
namespace
{
const std::array<std::string, 5> Roles{"BaseColor", "MetallicRoughness", "Normal", "Occlusion", "Emissive"};

FMaterialSampler MaterialSampler(const FModelSampler& InSampler)
{
	const auto Address = [](EWrapMode InMode)
	{
		return InMode == EWrapMode::Clamp    ? EMaterialAddressMode::Clamp
		       : InMode == EWrapMode::Mirror ? EMaterialAddressMode::Mirror
		                                     : EMaterialAddressMode::Repeat;
	};
	FMaterialSampler Result;
	Result.U = Address(InSampler.WrapU);
	Result.V = Address(InSampler.WrapV);
	Result.bMinLinear = InSampler.Min == ESamplerFilter::Linear || InSampler.Min == ESamplerFilter::LinearMipNearest ||
	                    InSampler.Min == ESamplerFilter::LinearMipLinear;
	Result.bMagLinear = InSampler.Mag == ESamplerFilter::Linear;
	Result.bMipLinear = InSampler.Min >= ESamplerFilter::NearestMipLinear;
	Result.MaxLod = InSampler.Min >= ESamplerFilter::NearestMipNearest ? Result.MaxLod : 0;
	return Result;
}

void SetFactors(FMaterialAsset& InAsset, const FModelMaterial& InMaterial)
{
	const auto Set = [&](std::string InName, FMaterialValue InValue)
	{
		InAsset.Values.push_back({std::move(InName), PersistMaterialValue(InValue)});
	};
	Set("Pbr.BaseColorFactor", FMaterialValue::Float(InMaterial.BaseColor));
	Set("Pbr.EmissiveFactor", FMaterialValue::Float(InMaterial.Emissive));
	Set("Pbr.NormalScale", FMaterialValue::Float(InMaterial.NormalScale));
	Set("Pbr.MetallicFactor", FMaterialValue::Float(InMaterial.Metallic));
	Set("Pbr.RoughnessFactor", FMaterialValue::Float(InMaterial.Roughness));
	Set("Pbr.OcclusionStrength", FMaterialValue::Float(InMaterial.OcclusionStrength));
	Set("Pbr.AlphaCutoff", FMaterialValue::Float(InMaterial.AlphaCutoff));
	Set("Pbr.AlphaMode", FMaterialValue::Uint(static_cast<std::uint32_t>(InMaterial.AlphaMode)));
	Set("Pbr.DoubleSided", FMaterialValue::Bool(InMaterial.bDoubleSided));
	Set("Pbr.Unlit", FMaterialValue::Bool(InMaterial.bUnlit));
	Set("Pbr.HasNormal", FMaterialValue::Bool(InMaterial.NormalTexture.Image >= 0));
}

struct FModelSplitter
{
	const FModelSource& Source;
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
		const std::array Views{InMaterial.BaseColorTexture, InMaterial.MetallicRoughnessTexture,
		                       InMaterial.NormalTexture, InMaterial.OcclusionTexture, InMaterial.EmissiveTexture};
		for (std::size_t Slot = 0; Slot < Views.size(); ++Slot)
		{
			FMaterialAssetValue Value;
			Value.Type = FMaterialParameterType::Resource(EMaterialValueKind::Texture2D);
			Value.Texture = Texture(Views[Slot].Image, Slot == 0 || Slot == 4);
			Asset.Values.push_back({Roles[Slot] + "Texture", std::move(Value)});
			const auto Sampler = Views[Slot].Sampler < 0 ? FModelSampler{} : Source.Samplers.at(Views[Slot].Sampler);
			Asset.Values.push_back(
			    {Roles[Slot] + "Sampler", PersistMaterialValue(FMaterialValue::FromSampler(MaterialSampler(Sampler)))});
			Asset.Values.push_back(
			    {"Pbr." + Roles[Slot] + "UvSet", PersistMaterialValue(FMaterialValue::Uint(Views[Slot].TexCoord))});
		}
		ValidateMaterialAsset(Asset);
		const auto Key = "material-" + std::to_string(InIndex);
		Result.Products.push_back({Key, std::make_shared<const FRecordDescriptor>(RecordType<FMaterialAsset>()),
		                           std::make_shared<const FMaterialAsset>(std::move(Asset))});
		Result.Model.MaterialSlots.push_back({"", "@" + Key, RecordType<FMaterialAsset>().Id, ""});
	}
};
} // namespace

FModelSourceAssets SplitModelSource(const FModelSource& InSource)
{
	ValidateModelSource(InSource);
	FModelSplitter Splitter{InSource};
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
	ValidateModel(Model);
	return std::move(Splitter.Result);
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

FModelSource ReadEmbeddedModelSource(const FAssetDocument& InDocument)
{
	if (InDocument.Header.TypeId != RecordType<FModelAsset>().Id || InDocument.Header.SchemaVersion != 1)
	{
		throw std::invalid_argument("Expected embedded model schema 1");
	}
	auto Node = InDocument.Object;
	std::get<FArchiveNode::FObject>(Node.Value).at("type") = WriteValue(RecordType<FModelSource>().Id);
	return *std::static_pointer_cast<FModelSource>(ReadRecord(RecordType<FModelSource>(), Node));
}
} // namespace Hyperion
