#include "Hyperion/Renderer/RenderResources.h"
#include "ModelMaterials.h"
#include <algorithm>

namespace Hyperion
{
FRenderResourceDesc PrepareModelResources(std::shared_ptr<const FModelAsset> InAsset, FShaderCompiler& InCompiler,
                                          EShaderFormat InFormat)
{
	const auto Prepared = PrepareModel(InAsset);
	FRenderResourceDesc Result;
	Result.Materials = PrepareModelMaterials(Prepared, InCompiler, InFormat);
	for (const auto& Primitive : Prepared.Primitives)
	{
		FRenderGeometryDesc Geometry;
		const auto Bytes = std::as_bytes(std::span(Primitive.Vertices));
		Geometry.Vertices.assign(Bytes.begin(), Bytes.end());
		Geometry.Indices = Primitive.Indices;
		Geometry.VertexStride = sizeof(FModelVertex);
		Geometry.Attributes = ModelVertexAttributes();
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
	return Result;
}
} // namespace Hyperion
