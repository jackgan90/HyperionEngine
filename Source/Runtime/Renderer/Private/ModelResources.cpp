#include "Hyperion/Renderer/ModelPreparation.h"
#include "Hyperion/Renderer/RenderResources.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
FPipelineDesc ModelPipeline(const FShaderArtifact& InVertex, const FShaderArtifact& InPixel,
                            const FPreparedMaterial& InMaterial)
{
	FPipelineDesc Desc;
	Desc.Vertex = InVertex;
	Desc.Pixel = InPixel;
	Desc.bMaterialLayout = true;
	Desc.bSrgbTarget = true;
	Desc.bDepthTest = true;
	Desc.bDepthWrite = InMaterial.Material.AlphaMode != EAlphaMode::Blend;
	Desc.bAlphaBlend = InMaterial.Material.AlphaMode == EAlphaMode::Blend;
	Desc.bCullBack = !InMaterial.Material.bDoubleSided;
	Desc.Samplers = InMaterial.Samplers;
	Desc.Attributes = {{"POSITION", 0, EVertexFormat::Float3, offsetof(FModelVertex, Position)},
	                   {"NORMAL", 0, EVertexFormat::Float3, offsetof(FModelVertex, Normal)},
	                   {"TANGENT", 0, EVertexFormat::Float4, offsetof(FModelVertex, Tangent)},
	                   {"COLOR", 0, EVertexFormat::Float4, offsetof(FModelVertex, Color)},
	                   {"TEXCOORD", 0, EVertexFormat::Float2, offsetof(FModelVertex, Uv0)},
	                   {"TEXCOORD", 1, EVertexFormat::Float2, offsetof(FModelVertex, Uv1)}};
	return Desc;
}
} // namespace

FRenderResourceDesc PrepareModelResources(std::shared_ptr<const FModelAsset> InAsset, FShaderCompiler& InCompiler,
                                          EShaderFormat InFormat)
{
	const auto Prepared = PrepareModel(InAsset);
	const auto Vertex = InCompiler.Compile("Model.hlsl", "VSMain", EShaderStage::Vertex, InFormat);
	const auto Pixel = InCompiler.Compile("Model.hlsl", "PSMain", EShaderStage::Pixel, InFormat);
	FRenderResourceDesc Result;
	Result.Textures = Prepared.Textures;
	for (const auto& Primitive : Prepared.Primitives)
	{
		FRenderGeometryDesc Geometry;
		const auto Bytes = std::as_bytes(std::span(Primitive.Vertices));
		Geometry.Vertices.assign(Bytes.begin(), Bytes.end());
		Geometry.Indices = Primitive.Indices;
		Geometry.VertexStride = sizeof(FModelVertex);
		for (const auto& Item : Primitive.Vertices)
		{
			if (!Geometry.Bounds.bValid)
			{
				Geometry.Bounds = {Item.Position, Item.Position, true};
			}
			Geometry.Bounds.Minimum = {std::min(Geometry.Bounds.Minimum.X, Item.Position.X),
			                           std::min(Geometry.Bounds.Minimum.Y, Item.Position.Y),
			                           std::min(Geometry.Bounds.Minimum.Z, Item.Position.Z)};
			Geometry.Bounds.Maximum = {std::max(Geometry.Bounds.Maximum.X, Item.Position.X),
			                           std::max(Geometry.Bounds.Maximum.Y, Item.Position.Y),
			                           std::max(Geometry.Bounds.Maximum.Z, Item.Position.Z)};
		}
		const auto Index = static_cast<std::uint32_t>(Result.Geometries.size());
		const auto Material = InAsset->Primitives[Index].Material;
		Result.Sections.push_back({Index,
		                           Material < 0 ? static_cast<std::uint32_t>(Prepared.Materials.size() - 1)
		                                        : static_cast<std::uint32_t>(Material),
		                           0, static_cast<std::uint32_t>(Primitive.Indices.size())});
		Result.Geometries.push_back(std::move(Geometry));
	}
	for (const auto& Material : Prepared.Materials)
	{
		Result.Materials.push_back(
		    {ModelPipeline(Vertex, Pixel, Material), Material.Material, Material.Textures, false});
	}
	return Result;
}
} // namespace Hyperion
