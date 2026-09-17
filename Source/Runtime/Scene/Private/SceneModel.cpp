#include "Hyperion/Scene/Scene.h"
#include <algorithm>

namespace Hyperion
{
bool FMaterialOverride::operator==(const FMaterialOverride& InOther) const
{
	if (BaseColor.has_value() != InOther.BaseColor.has_value())
	{
		return false;
	}
	return (!BaseColor || (BaseColor->X == InOther.BaseColor->X && BaseColor->Y == InOther.BaseColor->Y &&
	                       BaseColor->Z == InOther.BaseColor->Z && BaseColor->W == InOther.BaseColor->W)) &&
	       Metallic == InOther.Metallic && Roughness == InOther.Roughness;
}

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
	for (std::size_t Index = 0; Index < Data->Asset->Nodes.size(); ++Index)
	{
		const auto Id = ModelNodeId(*Data->Asset, Index);
		auto& Instances = Data->NodeInstances[Id];
		auto& Bounds = Data->NodeBounds[Id];
		for (const auto Primitive : Data->Asset->Nodes[Index].Primitives)
		{
			Instances.push_back({Primitive, Identity()});
			const auto& Local = Data->PrimitiveBounds[Primitive];
			Bounds = Bounds.bValid ? UnionBounds(Bounds, Local) : Local;
		}
	}
	return Data;
}

FBounds SceneModelBounds(const FSceneModel& InModel)
{
	if (!InModel.Data)
	{
		return {};
	}
	if (!InModel.SourcePrimitive.empty())
	{
		const auto Instances = SceneModelInstances(InModel);
		return InModel.Data->PrimitiveBounds.at(Instances.front().Primitive);
	}
	return InModel.SourceNode.empty() ? InModel.Data->Bounds : InModel.Data->NodeBounds.at(InModel.SourceNode);
}

std::vector<FModelInstance> SceneModelInstances(const FSceneModel& InModel)
{
	auto Result =
	    InModel.SourceNode.empty() ? InModel.Data->Instances : InModel.Data->NodeInstances.at(InModel.SourceNode);
	if (!InModel.SourcePrimitive.empty())
	{
		if (InModel.SourceNode.empty())
		{
			throw std::invalid_argument("Primitive selection requires a source node");
		}
		std::erase_if(Result,
		              [&](const auto& InInstance)
		              {
			              return ModelPrimitiveId(*InModel.Data->Asset, InInstance.Primitive) !=
			                     InModel.SourcePrimitive;
		              });
		if (Result.empty())
		{
			throw std::invalid_argument("Primitive selection is absent from its source node");
		}
	}
	return Result;
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
