#include "Hyperion/Scene/Scene.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Support/TestSupport.h"

namespace
{
using namespace Hyperion;

std::shared_ptr<FModelAsset> SharedModel()
{
	auto Model = std::make_shared<FModelAsset>();
	Model->MaterialSlots.push_back({{}, "material.hasset", "hyperion.materialasset"});
	FModelPrimitive Primitive;
	Primitive.Positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
	Primitive.Indices = {0, 1, 2};
	Primitive.Material = 0;
	Model->Primitives = {Primitive, Primitive};
	Model->Nodes = {{"parent", Translation({2, 0, 0}), {}, {1, 2}},
	                {"first", Translation({0, 3, 0}), {0, 1}},
	                {"second", Scale({-2, 1, 1}), {0}}};
	Model->Roots = {0};
	AssignModelSubresourceIds(*Model);
	return Model;
}

void CheckExpansion()
{
	const auto Model = SharedModel();
	const auto Data = PrepareSceneModel(Model);
	FSceneManifest Manifest;
	Manifest.Assets.push_back({"shared", {{}, "model.hasset", RecordType<FModelAsset>().Id}});
	FSceneNodeEntry Root;
	Root.Id = "instance";
	Root.Transform = Translation({10, 0, 0});
	Root.Model = FSceneNodeModel{};
	Root.Model->Asset = "shared";
	Root.Model->Material.Roughness = .7f;
	Root.Model->Sections.push_back({"primitive-1", false, {}});
	Manifest.Nodes.push_back(Root);
	Root.Id = "other";
	Root.bEnabled = false;
	Manifest.Nodes.push_back(Root);
	ExpandSceneModels(Manifest, {{"shared", Model}});
	HYP_CHECK(Manifest.Nodes.size() == 12 && SceneModelCount(Manifest) == 6);
	HYP_CHECK(!Manifest.Nodes[0].Model && !Manifest.Nodes[2].Model);
	HYP_CHECK(Manifest.Nodes[4].Model->Sections.size() == 1 && !Manifest.Nodes[4].Model->Sections[0].bVisible);
	HYP_CHECK(Manifest.Nodes[3].Model->Material.Roughness == .7f);
	FScene Scene;
	for (auto Node : NodesFromSceneManifest(Manifest))
	{
		if (Node.Model())
		{
			Node.Model()->Data = Data;
		}
		Scene.AddNode(std::move(Node));
	}
	const auto Expected = ModelInstances(*Model);
	std::vector<FModelInstance> Actual;
	for (const auto Handle : Scene.GetNodes(ESceneNodeKind::Model))
	{
		FSceneNodeView View;
		HYP_CHECK(Scene.GetNodeView(Handle, View));
		HYP_CHECK(View.Node->Model()->Data == Data);
		const auto Selected = SceneModelInstances(SceneModelTransfer(*View.Node, View.World, View.bEffectiveEnabled));
		for (const auto& Instance : Selected)
		{
			if (View.Node->Id.starts_with("instance/"))
			{
				Actual.push_back({Instance.Primitive, Multiply(View.World, Instance.World)});
			}
		}
	}
	HYP_CHECK(Actual.size() == Expected.size());
	for (std::size_t Index = 0; Index < Actual.size(); ++Index)
	{
		HYP_CHECK(Actual[Index].Primitive == Expected[Index].Primitive);
		HYP_CHECK(Actual[Index].World.Values == Multiply(Root.Transform, Expected[Index].World).Values);
	}
	const auto Archive = WriteValue(Manifest);
	auto Restored = ReadValue<FSceneManifest>(Archive);
	ExpandSceneModels(Restored, {});
	HYP_CHECK(Restored.Nodes.size() == Manifest.Nodes.size());
	HYP_CHECK(Restored.Nodes[4].Model->SourceNode == "node-1");
	HYP_CHECK(Restored.Nodes[4].Model->SourcePrimitive == "primitive-1");
	HYP_CHECK(Restored.Nodes[4].Model->Sections[0].Primitive == "primitive-1");
}
} // namespace

void CheckSceneExpansion()
{
	CheckExpansion();
}
