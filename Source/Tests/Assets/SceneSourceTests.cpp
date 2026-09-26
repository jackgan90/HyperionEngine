#include "Hyperion/AssetImport/AssetSourceJson.h"
#include "Hyperion/AssetImport/SkyImport.h"
#include "Hyperion/Scene/Scene.h"
#include "Hyperion/Scene/SceneManifest.h"
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
	const auto Source = ReadValue<FSceneManifest>(WriteValue(Legacy));
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

void CheckSceneJsonRejected()
{
	FTaskSystem Tasks(1, 1);
	auto Files = std::make_shared<FMemoryFileSystem>();
	FIOService IO(Tasks, Files);
	FAssetImportService Imports(IO);
	RegisterSkyImporter(Imports);
	for (const auto& Text : {std::string(R"({"type":"hyperion.scene","schema_version":2,"assets":[],"nodes":[]})"),
	                         EncodeAssetSourceJson(WriteValue(FSceneManifest{}))})
	{
		const auto Bytes = std::as_bytes(std::span(Text));
		IO.WriteAsync("scene.json", FBytes(Bytes.begin(), Bytes.end())).Get(Tasks);
		Rejects(
		    [&]
		    {
			    Imports.ImportAsync("scene.json", "scene.hasset").Get(Tasks);
		    });
		HYP_CHECK(!Files->Exists("scene.hasset"));
	}
}
} // namespace

void CheckSceneSources()
{
	CheckLegacyVersions();
	CheckSceneJsonRejected();
}
