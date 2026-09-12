#include "Hyperion/Renderer/ModelPreparation.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace Hyperion
{
FPreparedModel PrepareModel(std::shared_ptr<const FModelAsset> InModel)
{
	ValidateModel(*InModel);
	FPreparedModel Result;
	Result.Source = InModel;
	Result.Bounds = ModelBounds(*InModel);
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
