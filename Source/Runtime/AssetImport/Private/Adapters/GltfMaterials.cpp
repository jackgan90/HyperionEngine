#include "GltfImportInternal.h"

namespace Hyperion::Private
{
namespace
{
EWrapMode Wrap(cgltf_wrap_mode InWrap)
{
	if (InWrap == cgltf_wrap_mode_clamp_to_edge)
	{
		return EWrapMode::Clamp;
	}
	if (InWrap == cgltf_wrap_mode_mirrored_repeat)
	{
		return EWrapMode::Mirror;
	}
	return EWrapMode::Repeat;
}

ESamplerFilter Filter(cgltf_filter_type InFilter, bool bInMagnification)
{
	switch (InFilter)
	{
		case cgltf_filter_type_nearest:
			return ESamplerFilter::Nearest;
		case cgltf_filter_type_linear:
			return ESamplerFilter::Linear;
		case cgltf_filter_type_nearest_mipmap_nearest:
			return ESamplerFilter::NearestMipNearest;
		case cgltf_filter_type_linear_mipmap_nearest:
			return ESamplerFilter::LinearMipNearest;
		case cgltf_filter_type_nearest_mipmap_linear:
			return ESamplerFilter::NearestMipLinear;
		default:
			return bInMagnification ? ESamplerFilter::Linear : ESamplerFilter::LinearMipLinear;
	}
}

FTextureBinding Binding(const cgltf_texture_view& InView, const cgltf_data& InData)
{
	FTextureBinding Result;
	if (!InView.texture)
	{
		return Result;
	}
	Require(InView.texture->image != nullptr, "texture has no supported image fallback");
	Result.Image = static_cast<std::int32_t>(InView.texture->image - InData.images);
	Result.Sampler =
	    InView.texture->sampler ? static_cast<std::int32_t>(InView.texture->sampler - InData.samplers) : -1;
	Require(InView.texcoord >= 0 && InView.texcoord <= 1, "only TEXCOORD_0 and TEXCOORD_1 are supported");
	Result.TexCoord = static_cast<std::uint32_t>(InView.texcoord);
	return Result;
}

} // namespace

void LoadMaterials(const cgltf_data& InData, FModelAsset& OutModel)
{
	for (std::size_t Index = 0; Index < InData.samplers_count; ++Index)
	{
		const auto& Sampler = InData.samplers[Index];
		OutModel.Samplers.push_back({Wrap(Sampler.wrap_s), Wrap(Sampler.wrap_t), Filter(Sampler.min_filter, false),
		                             Filter(Sampler.mag_filter, true)});
	}
	for (std::size_t Index = 0; Index < InData.materials_count; ++Index)
	{
		const auto& Source = InData.materials[Index];
		FModelMaterial Material;
		Material.Name = Name(Source.name);
		if (Source.has_pbr_metallic_roughness)
		{
			const auto& Pbr = Source.pbr_metallic_roughness;
			Material.BaseColor = {Pbr.base_color_factor[0], Pbr.base_color_factor[1], Pbr.base_color_factor[2],
			                      Pbr.base_color_factor[3]};
			Material.Metallic = Pbr.metallic_factor;
			Material.Roughness = Pbr.roughness_factor;
			Material.BaseColorTexture = Binding(Pbr.base_color_texture, InData);
			Material.MetallicRoughnessTexture = Binding(Pbr.metallic_roughness_texture, InData);
		}
		Material.Emissive = {Source.emissive_factor[0], Source.emissive_factor[1], Source.emissive_factor[2]};
		Material.NormalTexture = Binding(Source.normal_texture, InData);
		Material.OcclusionTexture = Binding(Source.occlusion_texture, InData);
		Material.EmissiveTexture = Binding(Source.emissive_texture, InData);
		Material.NormalScale = Source.normal_texture.scale;
		Material.OcclusionStrength = Source.occlusion_texture.scale;
		Material.AlphaCutoff = Source.alpha_cutoff;
		Material.AlphaMode = Source.alpha_mode == cgltf_alpha_mode_mask    ? EAlphaMode::Mask
		                     : Source.alpha_mode == cgltf_alpha_mode_blend ? EAlphaMode::Blend
		                                                                   : EAlphaMode::Opaque;
		Material.bDoubleSided = Source.double_sided != 0;
		Material.bUnlit = Source.unlit != 0;
		OutModel.Materials.push_back(std::move(Material));
	}
}

} // namespace Hyperion::Private
