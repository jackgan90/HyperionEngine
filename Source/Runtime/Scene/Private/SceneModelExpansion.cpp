#include "Hyperion/Scene/SceneManifest.h"
#include <algorithm>
#include <set>

namespace Hyperion
{
namespace
{
void SetSource(FSceneNodeEntry& InNode, const std::string& InAsset, const std::string& InSource,
               const std::string& InRoot)
{
	InNode.Extensions.Slot<FSceneModelSource>() = FSceneModelSource{InAsset, InSource, InRoot};
}

FSceneNodeModel MeshSelection(const FSceneNodeModel& InSelection, const FModelAsset& InModel,
                              const std::string& InSource, std::span<const std::uint32_t> InPrimitives)
{
	auto Result = InSelection;
	Result.SourceNode = InSource;
	Result.SourcePrimitive = InPrimitives.size() == 1 ? ModelPrimitiveId(InModel, InPrimitives.front()) : std::string{};
	Result.Sections.clear();
	for (const auto Primitive : InPrimitives)
	{
		const auto Id = ModelPrimitiveId(InModel, Primitive);
		const auto Existing = std::find_if(InSelection.Sections.begin(), InSelection.Sections.end(),
		                                   [&](const auto& InSection)
		                                   {
			                                   return InSection.Primitive == Id;
		                                   });
		Result.Sections.push_back(Existing == InSelection.Sections.end() ? FSceneMeshSection{Id} : *Existing);
	}
	std::erase_if(Result.SectionSurfaces,
	              [&](const auto& InSection)
	              {
		              return std::find(InPrimitives.begin(), InPrimitives.end(), InSection.Section) ==
		                     InPrimitives.end();
	              });
	return Result;
}

void ExpandParts(std::vector<FSceneNodeEntry>& InOutput, FSceneNodeEntry InNode, const FSceneNodeModel& InSelection,
                 const FModelAsset& InModel, const FModelNode& InSource, const std::string& InSourceId,
                 const std::string& InRoot)
{
	if (InSource.Primitives.size() == 1)
	{
		InNode.Model = MeshSelection(InSelection, InModel, InSourceId, InSource.Primitives);
	}
	const auto ParentId = InNode.Id;
	InOutput.push_back(std::move(InNode));
	if (InSource.Primitives.size() <= 1)
	{
		return;
	}
	for (const auto Primitive : InSource.Primitives)
	{
		FSceneNodeEntry Part;
		Part.Id = ParentId + "/" + ModelPrimitiveId(InModel, Primitive);
		Part.Parent = ParentId;
		Part.Name = InModel.Primitives[Primitive].Name;
		if (Part.Name.empty())
		{
			Part.Name = ModelPrimitiveId(InModel, Primitive);
		}
		Part.Model = MeshSelection(InSelection, InModel, InSourceId, std::span(&Primitive, 1));
		SetSource(Part, InSelection.Asset, InSourceId, InRoot);
		InOutput.push_back(std::move(Part));
	}
}

void ExpandModel(std::vector<FSceneNodeEntry>& InOutput, FSceneNodeEntry InRoot, const FModelAsset& InModel)
{
	const auto Selection = *InRoot.Model;
	const auto RootId = InRoot.Id;
	InRoot.Model.reset();
	InRoot.ComponentIds.erase(RecordType<FSceneModelComponent>().Id);
	SetSource(InRoot, Selection.Asset, {}, RootId);
	InOutput.push_back(std::move(InRoot));
	std::vector<std::pair<std::uint32_t, std::string>> Pending;
	for (const auto Root : InModel.Roots)
	{
		Pending.emplace_back(Root, RootId);
	}
	for (std::size_t Index = 0; Index < Pending.size(); ++Index)
	{
		const auto [SourceIndex, Parent] = Pending[Index];
		const auto& Source = InModel.Nodes.at(SourceIndex);
		const auto SourceId = ModelNodeId(InModel, SourceIndex);
		FSceneNodeEntry Node;
		Node.Id = RootId + "/" + SourceId;
		Node.Name = Source.Name.empty() ? SourceId : Source.Name;
		Node.Parent = Parent;
		Node.Transform = Source.Local;
		SetSource(Node, Selection.Asset, SourceId, RootId);
		for (const auto Child : Source.Children)
		{
			Pending.emplace_back(Child, Node.Id);
		}
		ExpandParts(InOutput, std::move(Node), Selection, InModel, Source, SourceId, RootId);
	}
}
} // namespace

void ExpandSceneModels(FSceneManifest& InManifest,
                       const std::map<std::string, std::shared_ptr<const FModelAsset>>& InModels)
{
	FSceneManifest Candidate = InManifest;
	Candidate.Nodes.clear();
	for (const auto& Node : InManifest.Nodes)
	{
		if (!Node.Model || !Node.Model->SourceNode.empty())
		{
			Candidate.Nodes.push_back(Node);
			continue;
		}
		const auto Model = InModels.find(Node.Model->Asset);
		if (Model == InModels.end() || !Model->second)
		{
			throw std::invalid_argument("Missing model for scene expansion: " + Node.Model->Asset);
		}
		ValidateModel(*Model->second);
		ExpandModel(Candidate.Nodes, Node, *Model->second);
	}
	ValidateSceneManifest(Candidate);
	InManifest = std::move(Candidate);
}
} // namespace Hyperion
