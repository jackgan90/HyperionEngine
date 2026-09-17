#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/AssetImport/MaterialImport.h"
#include "Hyperion/AssetImport/SceneImport.h"
#include "Support/TestSupport.h"

namespace
{
using namespace Hyperion;

struct FReferenceFixture
{
	FTaskSystem Tasks{1, 1};
	FIOService IO{Tasks};
	FAssetImportService Imports{IO};
	FAssetService Assets{IO};
	std::filesystem::path Directory = std::filesystem::absolute("model-reference-publication");
	std::filesystem::path Source = std::filesystem::path(HYP_SOURCE_DIR) / "out/fixtures/Showcase.gltf";
	std::filesystem::path Output = Directory / "scene.hasset";
	FSceneManifest Scene;
	std::shared_ptr<const FModelAsset> Model;
	FAssetImportOptions Options;

	FReferenceFixture()
	{
		RegisterGltfImporter(Imports);
		RegisterSceneAssetTypes(Assets.Types());
		Options.bScene = true;
		Options.bForce = true;
		Imports.ImportAsync(Source, Output, Options).Get(Tasks);
		Scene = *Assets.LoadAsync<FSceneManifest>(Output).Get(Tasks);
		Model = Assets.LoadReferenceAsync<FModelAsset>(Scene.Assets.front().Reference, Output).Get(Tasks);
		Options.bForce = false;
	}
};

void CheckPublicationPolicy(FReferenceFixture& InFixture)
{
	auto& F = InFixture;
	HYP_CHECK(F.Scene.Nodes.size() == 3 && SceneModelCount(F.Scene) == 1 && F.Scene.DefaultCamera.empty());
	HYP_CHECK(ModelInstances(*F.Model).size() == 4);
	auto Previous = DecodeAsset(F.IO.ReadAsync(F.Output).Get(F.Tasks));
	Previous.Header.Import->Settings.erase("scene_model_policy");
	auto Expanded = F.Scene;
	ExpandSceneModels(Expanded, {{Expanded.Assets.front().Id, F.Model}});
	HYP_CHECK(SceneModelCount(Expanded) == 4);
	F.IO.WriteAsync(F.Output, EncodeAsset(RecordType<FSceneManifest>(), &Expanded, Previous.Header).Bytes).Get(F.Tasks);
	// No Scene importer is registered here: model-to-scene policy must invalidate its own old cache.
	const auto Rebuilt = F.Imports.ImportAsync(F.Source, F.Output, F.Options).Get(F.Tasks);
	HYP_CHECK(!Rebuilt->bUpToDate && Rebuilt->Header.Id == Previous.Header.Id);
	F.Assets.Invalidate(F.Output);
	const auto Compact = F.Assets.LoadAsync<FSceneManifest>(F.Output).Get(F.Tasks);
	HYP_CHECK(Compact->Nodes.size() == 3 && SceneModelCount(*Compact) == 1 && Compact->DefaultCamera.empty());
	HYP_CHECK(Serialize(*Compact) == Serialize(F.Scene));
	const auto Same = F.Imports.ImportAsync(F.Source, F.Output, F.Options).Get(F.Tasks);
	HYP_CHECK(Same->bUpToDate && Same->WrittenAssets == 0);
}

void CheckReferencesAndLegacyEdits(FReferenceFixture& InFixture)
{
	auto& F = InFixture;
	FAssetImportService Imports(F.IO);
	RegisterGltfImporter(Imports);
	RegisterSceneImporter(Imports);
	const auto ModelBefore = Serialize(*F.Model);
	auto Scene = F.Scene;
	auto Found = std::find_if(Scene.Nodes.begin(), Scene.Nodes.end(),
	                          [](const auto& InNode)
	                          {
		                          return InNode.Model.has_value();
	                          });
	HYP_CHECK(Found != Scene.Nodes.end());
	Found->Transform = Translation({10, 2, 3});
	Found->Model->Material.Roughness = .7f;
	Found->Model->Sections.push_back({ModelPrimitiveId(*F.Model, 0), true, {}});
	Found->Model->Sections.back().Material.Metallic = .4f;
	auto Other = *Found;
	Other.Id = "other";
	Other.Transform = Translation({-4, 0, 0});
	Other.Model->Material.Roughness = .2f;
	Scene.Nodes.push_back(Other);
	const auto Source = F.Directory / "instances.json";
	const auto Text = EncodeAssetSourceJson(WriteValue(Scene));
	const auto Bytes = std::as_bytes(std::span(Text));
	F.IO.WriteAsync(Source, FBytes(Bytes.begin(), Bytes.end())).Get(F.Tasks);
	const auto Output = F.Directory / "instances.hasset";
	FAssetImportOptions Force;
	Force.bForce = true;
	Imports.ImportAsync(Source, Output, Force).Get(F.Tasks);
	const auto Restored = F.Assets.LoadAsync<FSceneManifest>(Output).Get(F.Tasks);
	HYP_CHECK(Restored->Nodes.size() == Scene.Nodes.size() && SceneModelCount(*Restored) == 2);
	for (std::size_t Index = 0; Index < Scene.Nodes.size(); ++Index)
	{
		HYP_CHECK(Serialize(Restored->Nodes[Index]) == Serialize(Scene.Nodes[Index]));
	}
	HYP_CHECK(Restored->Assets.size() == 1);
	const auto Shared = F.Assets.LoadReferenceAsync<FModelAsset>(Restored->Assets[0].Reference, Output).Get(F.Tasks);
	HYP_CHECK(Serialize(*Shared) == ModelBefore);
	ExpandSceneModels(Scene, {{Scene.Assets.front().Id, F.Model}});
	auto Part = std::find_if(Scene.Nodes.begin(), Scene.Nodes.end(),
	                         [](const auto& InNode)
	                         {
		                         return InNode.Model.has_value();
	                         });
	HYP_CHECK(Part != Scene.Nodes.end());
	Part->Transform = Translation({17, 8, 9});
	Part->Name = "Authored child";
	const auto LegacySource = F.Directory / "expanded.hasset";
	F.IO.WriteAsync(LegacySource, EncodeAsset(RecordType<FSceneManifest>(), &Scene).Bytes).Get(F.Tasks);
	const auto LegacyOutput = F.Directory / "preserved.hasset";
	Imports.ImportAsync(LegacySource, LegacyOutput, Force).Get(F.Tasks);
	const auto Preserved = F.Assets.LoadAsync<FSceneManifest>(LegacyOutput).Get(F.Tasks);
	HYP_CHECK(Preserved->Nodes.size() == Scene.Nodes.size());
	for (std::size_t Index = 0; Index < Scene.Nodes.size(); ++Index)
	{
		HYP_CHECK(Serialize(Preserved->Nodes[Index]) == Serialize(Scene.Nodes[Index]));
	}
}
} // namespace

void CheckModelReferences()
{
	FReferenceFixture Fixture;
	CheckPublicationPolicy(Fixture);
	CheckReferencesAndLegacyEdits(Fixture);
}
