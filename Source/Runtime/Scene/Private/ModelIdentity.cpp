#include "Hyperion/Scene/Model.h"

namespace Hyperion
{
std::string ModelNodeId(const FModelAsset& InModel, std::size_t InIndex)
{
	const auto& Id = InModel.Nodes.at(InIndex).Id;
	return Id.empty() ? "node-" + std::to_string(InIndex) : Id;
}

std::string ModelPrimitiveId(const FModelAsset& InModel, std::size_t InIndex)
{
	const auto& Id = InModel.Primitives.at(InIndex).Id;
	return Id.empty() ? "primitive-" + std::to_string(InIndex) : Id;
}

void AssignModelSubresourceIds(FModelAsset& InModel)
{
	for (std::size_t Index = 0; Index < InModel.Nodes.size(); ++Index)
	{
		InModel.Nodes[Index].Id = ModelNodeId(InModel, Index);
	}
	for (std::size_t Index = 0; Index < InModel.Primitives.size(); ++Index)
	{
		InModel.Primitives[Index].Id = ModelPrimitiveId(InModel, Index);
	}
}

std::vector<FModelInstance> SelectedModelInstances(const FModelAsset& InModel, std::string_view InSourceNode)
{
	for (std::size_t Index = 0; Index < InModel.Nodes.size(); ++Index)
	{
		if (ModelNodeId(InModel, Index) == InSourceNode)
		{
			std::vector<FModelInstance> Instances;
			for (const auto Primitive : InModel.Nodes[Index].Primitives)
			{
				// Source hierarchy is already represented by scene Transforms.
				Instances.push_back({Primitive, Identity()});
			}
			return Instances;
		}
	}
	throw std::invalid_argument("Missing model source node: " + std::string(InSourceNode));
}
} // namespace Hyperion
