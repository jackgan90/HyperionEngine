#include "Hyperion/Scene/ModelSource.h"

namespace Hyperion
{
namespace
{
void Require(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(InMessage);
	}
}
} // namespace

void ValidateModelSource(const FModelSource& InModel)
{
	FModelAsset Geometry;
	Geometry.Name = InModel.Name;
	Geometry.Primitives = InModel.Primitives;
	Geometry.Nodes = InModel.Nodes;
	Geometry.Roots = InModel.Roots;
	Geometry.MaterialSlots.resize(InModel.Materials.size() + 1, {"", "source-material", "hyperion.materialasset", ""});
	for (auto& Primitive : Geometry.Primitives)
	{
		Require(Primitive.Material >= -1 &&
		            (Primitive.Material < 0 || std::size_t(Primitive.Material) < InModel.Materials.size()),
		        "Invalid source material reference");
		if (Primitive.Material == -1)
		{
			Primitive.Material = static_cast<std::int32_t>(InModel.Materials.size());
		}
	}
	ValidateModel(Geometry);
	for (const auto& Image : InModel.Images)
	{
		Require(Image.Width && Image.Height && Image.Width <= 16384 && Image.Height <= 16384 &&
		            std::uint64_t(Image.Width) * Image.Height * 4 == Image.Rgba.size(),
		        "Invalid model image");
	}
	for (const auto& Sampler : InModel.Samplers)
	{
		Require(Sampler.WrapU >= EWrapMode::Repeat && Sampler.WrapU <= EWrapMode::Mirror &&
		            Sampler.WrapV >= EWrapMode::Repeat && Sampler.WrapV <= EWrapMode::Mirror &&
		            Sampler.Min >= ESamplerFilter::Nearest && Sampler.Min <= ESamplerFilter::LinearMipLinear &&
		            Sampler.Mag >= ESamplerFilter::Nearest && Sampler.Mag <= ESamplerFilter::Linear,
		        "Invalid sampler enum");
	}
	for (const auto& Material : InModel.Materials)
	{
		Require(Material.AlphaMode >= EAlphaMode::Opaque && Material.AlphaMode <= EAlphaMode::Blend,
		        "Invalid alpha mode");
		for (float Value : {Material.BaseColor.X, Material.BaseColor.Y, Material.BaseColor.Z, Material.BaseColor.W,
		                    Material.Emissive.X, Material.Emissive.Y, Material.Emissive.Z, Material.Metallic,
		                    Material.Roughness, Material.NormalScale, Material.OcclusionStrength, Material.AlphaCutoff})
		{
			Require(std::isfinite(Value), "Non-finite material value");
		}
		Require(Material.Metallic >= 0 && Material.Metallic <= 1 && Material.Roughness >= 0 && Material.Roughness <= 1,
		        "Invalid PBR factors");
		for (const auto& Binding : {Material.BaseColorTexture, Material.MetallicRoughnessTexture,
		                            Material.NormalTexture, Material.OcclusionTexture, Material.EmissiveTexture})
		{
			Require(Binding.Image >= -1 && (Binding.Image < 0 || std::size_t(Binding.Image) < InModel.Images.size()) &&
			            Binding.TexCoord <= 1,
			        "Invalid texture reference or UV set");
			Require(Binding.Sampler >= -1 &&
			            (Binding.Sampler < 0 || std::size_t(Binding.Sampler) < InModel.Samplers.size()),
			        "Invalid sampler reference");
		}
	}
}
} // namespace Hyperion
