#include "Hyperion/Scene/Scene.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Support/TestSupport.h"
#include <atomic>
#include <iostream>
#include <limits>
#include <random>

void CheckSceneNodes();
void CheckSceneComponents();
void CheckSceneExpansion();

namespace
{
using namespace Hyperion;

bool CornerOracle(const FBounds& InBounds, const FMat4& InClip)
{
	std::array<bool, 6> Outside{true, true, true, true, true, true};
	for (unsigned Corner = 0; Corner < 8; ++Corner)
	{
		const auto P = BoundsCorner(InBounds, Corner);
		const auto C = Transform(InClip, {P.X, P.Y, P.Z, 1});
		const std::array Tests{C.X<-C.W, C.X> C.W, C.Y<-C.W, C.Y> C.W, C.Z<0, C.Z> C.W};
		for (unsigned Plane = 0; Plane < 6; ++Plane)
		{
			Outside[Plane] = Outside[Plane] && Tests[Plane];
		}
	}
	return std::none_of(Outside.begin(), Outside.end(),
	                    [](bool bInOutside)
	                    {
		                    return bInOutside;
	                    });
}

void CheckBounds()
{
	const FBounds Box{{-1, -1, -1}, {1, 1, 1}, true};
	const auto World = Multiply(Translation({2, 3, 4}), Scale({-2, .5f, 3}));
	const auto Bounds = TransformBounds(Box, World);
	HYP_CHECK(Bounds.Minimum.X == 0 && Bounds.Maximum.X == 4 && Bounds.Minimum.Z == 1 && Bounds.Maximum.Z == 7);
	HYP_CHECK(!IsUsable(UnionBounds(Box, {})));
	HYP_CHECK(FFrustum(Identity()).Intersects({}));
	HYP_CHECK(FFrustum(Identity()).Intersects({{1, -.5f, 0}, {2, .5f, 1}, true}));
	auto Invalid = Identity();
	Invalid.Values[0] = std::numeric_limits<float>::quiet_NaN();
	HYP_CHECK(FFrustum(Invalid).Intersects(Box));
	std::mt19937 Random(713);
	std::uniform_real_distribution<float> Position(-20, 20);
	for (unsigned Index = 0; Index < 5000; ++Index)
	{
		const auto TransformMatrix = Multiply(
		    Translation({Position(Random), Position(Random), Position(Random)}),
		    ComposeTRS({}, {0, std::sin(float(Index) * .01f), 0, std::cos(float(Index) * .01f)}, {-2, .25f, 1}));
		const auto Clip = Multiply(Perspective(1, 1.3f, .1f, 100), TransformMatrix);
		if (CornerOracle(Box, Clip))
		{
			HYP_CHECK(FFrustum(Clip).Intersects(Box));
		}
	}
}

void CheckLogicalScene()
{
	FScene Scene;
	FScene Foreign;
	const auto First = Scene.Add({"first"});
	const auto Initial = Scene.GetChanges();
	auto Model = *Scene.Find(First);
	Model.World = Translation({5, 0, 0});
	HYP_CHECK(Scene.Update(First, Model));
	Scene.Acknowledge(Initial.back().Revision);
	HYP_CHECK(Scene.GetChanges().size() == 1 && Initial[0].Model->World.Values[12] == 0);
	HYP_CHECK(Scene.Remove(First));
	const auto Second = Scene.Add({"second"});
	HYP_CHECK(First.Slot == Second.Slot && First.Generation != Second.Generation);
	HYP_CHECK(!Scene.Update(First, {}) && !Scene.Remove(First));
	HYP_CHECK(!Scene.Update(Foreign.Add({}), {}));
	HYP_CHECK(Scene.GetChanges().size() == 2 && Scene.Find(Second)->Name == "second");
	Model.World.Values[3] = 1;
	bool bRejected = false;
	try
	{
		Scene.Update(Second, Model);
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected && Scene.Find(Second)->Name == "second");
	std::atomic<bool> bWrongThreadRejected{};
	std::thread Other(
	    [&]
	    {
		    try
		    {
			    Scene.GetHandles();
		    }
		    catch (const std::logic_error&)
		    {
			    bWrongThreadRejected = true;
		    }
	    });
	Other.join();
	HYP_CHECK(bWrongThreadRejected);
	Scene.Clear();
	HYP_CHECK(Scene.GetHandles().empty());
}

void CheckManifestMigration()
{
	FLegacySceneManifest Original;
	Original.Assets = {{"model", {"", "Model.hasset", RecordType<FModelAsset>().Id, ""}}};
	Original.Instances = {{"stable-id", "model"}};
	auto Node = WriteValue(Original);
	auto& Root = std::get<FArchiveNode::FObject>(Node.Value);
	Root["version"] = WriteValue(1u);
	auto& Fields = std::get<FArchiveNode::FObject>(Root.at("fields").Value);
	auto& Asset = std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FArray>(Fields.at("assets").Value)[0].Value);
	Asset["version"] = WriteValue(1u);
	auto& AssetFields = std::get<FArchiveNode::FObject>(Asset.at("fields").Value);
	AssetFields.erase("reference");
	AssetFields["path"] = WriteValue(std::string("Model.hasset"));
	auto& Instance =
	    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FArray>(Fields.at("instances").Value)[0].Value);
	Instance["version"] = WriteValue(1u);
	auto& InstanceFields = std::get<FArchiveNode::FObject>(Instance.at("fields").Value);
	InstanceFields.erase("transform");
	InstanceFields.erase("material");
	InstanceFields.erase("name");
	InstanceFields["translation"] = WriteValue(FVec3{1, 2, 3});
	InstanceFields["rotation"] = WriteValue(FVec4{0, 0, 0, 1});
	InstanceFields["scale"] = WriteValue(FVec3{2, 3, 4});
	std::vector<std::string> Diagnostics;
	const auto Restored = ReadValue<FSceneManifest>(Node, {"legacy-scene", &Diagnostics});
	HYP_CHECK(Restored.Assets[0].Reference.TypeId == RecordType<FModelAsset>().Id);
	HYP_CHECK(Restored.Nodes[0].Id == "stable-id" && Restored.Nodes[0].Transform.Values[12] == 1);
	HYP_CHECK(Restored.Nodes[0].Transform.Values[5] == 3 && Diagnostics.size() == 3);
}

void CheckNativeNodeKinds()
{
	FSceneManifest Manifest;
	FSceneNodeEntry Entry;
	Entry.Id = "node";
	Manifest.Nodes.push_back(Entry);
	for (const auto Name : {"pointLight", "camrea", "projection"})
	{
		auto Archive = WriteValue(Manifest);
		auto& Root = std::get<FArchiveNode::FObject>(Archive.Value);
		auto& Fields = std::get<FArchiveNode::FObject>(Root.at("fields").Value);
		auto& Nodes = std::get<FArchiveNode::FArray>(Fields.at("nodes").Value);
		auto& Node = std::get<FArchiveNode::FObject>(Nodes[0].Value);
		auto& NodeFields = std::get<FArchiveNode::FObject>(Node.at("fields").Value);
		if (std::string_view(Name) == "projection")
		{
			auto Camera = WriteValue(FSceneCamera{});
			auto& CameraRecord = std::get<FArchiveNode::FObject>(Camera.Value);
			auto& CameraFields = std::get<FArchiveNode::FObject>(CameraRecord.at("fields").Value);
			CameraFields[Name] = WriteValue(std::string("orthographic"));
			auto& Components = std::get<FArchiveNode::FArray>(NodeFields.at("components").Value);
			Components.emplace_back(FArchiveNode::FObject{{"id", WriteValue(std::string("lens"))},
			                                              {"type", WriteValue(RecordType<FSceneCamera>().Id)},
			                                              {"state", std::move(Camera)}});
		}
		else
		{
			NodeFields[Name] = FArchiveNode(FArchiveNode::FObject{});
		}
		bool bRejected{};
		try
		{
			ReadValue<FSceneManifest>(Archive, {"native-scene"});
		}
		catch (const std::runtime_error& Error)
		{
			const std::string Message = Error.what();
			bRejected = Message.find("nodes[0]") != std::string::npos && Message.find(Name) != std::string::npos;
		}
		HYP_CHECK(bRejected);
	}
	const auto Restored = ReadValue<FSceneManifest>(WriteValue(Manifest));
	HYP_CHECK(Restored.Nodes.size() == 1 && NodeFromSceneEntry(Restored.Nodes[0]).GetKind() == ESceneNodeKind::Group);
}

void CheckManifest()
{
	FLegacySceneManifest Manifest;
	Manifest.Assets.push_back({"a", {"", "../Models/A.hasset", RecordType<FModelAsset>().Id, ""}});
	Manifest.Instances = {{"one", "a"}, {"two", "a", Identity(), false}};
	HYP_CHECK(Manifest.Instances.size() == 2 && !Manifest.Instances[1].bVisible);
	const auto Copy = ReadValue<FSceneManifest>(WriteValue(UpgradeLegacyScene(Manifest)));
	HYP_CHECK(Copy.Nodes[0].Model->Asset == "a");
	for (unsigned Case = 0; Case < 5; ++Case)
	{
		auto Bad = Manifest;
		if (Case == 0)
		{
			Bad.Instances[1].Id = "one";
		}
		if (Case == 1)
		{
			Bad.Assets.push_back(Bad.Assets[0]);
		}
		if (Case == 2)
		{
			Bad.Instances[0].Asset = "missing";
		}
		if (Case == 3)
		{
			Bad.Instances[0].Transform.Values[15] = 0;
		}
		if (Case == 4)
		{
			Bad.Far = Bad.Near;
		}
		bool bRejected = false;
		try
		{
			ValidateLegacySceneManifest(Bad);
		}
		catch (const std::invalid_argument&)
		{
			bRejected = true;
		}
		HYP_CHECK(bRejected);
	}
}
} // namespace

void CheckTransformInspection();
void CheckInitialViews();
void CheckSceneBatches();

int main()
{
	try
	{
		CheckBounds();
		CheckLogicalScene();
		CheckSceneNodes();
		CheckSceneComponents();
		CheckTransformInspection();
		CheckInitialViews();
		CheckSceneBatches();
		CheckSceneExpansion();
		CheckManifest();
		CheckManifestMigration();
		CheckNativeNodeKinds();
		std::cout << "Bounds oracle, logical scene ownership, generations and manifest validation passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
