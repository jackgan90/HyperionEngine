#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/AssetImport/MaterialImport.h"
#include "Hyperion/AssetImport/ModelImport.h"
#include "Hyperion/AssetImport/SceneImport.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Support/TestSupport.h"
#include <iostream>
#include <set>

namespace
{
using namespace Hyperion;

template<class F> void Rejects(F InAction)
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
	HYP_CHECK(bRejected);
}

struct FFixture
{
	FTaskSystem Tasks{1, 1};
	FIOService IO{Tasks};
	FAssetImportService Imports{IO};
	FAssetService Assets{IO};
	std::filesystem::path Directory = std::filesystem::absolute("shared-publication");
	FAssetImportOptions Options;

	FFixture()
	{
		RegisterGltfImporter(Imports);
		RegisterSceneImporter(Imports);
		RegisterSceneAssetTypes(Assets.Types());
		Options.Library = Directory / "library";
		const auto Root = std::filesystem::path(HYP_SOURCE_DIR) / "out/fixtures";
		for (const auto* Name : {"Showcase.gltf", "Showcase.bin", "Checker.png"})
		{
			IO.WriteAsync(Directory / Name, *IO.ReadAsync(Root / Name).Get(Tasks)).Get(Tasks);
		}
		IO.WriteAsync(Directory / "Other.gltf", *IO.ReadAsync(Root / "Showcase.gltf").Get(Tasks)).Get(Tasks);
	}
};

std::map<std::string, FAssetRef> TextureReferences(const FAssetGraph& InGraph)
{
	std::map<std::string, FAssetRef> Result;
	for (const auto& [Path, Loaded] : InGraph.Assets)
	{
		if (Loaded->Header.TypeId == RecordType<FTextureAsset>().Id)
		{
			const auto Asset = Loaded->As<FTextureAsset>();
			Result.emplace(Asset->Name, FAssetRef{Loaded->Header.Id, Path.generic_string(), Loaded->Header.TypeId,
			                                      Loaded->Header.Revision});
		}
	}
	return Result;
}

void CheckSharedLibrary(FFixture& InFixture)
{
	auto& F = InFixture;
	// Two independent roots enter one library concurrently, including one Worker configuration.
	const auto First =
	    F.Imports.ImportAsync(F.Directory / "Showcase.gltf", F.Directory / "one/Model.hasset", F.Options);
	const auto Second = F.Imports.ImportAsync(F.Directory / "Other.gltf", F.Directory / "two/Model.hasset", F.Options);
	const auto A = First.Get(F.Tasks);
	const auto B = Second.Get(F.Tasks);
	const auto GraphA = F.Assets.LoadGraphAsync(F.Directory / "one/Model.hasset").Get(F.Tasks);
	const auto GraphB = F.Assets.LoadGraphAsync(F.Directory / "two/Model.hasset").Get(F.Tasks);
	HYP_CHECK(GraphA->Failures.empty() && GraphB->Failures.empty());
	HYP_CHECK(GraphA->Root->Header.Id != GraphB->Root->Header.Id);
	HYP_CHECK(GraphA->Root->As<FModelAsset>()->MaterialSlots[0].Id !=
	          GraphB->Root->As<FModelAsset>()->MaterialSlots[0].Id);
	const auto Textures = TextureReferences(*GraphA);
	HYP_CHECK(Textures == TextureReferences(*GraphB) && Textures.size() == 2);
	for (const auto& [Name, Ref] : Textures)
	{
		HYP_CHECK(GraphA->Assets.at(Ref.Path)->Object == GraphB->Assets.at(Ref.Path)->Object);
	}
	const auto Same =
	    F.Imports.ImportAsync(F.Directory / "Showcase.gltf", F.Directory / "one/Model.hasset", F.Options).Get(F.Tasks);
	HYP_CHECK(Same->bUpToDate && Same->WrittenAssets == 0);
	const auto OldBytes = F.IO.ReadAsync(F.Directory / "one/Model.hasset").Get(F.Tasks);
	F.IO.WriteAsync(F.Directory / "one/Old.hasset", *OldBytes).Get(F.Tasks);
	FImage White{64, 64, EColorSpace::Srgb, std::vector<float>(64 * 64 * 4, 1)};
	F.IO.WriteAsync(F.Directory / "Checker.png", EncodePng(White)).Get(F.Tasks);
	const auto Changed =
	    F.Imports.ImportAsync(F.Directory / "Showcase.gltf", F.Directory / "one/Model.hasset", F.Options).Get(F.Tasks);
	HYP_CHECK(Changed->Header.Id == A->Header.Id && Changed->Header.Revision != A->Header.Revision);
	const auto OldGraph = F.Assets.LoadGraphAsync(F.Directory / "one/Old.hasset").Get(F.Tasks);
	HYP_CHECK(OldGraph->Failures.empty() && TextureReferences(*OldGraph) == Textures);
	F.Assets.Invalidate(F.Directory / "one/Model.hasset");
	const auto NewGraph = F.Assets.LoadGraphAsync(F.Directory / "one/Model.hasset").Get(F.Tasks);
	const auto NewTextures = TextureReferences(*NewGraph);
	HYP_CHECK(NewTextures.at("Checker.png").Id == Textures.at("Checker.png").Id);
	HYP_CHECK(NewTextures.at("Checker.png").Revision != Textures.at("Checker.png").Revision);
	// A held library lease prevents publication through another service.
	FAssetImportService Other(F.IO);
	RegisterGltfImporter(Other);
	const auto Lease = F.IO.AcquireWriteLeaseAsync(F.Options.Library / ".asset-library.hasset").Get(F.Tasks);
	Rejects(
	    [&]
	    {
		    Other.ImportAsync(F.Directory / "Other.gltf", F.Directory / "three/Model.hasset", F.Options).Get(F.Tasks);
	    });
}

void CheckPinnedGenerations(FFixture& InFixture)
{
	auto& F = InFixture;
	const auto Old = F.Assets.LoadGraphAsync(F.Directory / "one/Old.hasset").Get(F.Tasks);
	const auto New = F.Assets.LoadGraphAsync(F.Directory / "one/Model.hasset").Get(F.Tasks);
	FSceneManifest Scene;
	for (const auto& Entry : {std::pair{"old", Old}, std::pair{"new", New}})
	{
		const auto& Header = Entry.second->Root->Header;
		Scene.Assets.push_back(
		    {Entry.first,
		     {Header.Id, std::string("one/") + (Entry.first == std::string_view("old") ? "Old.hasset" : "Model.hasset"),
		      Header.TypeId, Header.Revision}});
		Scene.Instances.push_back({Entry.first, Entry.first});
	}
	const auto Source = F.Directory / "MixedGenerations.source.hasset";
	F.IO.WriteAsync(Source, EncodeAsset(RecordType<FSceneManifest>(), &Scene).Bytes).Get(F.Tasks);
	const auto Output = F.Directory / "MixedGenerations.hasset";
	F.Imports.ImportAsync(Source, Output, F.Options).Get(F.Tasks);
	const auto Graph = F.Assets.LoadGraphAsync(Output).Get(F.Tasks);
	HYP_CHECK(Graph->Failures.empty());
	std::map<std::string, std::set<std::string>> Revisions;
	for (const auto& [Path, Asset] : Graph->Assets)
	{
		Revisions[Asset->Header.Id].insert(Asset->Header.Revision);
	}
	const auto MaterialId = Old->Root->As<FModelAsset>()->MaterialSlots[0].Id;
	HYP_CHECK(Revisions.at(MaterialId).size() == 2);
	const auto TextureId = TextureReferences(*Old).at("Checker.png").Id;
	HYP_CHECK(Revisions.at(TextureId).size() == 2);
	HYP_CHECK(F.Imports.ImportAsync(Source, Output, F.Options).Get(F.Tasks)->bUpToDate);
	std::cout << "Pinned material/texture generations coexist in one published scene\n";
}

void CheckSourceProductConflict(FFixture& InFixture)
{
	auto& F = InFixture;
	FAssetImportService Imports(F.IO);
	Imports.Register({"test.conflicting-products",
	                  1,
	                  &RecordType<FMaterialAsset>(),
	                  {".conflict"},
	                  [](FAssetImportContext& InContext)
	                  {
		                  FMaterialDescription Description;
		                  Description.Name = "Conflicting source products";
		                  FMaterialPass Pass;
		                  Pass.Vertex = {"SharedAsset.hlsl", "AssetVertex"};
		                  Pass.Pixel = {"SharedAsset.hlsl", "AssetPixel"};
		                  Description.Passes.push_back(Pass);
		                  auto Asset = std::make_shared<FMaterialAsset>();
		                  for (const auto* Name : {"A", "B"})
		                  {
			                  FMaterialParameterDeclaration Parameter;
			                  Parameter.Name = Name;
			                  Parameter.Type = FMaterialParameterType::Resource(EMaterialValueKind::Texture2D);
			                  Description.Parameters.push_back(Parameter);
			                  auto Texture = std::make_shared<const FTextureAsset>(BuildTextureAsset(
			                      Name, EMaterialTextureEncoding::Linear, {1, 1, {255, 255, 255, 255}}));
			                  FMaterialAssetValue Value;
			                  Value.Type = Parameter.Type;
			                  Value.Texture = InContext.Emit(
			                      {Name, std::make_shared<const FRecordDescriptor>(RecordType<FTextureAsset>()),
			                       Texture, "test-conflict"});
			                  Asset->Values.push_back({Name, Value});
		                  }
		                  auto Values = std::move(Asset->Values);
		                  *Asset = PersistMaterialDescription(Description);
		                  Asset->Values = std::move(Values);
		                  return Asset;
	                  }});
	const auto Source = F.Directory / "Products.conflict";
	F.IO.WriteAsync(Source, FBytes{std::byte{1}}).Get(F.Tasks);
	bool bRejected{};
	try
	{
		Imports.ImportAsync(Source, F.Directory / "Conflicting.hasset", F.Options).Get(F.Tasks);
	}
	catch (const std::runtime_error& Error)
	{
		bRejected = std::string(Error.what()).find("Conflicting products claim one shared import identity") !=
		            std::string::npos;
		if (!bRejected)
		{
			throw;
		}
	}
	HYP_CHECK(bRejected);
}

void CheckLegacyAndVariants(FFixture& InFixture)
{
	auto& F = InFixture;
	const auto Source = F.Imports.LoadAsync<FModelSource>(F.Directory / "Showcase.gltf").Get(F.Tasks);
	auto Embedded = *Source;
	Embedded.Materials[0].NormalTexture = Embedded.Materials[0].BaseColorTexture;
	Embedded.Samplers.push_back(
	    {EWrapMode::Clamp, EWrapMode::Mirror, ESamplerFilter::Nearest, ESamplerFilter::Nearest});
	Embedded.Materials[1].BaseColorTexture = Embedded.Materials[0].BaseColorTexture;
	Embedded.Materials[1].BaseColorTexture.Sampler = static_cast<std::int32_t>(Embedded.Samplers.size() - 1);
	const auto Split = SplitModelSource(Embedded);
	std::size_t Srgb{};
	std::size_t Linear{};
	for (const auto& Product : Split.Products)
	{
		if (Product.Key == "image-0-srgb")
		{
			++Srgb;
		}
		if (Product.Key == "image-0-linear")
		{
			++Linear;
		}
	}
	HYP_CHECK(Srgb == 1 && Linear == 1);
	auto LegacyType = RecordType<FModelSource>();
	LegacyType.Id = RecordType<FModelAsset>().Id;
	const auto Legacy = EncodeAsset(LegacyType, &Embedded);
	const auto Path = F.Directory / "Embedded.hasset";
	F.IO.WriteAsync(Path, Legacy.Bytes).Get(F.Tasks);
	Rejects(
	    [&]
	    {
		    F.Assets.LoadAsync<FModelAsset>(Path).Get(F.Tasks);
	    });
	const auto Output = F.Directory / "Upgraded.hasset";
	F.IO.WriteAsync(Output, Legacy.Bytes).Get(F.Tasks);
	const auto Upgraded = F.Imports.ImportAsync(Path, Output, F.Options).Get(F.Tasks);
	HYP_CHECK(Upgraded->Header.Id == Legacy.Header.Id && Upgraded->Header.SchemaVersion == 2);
	F.Assets.Invalidate(Output);
	const auto Graph = F.Assets.LoadGraphAsync(Output).Get(F.Tasks);
	HYP_CHECK(Graph->Failures.empty());
	HYP_CHECK(Graph->Root->As<FModelAsset>()->Primitives[0].Positions == Source->Primitives[0].Positions);
	HYP_CHECK(TextureReferences(*Graph).size() >= 2);
}

void CheckJson(FFixture& InFixture)
{
	auto Texture =
	    BuildTextureAsset("Authored", EMaterialTextureEncoding::Srgb, {2, 1, {0, 0, 0, 255, 255, 255, 255, 255}});
	const auto Node = WriteRecord(RecordType<FTextureAsset>(), &Texture);
	const auto Json = EncodeAssetSourceJson(Node);
	const auto Restored =
	    std::static_pointer_cast<FTextureAsset>(ReadRecord(RecordType<FTextureAsset>(), DecodeAssetSourceJson(Json)));
	HYP_CHECK(Serialize(*Restored) == Serialize(Texture));
	const auto Path = InFixture.Directory / "Texture.json";
	const auto Bytes = std::as_bytes(std::span(Json.data(), Json.size()));
	InFixture.IO.WriteAsync(Path, FBytes(Bytes.begin(), Bytes.end())).Get(InFixture.Tasks);
	const auto Published =
	    InFixture.Imports.ImportAsync(Path, InFixture.Directory / "Texture.hasset", InFixture.Options)
	        .Get(InFixture.Tasks);
	HYP_CHECK(Published->Header.TypeId == RecordType<FTextureAsset>().Id);
	for (const auto* Invalid :
	     {R"({"$bulk":"u8","data":[256]})", R"({"$bulk":"u32","data":[-1]})", R"({"type":1,"type":2})"})
	{
		Rejects(
		    [&]
		    {
			    DecodeAssetSourceJson(Invalid);
		    });
	}
}
} // namespace

int main()
{
	try
	{
		FFixture Fixture;
		CheckSharedLibrary(Fixture);
		CheckPinnedGenerations(Fixture);
		CheckSourceProductConflict(Fixture);
		CheckLegacyAndVariants(Fixture);
		CheckJson(Fixture);
		std::cout << "Shared source identities, concurrent roots, old revisions, explicit legacy split, roles/samplers "
		             "and JSON passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
