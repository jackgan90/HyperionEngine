#include "Hyperion/Renderer/ModelPreparation.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace Hyperion
{
namespace
{
float Linear(float InValue)
{
	return InValue <= .04045f ? InValue / 12.92f : std::pow((InValue + .055f) / 1.055f, 2.4f);
}

float Srgb(float InValue)
{
	return InValue <= .0031308f ? InValue * 12.92f : 1.055f * std::pow(InValue, 1 / 2.4f) - .055f;
}

FTextureDesc Texture(const FModelImage& InImage, bool bInSrgb)
{
	FTextureDesc Result;
	Result.bSrgb = bInSrgb;
	Result.Mips.push_back({InImage.Width, InImage.Height, InImage.Rgba});
	while (Result.Mips.back().Width > 1 || Result.Mips.back().Height > 1)
	{
		const auto& Previous = Result.Mips.back();
		FTextureMip Mip;
		Mip.Width = std::max(1u, Previous.Width / 2);
		Mip.Height = std::max(1u, Previous.Height / 2);
		Mip.Rgba.resize(std::size_t(Mip.Width) * Mip.Height * 4);
		for (std::uint32_t Y = 0; Y < Mip.Height; ++Y)
		{
			for (std::uint32_t X = 0; X < Mip.Width; ++X)
			{
				for (std::uint32_t Channel = 0; Channel < 4; ++Channel)
				{
					float Sum{};
					unsigned Count{};
					for (auto Row = Y * Previous.Height / Mip.Height; Row < (Y + 1) * Previous.Height / Mip.Height;
					     ++Row)
					{
						for (auto Column = X * Previous.Width / Mip.Width;
						     Column < (X + 1) * Previous.Width / Mip.Width; ++Column)
						{
							float Value =
							    Previous.Rgba[(std::size_t(Row) * Previous.Width + Column) * 4 + Channel] / 255.f;
							Sum += bInSrgb && Channel < 3 ? Linear(Value) : Value;
							++Count;
						}
					}
					float Value = Sum / Count;
					if (bInSrgb && Channel < 3)
					{
						Value = Srgb(Value);
					}
					Mip.Rgba[(std::size_t(Y) * Mip.Width + X) * 4 + Channel] =
					    static_cast<std::uint8_t>(std::lround(std::clamp(Value, 0.f, 1.f) * 255));
				}
			}
		}
		Result.Mips.push_back(std::move(Mip));
	}
	return Result;
}

FSamplerDesc Sampler(const FModelSampler& InSampler)
{
	const auto Address = [](EWrapMode InMode)
	{
		return InMode == EWrapMode::Clamp    ? ERHIAddressMode::Clamp
		       : InMode == EWrapMode::Mirror ? ERHIAddressMode::Mirror
		                                     : ERHIAddressMode::Repeat;
	};
	return {Address(InSampler.WrapU),
	        Address(InSampler.WrapV),
	        InSampler.Min == ESamplerFilter::Linear || InSampler.Min == ESamplerFilter::LinearMipNearest ||
	            InSampler.Min == ESamplerFilter::LinearMipLinear,
	        InSampler.Mag == ESamplerFilter::Linear,
	        InSampler.Min >= ESamplerFilter::NearestMipLinear,
	        InSampler.Min >= ESamplerFilter::NearestMipNearest};
}

} // namespace

FPreparedModel PrepareModel(std::shared_ptr<const FModelAsset> InModel)
{
	ValidateModel(*InModel);
	FPreparedModel Result;
	Result.Source = InModel;
	Result.Bounds = ModelBounds(*InModel);
	Result.Textures.push_back({false, {{1, 1, {255, 255, 255, 255}}}});
	std::map<std::pair<std::int32_t, bool>, std::uint32_t> ImageCache;
	auto Materials = InModel->Materials;
	Materials.push_back(FModelMaterial{});
	for (const auto& Material : Materials)
	{
		FPreparedMaterial Prepared;
		Prepared.Material = Material;
		const std::array Views{Material.BaseColorTexture, Material.MetallicRoughnessTexture, Material.NormalTexture,
		                       Material.OcclusionTexture, Material.EmissiveTexture};
		for (std::size_t Slot = 0; Slot < Views.size(); ++Slot)
		{
			const auto& View = Views[Slot];
			Prepared.Samplers[Slot] = Sampler(View.Sampler >= 0 ? InModel->Samplers[View.Sampler] : FModelSampler{});
			if (View.Image < 0)
			{
				continue;
			}
			const auto Key = std::make_pair(View.Image, Slot == 0 || Slot == 4);
			auto It = ImageCache.find(Key);
			if (It == ImageCache.end())
			{
				const auto Index = static_cast<std::uint32_t>(Result.Textures.size());
				Result.Textures.push_back(Texture(InModel->Images[View.Image], Key.second));
				It = ImageCache.emplace(Key, Index).first;
			}
			Prepared.Textures[Slot] = It->second;
		}
		Result.Materials.push_back(std::move(Prepared));
	}
	for (auto Primitive : InModel->Primitives)
	{
		if (Primitive.Normals.empty() || Primitive.Tangents.empty())
		{
			GenerateMeshDirections(Primitive);
		}
		FPreparedPrimitive Prepared;
		Prepared.Indices = Primitive.Indices;
		const auto Count = Primitive.Positions.size() / 3;
		Prepared.Vertices.reserve(Count);
		FVec3 Minimum{Primitive.Positions[0], Primitive.Positions[1], Primitive.Positions[2]};
		FVec3 Maximum = Minimum;
		for (std::size_t Index = 0; Index < Count; ++Index)
		{
			FModelVertex Vertex;
			Vertex.Position = {Primitive.Positions[Index * 3], Primitive.Positions[Index * 3 + 1],
			                   Primitive.Positions[Index * 3 + 2]};
			Vertex.Normal = {Primitive.Normals[Index * 3], Primitive.Normals[Index * 3 + 1],
			                 Primitive.Normals[Index * 3 + 2]};
			Vertex.Tangent = {Primitive.Tangents[Index * 4], Primitive.Tangents[Index * 4 + 1],
			                  Primitive.Tangents[Index * 4 + 2], Primitive.Tangents[Index * 4 + 3]};
			Vertex.Color = Primitive.Colors.empty()
			                   ? FVec4{1, 1, 1, 1}
			                   : FVec4{Primitive.Colors[Index * 4], Primitive.Colors[Index * 4 + 1],
			                           Primitive.Colors[Index * 4 + 2], Primitive.Colors[Index * 4 + 3]};
			Vertex.Uv0 = Primitive.TexCoords0.empty()
			                 ? FVec2{}
			                 : FVec2{Primitive.TexCoords0[Index * 2], Primitive.TexCoords0[Index * 2 + 1]};
			Vertex.Uv1 = Primitive.TexCoords1.empty()
			                 ? FVec2{}
			                 : FVec2{Primitive.TexCoords1[Index * 2], Primitive.TexCoords1[Index * 2 + 1]};
			Minimum = {std::min(Minimum.X, Vertex.Position.X), std::min(Minimum.Y, Vertex.Position.Y),
			           std::min(Minimum.Z, Vertex.Position.Z)};
			Maximum = {std::max(Maximum.X, Vertex.Position.X), std::max(Maximum.Y, Vertex.Position.Y),
			           std::max(Maximum.Z, Vertex.Position.Z)};
			Prepared.Vertices.push_back(Vertex);
		}
		Prepared.Center = ScaleVector(Add(Minimum, Maximum), .5f);
		Result.Primitives.push_back(std::move(Prepared));
	}
	return Result;
}

} // namespace Hyperion
