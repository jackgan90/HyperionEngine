#include "Hyperion/Scene/SceneQuery.h"

namespace Hyperion
{
std::shared_ptr<const FSceneModelGeometry> PrepareSceneModelGeometry(const FModelAsset& InAsset,
                                                                     const std::function<void()>& InCheckCancellation)
{
	ValidateModel(InAsset);
	auto Geometry = std::make_shared<FSceneModelGeometry>();
	for (const auto& Primitive : InAsset.Primitives)
	{
		std::vector<FBounds> Bounds;
		Bounds.reserve(Primitive.Indices.size() / 3);
		for (std::size_t Index = 0; Index < Primitive.Indices.size(); Index += 3)
		{
			if (Index % 3072 == 0 && InCheckCancellation)
			{
				InCheckCancellation();
			}
			FBounds Triangle;
			for (std::size_t Corner = 0; Corner < 3; ++Corner)
			{
				const auto Vertex = std::size_t(Primitive.Indices[Index + Corner]) * 3;
				const FVec3 P{Primitive.Positions[Vertex], Primitive.Positions[Vertex + 1],
				              Primitive.Positions[Vertex + 2]};
				const FBounds Point{P, P, true};
				Triangle = Corner ? UnionBounds(Triangle, Point) : Point;
			}
			Bounds.push_back(Triangle);
		}
		Geometry->Primitives.emplace_back(Bounds, InCheckCancellation);
		Geometry->StorageBytes += Geometry->Primitives.back().GetStorageBytes();
	}
	return Geometry;
}
} // namespace Hyperion
