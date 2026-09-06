#include "Hyperion/Renderer/ModelRenderer.h"
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

FTextureDesc Texture(const FModelImage& InImage, bool InSrgb)
{
	FTextureDesc Result;
	Result.Srgb = InSrgb;
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
							Sum += InSrgb && Channel < 3 ? Linear(Value) : Value;
							++Count;
						}
					}
					float Value = Sum / Count;
					if (InSrgb && Channel < 3)
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

struct FModelConstants
{
	FMat4 World;
	FMat4 ViewProjection;
	FMat4 Normal;
	FVec4 Camera;
	FVec4 BaseColor;
	FVec4 EmissiveAndNormal;
	FVec4 Pbr;
	FVec4 Modes;
	FVec4 UvSets;
	FVec4 Extra;
	std::array<std::byte, 208> Padding{};
};

static_assert(sizeof(FModelConstants) == 512);
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

FModelRenderer::FModelRenderer(IRHIDevice& InDevice, const FPreparedModel& InModel, const FShaderArtifact& InVertex,
                               const FShaderArtifact& InPixel)
    : Device(InDevice), Source(InModel.Source), Instances(ModelInstances(*Source)), Materials(InModel.Materials)
{
	Textures = Device.CreateTexturesAsync(InModel.Textures);
	for (const auto& Primitive : InModel.Primitives)
	{
		Vertices.push_back(Device.CreateBuffer(std::as_bytes(std::span(Primitive.Vertices))));
		Indices.push_back(Device.CreateBuffer(std::as_bytes(std::span(Primitive.Indices))));
		Centers.push_back(Primitive.Center);
	}
	for (const auto& Prepared : Materials)
	{
		FPipelineDesc Desc;
		Desc.Vertex = InVertex;
		Desc.Pixel = InPixel;
		Desc.MaterialLayout = true;
		Desc.SrgbTarget = true;
		Desc.DepthTest = true;
		Desc.DepthWrite = Prepared.Material.AlphaMode != EAlphaMode::Blend;
		Desc.AlphaBlend = Prepared.Material.AlphaMode == EAlphaMode::Blend;
		Desc.CullBack = !Prepared.Material.DoubleSided;
		Desc.Samplers = Prepared.Samplers;
		Desc.Attributes = {{"POSITION", 0, EVertexFormat::Float3, offsetof(FModelVertex, Position)},
		                   {"NORMAL", 0, EVertexFormat::Float3, offsetof(FModelVertex, Normal)},
		                   {"TANGENT", 0, EVertexFormat::Float4, offsetof(FModelVertex, Tangent)},
		                   {"COLOR", 0, EVertexFormat::Float4, offsetof(FModelVertex, Color)},
		                   {"TEXCOORD", 0, EVertexFormat::Float2, offsetof(FModelVertex, Uv0)},
		                   {"TEXCOORD", 1, EVertexFormat::Float2, offsetof(FModelVertex, Uv1)}};
		std::array<FPipeline, 2> Pair;
		Pair[0] = Device.CreatePipeline(Desc);
		Desc.FrontCounterClockwise = false;
		Pair[1] = Device.CreatePipeline(Desc);
		Pipelines.push_back(std::move(Pair));
	}
}

bool FModelRenderer::Ready()
{
	return Device.TexturesReady(Textures);
}

std::vector<FDrawPacket> FModelRenderer::Draws(const FMat4& InViewProjection, FVec3 InEye, FSize InSize)
{
	const auto MaterialIndex = [&](const FModelInstance& InInstance)
	{
		const auto Index = Source->Primitives[InInstance.Primitive].Material;
		return Index < 0 ? Materials.size() - 1 : std::size_t(Index);
	};

	struct FOrderedInstance
	{
		FModelInstance Instance;
		bool Blend{};
		float Depth{};
	};

	std::vector<FOrderedInstance> Ordered;
	Ordered.reserve(Instances.size());
	for (const auto& Instance : Instances)
	{
		FOrderedInstance Item{Instance, Materials[MaterialIndex(Instance)].Material.AlphaMode == EAlphaMode::Blend};
		if (Item.Blend)
		{
			const auto Center = Centers[Instance.Primitive];
			const auto Clip = Transform(InViewProjection, Transform(Instance.World, {Center.X, Center.Y, Center.Z, 1}));
			Item.Depth = Clip.W > 0 ? Clip.Z / Clip.W : std::numeric_limits<float>::lowest();
			if (!std::isfinite(Item.Depth))
			{
				Item.Depth = std::numeric_limits<float>::lowest();
			}
		}
		Ordered.push_back(Item);
	}
	std::stable_sort(Ordered.begin(), Ordered.end(),
	                 [](const FOrderedInstance& InA, const FOrderedInstance& InB)
	                 {
		                 return InA.Blend != InB.Blend ? !InA.Blend : InA.Blend && InA.Depth > InB.Depth;
	                 });
	std::vector<FDrawPacket> Draws;
	Draws.reserve(Ordered.size());
	std::vector<FModelConstants> ConstantData;
	ConstantData.reserve(Ordered.size());
	for (const auto& Item : Ordered)
	{
		const auto& Instance = Item.Instance;
		const auto Index = MaterialIndex(Instance);
		const auto& Prepared = Materials[Index];
		const auto& Material = Prepared.Material;
		const bool Mirrored = Determinant(Instance.World) < 0;
		FModelConstants Constants{};
		Constants.World = Instance.World;
		Constants.ViewProjection = InViewProjection;
		Constants.Normal = NormalMatrix(Instance.World);
		Constants.Camera = {InEye.X, InEye.Y, InEye.Z, 1};
		Constants.BaseColor = Material.BaseColor;
		Constants.EmissiveAndNormal = {Material.Emissive.X, Material.Emissive.Y, Material.Emissive.Z,
		                               Material.NormalScale};
		Constants.Pbr = {Material.Metallic, Material.Roughness, Material.OcclusionStrength, Material.AlphaCutoff};
		Constants.Modes = {static_cast<float>(Material.AlphaMode), Material.DoubleSided ? 1.f : 0.f,
		                   Material.Unlit ? 1.f : 0.f, Mirrored ? -1.f : 1.f};
		Constants.UvSets = {float(Material.BaseColorTexture.TexCoord),
		                    float(Material.MetallicRoughnessTexture.TexCoord), float(Material.NormalTexture.TexCoord),
		                    float(Material.OcclusionTexture.TexCoord)};
		Constants.Extra = {float(Material.EmissiveTexture.TexCoord), Material.NormalTexture.Image >= 0 ? 1.f : 0.f, 0,
		                   0};
		FDrawPacket Draw;
		Draw.Pipeline = Pipelines[Index][Mirrored ? 1 : 0];
		Draw.Vertices = Vertices[Instance.Primitive];
		Draw.Indices = Indices[Instance.Primitive];
		Draw.VertexStride = sizeof(FModelVertex);
		Draw.IndexCount = static_cast<std::uint32_t>(Source->Primitives[Instance.Primitive].Indices.size());
		Draw.MaterialConstantOffset = ConstantData.size() * sizeof(FModelConstants);
		ConstantData.push_back(Constants);
		for (std::size_t Slot = 0; Slot < 5; ++Slot)
		{
			Draw.MaterialTextures[Slot] = Textures[Prepared.Textures[Slot]];
		}
		Draw.Scissor = {0, 0, static_cast<std::int32_t>(InSize.Width), static_cast<std::int32_t>(InSize.Height)};
		Draws.push_back(std::move(Draw));
	}
	if (!ConstantData.empty())
	{
		const auto Buffer = Device.CreateBuffer(std::as_bytes(std::span(ConstantData)));
		for (auto& Draw : Draws)
		{
			Draw.MaterialConstants = Buffer;
		}
	}
	return Draws;
}
} // namespace Hyperion
