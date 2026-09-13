#include "Hyperion/AssetImport/MaterialImport.h"
#include "Hyperion/AssetImport/SceneImport.h"
#include "Hyperion/Scene/Scene.h"
#include "Support/TestSupport.h"

namespace
{
using namespace Hyperion;

template<class F> void Rejects(F InAction, std::string_view InCase = "native conflict")
{
	bool bRejected{};
	try
	{
		InAction();
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	if (!bRejected)
	{
		throw std::runtime_error("Expected rejection: " + std::string(InCase));
	}
}

void CheckSourceNodes()
{
	const auto Manifest = DecodeSceneManifest(R"({"type":"hyperion.scene","schema_version":2,"assets":[],
	  "nodes":[{"id":"camera","parent":"rig","translation":[0,0,8],"camera":{"focusDistance":8}},
	  {"id":"rig","translation":[1,2,3]}, {"id":"sun","directionalLight":{"color":[2,3,4],"intensity":2}},
	  {"id":"ambient","environmentLight":{"intensity":0}}],"defaultCamera":"camera","mainDirectionalLight":"sun"})");
	HYP_CHECK(Manifest.Nodes.size() == 4 && SceneModelCount(Manifest) == 0);
	FScene Scene;
	Scene.LoadNodes(NodesFromSceneManifest(Manifest));
	Scene.SetSettings(ResolveSceneSettings(Manifest, Scene));
	FSceneCameraPose Pose;
	HYP_CHECK(Scene.GetCameraPose(*Scene.GetSettings().DefaultCamera, Pose));
	HYP_CHECK(Pose.Eye.X == 1 && Pose.Eye.Y == 2 && Pose.Eye.Z == 11 && Pose.Forward.Z == -1);
	const auto RoundTrip = DecodeSceneManifest(EncodeAssetSourceJson(WriteValue(Manifest)));
	HYP_CHECK(Serialize(RoundTrip) == Serialize(Manifest));
	const auto Empty = DecodeSceneManifest(R"({"type":"hyperion.scene","schema_version":2,"assets":[],"nodes":[]})");
	HYP_CHECK(Empty.Nodes.empty() && Empty.DefaultCamera.empty() && Empty.MainDirectionalLight.empty());
	const auto Matrix = DecodeSceneManifest(R"({"type":"hyperion.scene","schema_version":2,"assets":[],
	  "nodes":[{"id":"group","transform":[1,0,0,0,0.3,1,0,0,0,0,-2,0,1,2,3,1]}]})");
	const auto ValidSingular = DecodeSceneManifest(
	    R"({"type":"hyperion.scene","schema_version":2,"assets":[],"nodes":[{"id":"camera","camera":{},"scale":[0,1,1]}]})");
	HYP_CHECK(ValidSingular.Nodes.size() == 1);
	HYP_CHECK(Matrix.Nodes[0].Transform.Values[4] == .3f && Matrix.Nodes[0].Transform.Values[10] == -2);
}

void CheckInvalidSourceNodes()
{
	for (const auto* Node :
	     {R"({"id":"a","camra":{}})", R"({"id":"a","camera":{"projection":"orthographic"}})",
	      R"({"id":"a","camera":{},"directionalLight":{}})", R"({"id":"a","parent":"missing"})",
	      R"({"id":"a","parent":"a"})", R"({"id":"a","camera":{"near":3,"far":2}})",
	      R"({"id":"a","camera":{},"scale":[1,0,1]})", R"({"id":"a","directionalLight":{"intensity":-1}})",
	      R"({"id":"a","environmentLight":{"color":[1,-1,1]}})", R"({"id":"a","model":{"asset":"missing"}})",
	      R"({"id":"a","rotation":[0,0,0,0]})", R"({"id":"a","id":"b"})",
	      R"({"id":"a","transform":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],"translation":[0,0,0]})"})
	{
		Rejects(
		    [&]
		    {
			    DecodeSceneManifest(
			        std::string(R"({"type":"hyperion.scene","schema_version":2,"assets":[],"nodes":[)") + Node + "]}");
		    },
		    Node);
	}
	for (const auto* Suffix : {R"(,"camera":{})", R"(,"instances":[])", R"(,"eye":[0,0,1])",
	                           R"(,"defaultCamera":"missing")", R"(,"mainDirectionalLight":"a")"})
	{
		Rejects(
		    [&]
		    {
			    DecodeSceneManifest(
			        std::string(
			            R"({"type":"hyperion.scene","schema_version":2,"assets":[],"nodes":[{"id":"a","camera":{}}])") +
			        Suffix + "}");
		    },
		    Suffix);
	}
}

void CheckLegacyVersions()
{
	FLegacySceneManifest Legacy;
	Legacy.Eye = {1, 2, 8};
	Legacy.Target = {1, 2, 0};
	Legacy.Assets.push_back({"model", {{}, "model.hasset", RecordType<FModelAsset>().Id, {}}});
	Legacy.Instances.push_back({"camera-main", "model"});
	Legacy.Instances.push_back({"camera-main-1", "model"});
	Legacy.Instances[0].Transform.Values[4] = .375f;
	Legacy.Instances[0].bVisible = false;
	Legacy.Instances[0].Material.Roughness = .21f;
	Legacy.Instances[0].Surface.Reference = FAssetRef{{}, "material.hasset", RecordType<FMaterialAsset>().Id, {}};
	Legacy.Instances[0].SectionSurfaces.push_back({3, Legacy.Instances[0].Surface});
	for (std::uint32_t Version : {1u, 2u, 3u})
	{
		auto Archive = WriteValue(Legacy);
		std::get<FArchiveNode::FObject>(Archive.Value)["version"] = WriteValue(Version);
		const auto Migrated = ReadValue<FSceneManifest>(Archive);
		HYP_CHECK(Migrated.Nodes.size() == 5 && Migrated.DefaultCamera == "camera-main-2");
		HYP_CHECK(Migrated.Nodes[0].Transform.Values[4] == .375f && !Migrated.Nodes[0].Model->bVisible);
		HYP_CHECK(Migrated.Nodes[0].Model->Material.Roughness == .21f);
		HYP_CHECK(Migrated.Nodes[0].Model->SectionSurfaces[0].Material.Reference ==
		          Legacy.Instances[0].Surface.Reference);
		const auto Current = ReadValue<FSceneManifest>(WriteValue(Migrated));
		HYP_CHECK(Serialize(Current) == Serialize(Migrated));
	}
	const auto Source = DecodeSceneManifest(R"({"type":"hyperion.scene","schema_version":1,"assets":[],"instances":[],
	  "camera":{"eye":[1,2,8],"target":[1,2,0],"near":0.02,"far":50}})");
	HYP_CHECK(Source.Nodes.size() == 3 && Source.Nodes[0].Camera->FocusDistance == 8);
	auto Conflict = WriteValue(Source);
	auto& Root = std::get<FArchiveNode::FObject>(Conflict.Value);
	auto& Fields = std::get<FArchiveNode::FObject>(Root.at("fields").Value);
	Fields["eye"] = WriteValue(FVec3{});
	Rejects(
	    [&]
	    {
		    ReadValue<FSceneManifest>(Conflict);
	    });
	Root["version"] = WriteValue(3u);
	Rejects(
	    [&]
	    {
		    ReadValue<FSceneManifest>(Conflict);
	    });
}

void CheckLocalLightSources()
{
	const auto Source = DecodeSceneManifest(R"({"type":"hyperion.scene","schema_version":3,"assets":[],"nodes":[
	{"id":"rig","translation":[1,2,3]},
	{"id":"point","parent":"rig","pointLight":{"color":[1,0.5,0.2],"intensity":8,"range":4}},
	{"id":"spot","enabled":false,"spotLight":{"range":6,"innerRadians":0.2,"outerRadians":0.7}}]})");
	HYP_CHECK(Source.Nodes.size() == 3 && Source.Nodes[1].PointLight->Range == 4 && !Source.Nodes[2].bEnabled);
	const auto Native = ReadValue<FSceneManifest>(WriteValue(Source));
	HYP_CHECK(Serialize(Native) == Serialize(Source));
	const auto Text = DecodeSceneManifest(EncodeAssetSourceJson(WriteValue(Native)));
	HYP_CHECK(Text.Nodes[2].SpotLight == Source.Nodes[2].SpotLight);
	FScene Scene;
	Scene.LoadNodes(NodesFromSceneManifest(Text));
	for (const auto Handle : Scene.GetNodes())
	{
		const auto Entry = SceneEntryFromNode(*Scene.FindNode(Handle));
		HYP_CHECK(NodeFromSceneEntry(Entry) == *Scene.FindNode(Handle));
	}
	for (const auto* Payload : {R"("pointLight":{"range":0})", R"("spotLight":{"innerRadians":0.8,"outerRadians":0.7})",
	                            R"("pointLight":{},"spotLight":{})", R"("pointLight":{"rang":4})"})
	{
		Rejects(
		    [&]
		    {
			    DecodeSceneManifest(
			        std::string(R"({"type":"hyperion.scene","schema_version":3,"assets":[],"nodes":[{"id":"bad",)") +
			        Payload + "}]}");
		    });
	}
	auto Old = WriteValue(FSceneManifest{});
	std::get<FArchiveNode::FObject>(Old.Value)["version"] = WriteValue(4u);
	HYP_CHECK(ReadValue<FSceneManifest>(Old).Nodes.empty());
}
} // namespace

void CheckSceneSources()
{
	CheckSourceNodes();
	CheckInvalidSourceNodes();
	CheckLegacyVersions();
	CheckLocalLightSources();
}
