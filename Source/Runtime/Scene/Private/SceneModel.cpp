#include "Hyperion/Scene/Scene.h"

namespace Hyperion
{
std::shared_ptr<const FSceneModelData> PrepareSceneModel(
    std::shared_ptr<const FModelAsset> InAsset, std::vector<std::shared_ptr<const FMaterialAssetData>> InMaterials)
{
	if (!InAsset)
	{
		throw std::invalid_argument("Scene model requires an asset");
	}
	auto Data = std::make_shared<FSceneModelData>();
	Data->Asset = std::move(InAsset);
	Data->Materials = std::move(InMaterials);
	Data->Instances = ModelInstances(*Data->Asset);
	for (const auto& Primitive : Data->Asset->Primitives)
	{
		FBounds Bounds;
		for (std::size_t Index = 0; Index < Primitive.Positions.size(); Index += 3)
		{
			const FVec3 P{Primitive.Positions[Index], Primitive.Positions[Index + 1], Primitive.Positions[Index + 2]};
			const FBounds Point{P, P, true};
			Bounds = Index ? UnionBounds(Bounds, Point) : Point;
		}
		Data->PrimitiveBounds.push_back(Bounds);
	}
	bool bFirst = true;
	for (const auto& Instance : Data->Instances)
	{
		const auto Bounds = TransformBounds(Data->PrimitiveBounds[Instance.Primitive], Instance.World);
		Data->Bounds = bFirst ? Bounds : UnionBounds(Data->Bounds, Bounds);
		bFirst = false;
	}
	return Data;
}

void ValidateMaterialOverride(const FMaterialOverride& InMaterial)
{
	if (InMaterial.BaseColor)
	{
		const auto Color = *InMaterial.BaseColor;
		if (!IsFinite({Color.X, Color.Y, Color.Z}) || !std::isfinite(Color.W))
		{
			throw std::invalid_argument("Invalid scene material color");
		}
	}
	for (const auto Value : {InMaterial.Metallic, InMaterial.Roughness})
	{
		if (Value && (!std::isfinite(*Value) || *Value < 0 || *Value > 1))
		{
			throw std::invalid_argument("Invalid scene material parameter");
		}
	}
}
} // namespace Hyperion
