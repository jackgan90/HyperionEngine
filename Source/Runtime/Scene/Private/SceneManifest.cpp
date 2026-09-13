#include "Hyperion/Scene/SceneManifest.h"
#include "Hyperion/Scene/Scene.h"
#include <algorithm>
#include <set>

namespace Hyperion
{
namespace
{
void ValidateNodeEntry(const FSceneNodeEntry& InEntry)
{
	const auto Node = NodeFromSceneEntry(InEntry);
	ValidateSceneNode(Node);
	if (!InEntry.Model)
	{
		return;
	}
	ValidateSceneMaterialAsset(InEntry.Model->Surface);
	std::set<std::uint32_t> Sections;
	for (const auto& Section : InEntry.Model->SectionSurfaces)
	{
		if (!Sections.insert(Section.Section).second)
		{
			throw std::invalid_argument("Duplicate persistent scene material section");
		}
		ValidateSceneMaterialAsset(Section.Material);
	}
}
} // namespace

FSceneNode NodeFromSceneEntry(const FSceneNodeEntry& InEntry)
{
	FSceneNode Node;
	Node.Id = InEntry.Id;
	Node.Name = InEntry.Name.empty() ? InEntry.Id : InEntry.Name;
	Node.Parent = InEntry.Parent;
	Node.Local = InEntry.Transform;
	Node.bEnabled = InEntry.bEnabled;
	Node.Camera = InEntry.Camera;
	Node.DirectionalLight = InEntry.DirectionalLight;
	Node.EnvironmentLight = InEntry.EnvironmentLight;
	Node.SpotLight = InEntry.SpotLight;
	Node.PointLight = InEntry.PointLight;
	if (InEntry.Model)
	{
		Node.Model = FSceneModelComponent{};
		Node.Model->Asset = InEntry.Model->Asset;
		Node.Model->bVisible = InEntry.Model->bVisible;
		Node.Model->Material = InEntry.Model->Material;
	}
	return Node;
}

FSceneNodeEntry SceneEntryFromNode(const FSceneNode& InNode)
{
	FSceneNodeEntry Entry;
	Entry.Id = InNode.Id;
	Entry.Name = InNode.Name;
	Entry.Parent = InNode.Parent;
	Entry.Transform = InNode.Local;
	Entry.bEnabled = InNode.bEnabled;
	Entry.Camera = InNode.Camera;
	Entry.DirectionalLight = InNode.DirectionalLight;
	Entry.EnvironmentLight = InNode.EnvironmentLight;
	Entry.SpotLight = InNode.SpotLight;
	Entry.PointLight = InNode.PointLight;
	if (InNode.Model)
	{
		Entry.Model = FSceneNodeModel{};
		Entry.Model->Asset = InNode.Model->Asset;
		Entry.Model->bVisible = InNode.Model->bVisible;
		Entry.Model->Material = InNode.Model->Material;
	}
	return Entry;
}

std::vector<FSceneNode> NodesFromSceneManifest(const FSceneManifest& InManifest)
{
	std::vector<FSceneNode> Nodes;
	Nodes.reserve(InManifest.Nodes.size());
	for (const auto& Entry : InManifest.Nodes)
	{
		Nodes.push_back(NodeFromSceneEntry(Entry));
	}
	return Nodes;
}

FSceneSettings ResolveSceneSettings(const FSceneManifest& InManifest, const FScene& InScene)
{
	const auto Resolve = [&](const std::string& InId) -> std::optional<FSceneHandle>
	{
		if (InId.empty())
		{
			return {};
		}
		const auto Handle = InScene.FindHandle(InId);
		if (!Handle.Scene)
		{
			throw std::invalid_argument("Scene selection refers to a missing node: " + InId);
		}
		return Handle;
	};
	return {Resolve(InManifest.DefaultCamera), Resolve(InManifest.MainDirectionalLight),
	        Resolve(InManifest.EnvironmentLight)};
}

std::size_t SceneModelCount(const FSceneManifest& InManifest)
{
	return std::count_if(InManifest.Nodes.begin(), InManifest.Nodes.end(),
	                     [](const FSceneNodeEntry& InNode)
	                     {
		                     return InNode.Model.has_value();
	                     });
}

void ValidateSceneManifest(const FSceneManifest& InManifest)
{
	std::set<std::string> Assets;
	for (const auto& Asset : InManifest.Assets)
	{
		ValidateAssetRef(Asset.Reference);
		if (Asset.Id.empty() || !Assets.insert(Asset.Id).second ||
		    Asset.Reference.TypeId != RecordType<FModelAsset>().Id)
		{
			throw std::invalid_argument("Scene asset IDs must be unique and references must target models");
		}
	}
	for (const auto& Node : InManifest.Nodes)
	{
		ValidateNodeEntry(Node);
		if (Node.Model && !Assets.contains(Node.Model->Asset))
		{
			throw std::invalid_argument("Scene model node references a missing asset");
		}
	}
	// Use the same iterative hierarchy/pose implementation for document validation and installation.
	FScene Validation;
	Validation.LoadNodes(NodesFromSceneManifest(InManifest));
	Validation.SetSettings(ResolveSceneSettings(InManifest, Validation));
}

template<> const FRecordDescriptor& RecordType<FSceneNodeModel>()
{
	static const auto Type = MakeRecord<FSceneNodeModel>(
	    "hyperion.scenenodemodel",
	    {Member("asset", &FSceneNodeModel::Asset, {true}), Member("visible", &FSceneNodeModel::bVisible),
	     Member("material", &FSceneNodeModel::Material), Member("surface", &FSceneNodeModel::Surface),
	     Member("sectionSurfaces", &FSceneNodeModel::SectionSurfaces)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneNodeEntry>()
{
	static const auto Type = []
	{
		auto Result = MakeRecord<FSceneNodeEntry>(
		    "hyperion.scenenode",
		    {Member("id", &FSceneNodeEntry::Id, {true}), Member("name", &FSceneNodeEntry::Name),
		     Member("parent", &FSceneNodeEntry::Parent), Member("transform", &FSceneNodeEntry::Transform),
		     Member("enabled", &FSceneNodeEntry::bEnabled), Member("model", &FSceneNodeEntry::Model),
		     Member("camera", &FSceneNodeEntry::Camera), Member("directionalLight", &FSceneNodeEntry::DirectionalLight),
		     Member("environmentLight", &FSceneNodeEntry::EnvironmentLight),
		     Member("pointLight", &FSceneNodeEntry::PointLight), Member("spotLight", &FSceneNodeEntry::SpotLight)},
		    2, ValidateNodeEntry);
		Result.Migrations.emplace(1,
		                          [](FArchiveNode::FObject&)
		                          {
		                          });
		Result.bRejectUnknownFields = true;
		return Result;
	}();
	return Type;
}

void MigrateSceneNodes(FArchiveNode::FObject& InFields, const FRecordReadContext& InContext);

template<> const FRecordDescriptor& RecordType<FSceneManifest>()
{
	static const auto Type = []
	{
		auto Result = MakeRecord<FSceneManifest>("hyperion.scene",
		                                         {Member("assets", &FSceneManifest::Assets, {true}),
		                                          Member("nodes", &FSceneManifest::Nodes, {true}),
		                                          Member("defaultCamera", &FSceneManifest::DefaultCamera),
		                                          Member("mainDirectionalLight", &FSceneManifest::MainDirectionalLight),
		                                          Member("environmentLight", &FSceneManifest::EnvironmentLight)},
		                                         5, ValidateSceneManifest);
		Result.Migrations.emplace(4,
		                          [](FArchiveNode::FObject&)
		                          {
		                          });
		Result.Migrations.emplace(1,
		                          [](FArchiveNode::FObject&)
		                          {
		                          });
		Result.Migrations.emplace(2,
		                          [](FArchiveNode::FObject&)
		                          {
		                          });
		Result.ContextMigrations.emplace(3, MigrateSceneNodes);
		Result.RejectedFields = {"instances", "eye", "target", "near", "far", "camera"};
		return Result;
	}();
	return Type;
}

void RegisterSceneAssetTypes(FRecordRegistry& InRegistry)
{
	InRegistry.Register<FModelAsset>();
	InRegistry.Register<FMaterialAsset>();
	InRegistry.Register<FTextureAsset>();
	InRegistry.Register<FSkyAsset>();
	InRegistry.Register<FSceneManifest>();
}
} // namespace Hyperion
