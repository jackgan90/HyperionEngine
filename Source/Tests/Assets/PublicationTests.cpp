#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/AssetImport/ModelImport.h"
#include "Hyperion/AssetImport/SceneImport.h"
#include "Support/TestSupport.h"
#include <iostream>

namespace Hyperion
{
struct FImportFixture
{
	std::string Name;
	std::string External;
	std::vector<FAssetRef> Children;
	std::vector<std::uint8_t> Data;
};

template<> const FRecordDescriptor& RecordType<FImportFixture>()
{
	static const auto Type = MakeRecord<FImportFixture>(
	    "test.import", {Member("name", &FImportFixture::Name), Member("external", &FImportFixture::External),
	                    Member("children", &FImportFixture::Children), Member("data", &FImportFixture::Data)});
	return Type;
}
} // namespace Hyperion

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

class FImportStorage final : public IFileSystem
{
public:
	FMemoryFileSystem Memory;
	bool bFailDependencyWrites{};
	bool bDenySources{};

	FBytes Read(const std::filesystem::path& InPath, std::size_t InLimit) override
	{
		if (bDenySources && InPath.extension() != ".hasset")
		{
			throw std::runtime_error("Runtime source access forbidden");
		}
		return Memory.Read(InPath, InLimit);
	}

	void WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes) override
	{
		if (bFailDependencyWrites && InPath.parent_path().filename() == ".assets")
		{
			throw std::runtime_error("Injected dependency publication failure");
		}
		Memory.WriteAtomic(InPath, InBytes);
	}
};

FAssetImporter FixtureImporter()
{
	return {"test.source",
	        1,
	        &RecordType<FImportFixture>(),
	        {".source"},
	        [](FAssetImportContext& InContext)
	        {
		        auto Object = std::make_shared<FImportFixture>(Deserialize<FImportFixture>(*InContext.Bytes));
		        if (!Object->External.empty())
		        {
			        const auto Bytes = InContext.Read(InContext.Path.parent_path() / Object->External);
			        Object->Data.assign(reinterpret_cast<const std::uint8_t*>(Bytes->data()),
			                            reinterpret_cast<const std::uint8_t*>(Bytes->data()) + Bytes->size());
		        }
		        return Object;
	        }};
}

void CheckPublication()
{
	FTaskSystem Tasks(1, 1);
	auto Files = std::make_shared<FImportStorage>();
	FIOService IO(Tasks, Files);
	FAssetImportService Imports(IO);
	Imports.Register(FixtureImporter());
	const auto Directory = std::filesystem::absolute("publication-test").lexically_normal();
	const auto Source = Directory / "root.source";
	const auto Output = Directory / "native/root.hasset";
	FImportFixture Child{"child", "data.bin"};
	FImportFixture Root{"root",
	                    "",
	                    {{"", "child.source", RecordType<FImportFixture>().Id, ""},
	                     {"", "child.source", RecordType<FImportFixture>().Id, ""}}};
	IO.WriteAsync(Source, Serialize(Root)).Get(Tasks);
	IO.WriteAsync(Directory / "child.source", Serialize(Child)).Get(Tasks);
	IO.WriteAsync(Directory / "data.bin", {std::byte{1}}).Get(Tasks);
	const auto First = Imports.ImportAsync(Source, Output).Get(Tasks);
	HYP_CHECK(!First->bUpToDate && First->WrittenAssets == 3 && First->Header.Dependencies.size() == 2);
	HYP_CHECK(First->Header.Dependencies[0].Reference == First->Header.Dependencies[1].Reference);
	const auto Old = IO.ReadAsync(Output).Get(Tasks);
	const auto Writes = IO.Statistics().Writes.load();
	const auto Same = Imports.ImportAsync(Source, Output).Get(Tasks);
	HYP_CHECK(Same->bUpToDate && Same->WrittenAssets == 0 && IO.Statistics().Writes == Writes);
	IO.WriteAsync(Directory / "data.bin", {std::byte{2}}).Get(Tasks);
	const auto Changed = Imports.ImportAsync(Source, Output).Get(Tasks);
	HYP_CHECK(!Changed->bUpToDate && Changed->Header.Id == First->Header.Id &&
	          Changed->Header.Revision != First->Header.Revision);
	HYP_CHECK(Changed->Header.Dependencies[0].Reference.Id == First->Header.Dependencies[0].Reference.Id);
	const auto Previous = IO.ReadAsync(Output).Get(Tasks);
	IO.WriteAsync(Directory / "data.bin", {std::byte{3}}).Get(Tasks);
	Files->bFailDependencyWrites = true;
	Rejects(
	    [&]
	    {
		    Imports.ImportAsync(Source, Output).Get(Tasks);
	    });
	Files->bFailDependencyWrites = false;
	HYP_CHECK(*IO.ReadAsync(Output).Get(Tasks) == *Previous);
	const auto Lease = IO.AcquireWriteLeaseAsync(Output).Get(Tasks);
	Rejects(
	    [&]
	    {
		    Imports.ImportAsync(Source, Output).Get(Tasks);
	    });
	IO.WriteAsync(Output.parent_path() / "old.hasset", *Old).Get(Tasks);
	Files->bDenySources = true;
	FAssetService Native(IO);
	Native.Types().Register<FImportFixture>();
	const auto Graph = Native.LoadGraphAsync(Output).Get(Tasks);
	HYP_CHECK(Graph->Failures.empty() && Graph->Assets.size() == 2);
	const auto OldGraph = Native.LoadGraphAsync(Output.parent_path() / "old.hasset").Get(Tasks);
	HYP_CHECK(OldGraph->Failures.empty());
	const auto OldChild =
	    Native.LoadReferenceAsync(OldGraph->Root->Header.Dependencies[0].Reference, OldGraph->Root->Path)
	        .Get(Tasks)
	        ->As<FImportFixture>();
	HYP_CHECK(OldChild->Data == std::vector<std::uint8_t>{1});
}

FAssetHeader StoreNative(FIOService& InIO, const std::filesystem::path& InPath, const FImportFixture& InObject,
                         FAssetHeader InHeader = {})
{
	auto Encoded = EncodeAsset(RecordType<FImportFixture>(), &InObject, std::move(InHeader));
	InIO.WriteAsync(InPath, std::move(Encoded.Bytes)).Get(InIO.TaskSystem());
	return Encoded.Header;
}

void CheckPinnedNativeImport()
{
	FTaskSystem Tasks(1, 1);
	FIOService IO(Tasks, std::make_shared<FMemoryFileSystem>());
	FAssetImportService Imports(IO);
	Imports.Register({"test.native",
	                  1,
	                  &RecordType<FImportFixture>(),
	                  {".hasset"},
	                  [](FAssetImportContext& InContext)
	                  {
		                  return ReadRecord(RecordType<FImportFixture>(), DecodeAsset(InContext.Bytes).Object);
	                  }});
	const auto Directory = std::filesystem::absolute("pinned-import");
	const auto Source = Directory / "root.hasset";
	const auto Output = Directory / "native/root.hasset";
	const auto Type = RecordType<FImportFixture>().Id;
	const auto Leaf = StoreNative(IO, Directory / "leaf.hasset", {"leaf"});
	const auto Child =
	    StoreNative(IO, Directory / "child.hasset", {"child", "", {{Leaf.Id, "leaf.hasset", Type, Leaf.Revision}}});
	const FAssetRef Valid{Child.Id, "child.hasset", Type, Child.Revision};
	StoreNative(IO, Source, {"root", "", {Valid, Valid}});
	const auto Initial = Imports.ImportAsync(Source, Output).Get(Tasks);
	// Rebasing the child's own reference changes its published revision, while its source pin remains valid.
	HYP_CHECK(Initial->Header.Dependencies[0].Reference.Revision != Child.Revision);
	const auto Previous = IO.ReadAsync(Output).Get(Tasks);
	for (unsigned Case = 0; Case < 4; ++Case)
	{
		auto Invalid = Valid;
		if (Case < 2)
		{
			Invalid.Revision = std::string(64, '0');
		}
		else
		{
			Invalid.Id = std::string(32, '0');
		}
		const auto Index = Case % 2;
		FImportFixture Root{"root", "", {Valid, Valid}};
		Root.Children[Index] = Invalid;
		StoreNative(IO, Source, Root);
		std::string Error;
		try
		{
			Imports.ImportAsync(Source, Output).Get(Tasks);
		}
		catch (const std::exception& Failure)
		{
			Error = Failure.what();
		}
		HYP_CHECK(Error.find("children[" + std::to_string(Index) + "]") != std::string::npos);
		HYP_CHECK(Error.find("identity/type/revision mismatch") != std::string::npos);
		HYP_CHECK(*IO.ReadAsync(Output).Get(Tasks) == *Previous);
	}
	FAssetService Assets(IO);
	Assets.Types().Register<FImportFixture>();
	HYP_CHECK(Assets.LoadGraphAsync(Output).Get(Tasks)->Failures.empty());
}

void CheckCrossVolumeImport()
{
	FTaskSystem Tasks(1, 1);
	FIOService IO(Tasks, std::make_shared<FMemoryFileSystem>());
	FAssetImportService Imports(IO);
	Imports.Register(FixtureImporter());
	const auto Directory = std::filesystem::absolute("cross-volume-import");
	const auto OtherDrive = Directory.root_name() == "C:" ? "F:" : "C:";
	const auto Other = std::filesystem::path(std::string(OtherDrive) + "/other-import");
	const auto Source = Directory / "root.source";
	const auto Output = Directory / "native/root.hasset";
	const auto Type = RecordType<FImportFixture>().Id;
	const FImportFixture Root{
	    "root",
	    "",
	    {{"", (Other / "a.source").generic_string(), Type, ""}, {"", (Other / "b.source").generic_string(), Type, ""}}};
	IO.WriteAsync(Source, Serialize(Root)).Get(Tasks);
	IO.WriteAsync(Other / "a.source", Serialize(FImportFixture{"a"})).Get(Tasks);
	IO.WriteAsync(Other / "b.source", Serialize(FImportFixture{"b"})).Get(Tasks);
	const auto First = Imports.ImportAsync(Source, Output).Get(Tasks);
	const auto Same = Imports.ImportAsync(Source, Output).Get(Tasks);
	HYP_CHECK(Same->bUpToDate && Serialize(Same->Header) == Serialize(First->Header));
	FAssetImportOptions Options;
	Options.bForce = true;
	const auto Forced = Imports.ImportAsync(Source, Output, Options).Get(Tasks);
	HYP_CHECK(Forced->Header.Dependencies == First->Header.Dependencies);
	HYP_CHECK(Forced->Header.Dependencies[0].Reference.Id != Forced->Header.Dependencies[1].Reference.Id);
	for (const auto& Entry : Forced->Header.Import->Sources)
	{
		HYP_CHECK(!Entry.Path.empty());
	}
	IO.WriteAsync(Other / "a.source", Serialize(FImportFixture{"changed"})).Get(Tasks);
	const auto Changed = Imports.ImportAsync(Source, Output).Get(Tasks);
	HYP_CHECK(!Changed->bUpToDate);
	HYP_CHECK(Changed->Header.Dependencies[0].Reference.Id == First->Header.Dependencies[0].Reference.Id);
	HYP_CHECK(Changed->Header.Dependencies[1].Reference == First->Header.Dependencies[1].Reference);
	FAssetService Assets(IO);
	Assets.Types().Register<FImportFixture>();
	HYP_CHECK(Assets.LoadGraphAsync(Output).Get(Tasks)->Failures.empty());
}

void CheckCyclesAndOrdering()
{
	FTaskSystem Tasks(1, 1);
	FIOService IO(Tasks, std::make_shared<FMemoryFileSystem>());
	FAssetImportService Imports(IO);
	Imports.Register(FixtureImporter());
	const auto Directory = std::filesystem::absolute("publication-cycle").lexically_normal();
	FImportFixture Root{"cycle", "", {{"", "root.source", RecordType<FImportFixture>().Id, ""}}};
	IO.WriteAsync(Directory / "root.source", Serialize(Root)).Get(Tasks);
	Rejects(
	    [&]
	    {
		    Imports.ImportAsync(Directory / "root.source", Directory / "root.hasset").Get(Tasks);
	    });
	Root.Children.clear();
	IO.WriteAsync(Directory / "root.source", Serialize(Root)).Get(Tasks);
	const auto First = Imports.ImportAsync(Directory / "root.source", Directory / "root.hasset");
	const auto Second = Imports.ImportAsync(Directory / "root.source", Directory / "root.hasset");
	HYP_CHECK(!First.Get(Tasks)->bUpToDate && Second.Get(Tasks)->bUpToDate);
	Rejects(
	    [&]
	    {
		    Imports.ImportAsync(Directory / "root.source", Directory / "root.source");
	    });
}

void CheckModelToScene()
{
	FTaskSystem Tasks(1, 1);
	FIOService IO(Tasks);
	FAssetImportService Imports(IO);
	RegisterGltfImporter(Imports);
	RegisterSceneImporter(Imports);
	const auto Source = std::filesystem::path(HYP_SOURCE_DIR) / "out/fixtures/Showcase.gltf";
	const auto Output = std::filesystem::absolute("publication-model-scene/scene.hasset");
	FAssetImportOptions Options;
	Options.bScene = true;
	Options.Name = "Imported model scene";
	const auto Result = Imports.ImportAsync(Source, Output, Options).Get(Tasks);
	HYP_CHECK(Result->Header.TypeId == RecordType<FSceneManifest>().Id && Result->Header.Dependencies.size() == 1);
	FAssetService Assets(IO);
	RegisterSceneAssetTypes(Assets.Types());
	const auto Graph = Assets.LoadGraphAsync(Output).Get(Tasks);
	HYP_CHECK(Graph->Failures.empty() && Graph->Root->As<FSceneManifest>()->Instances.size() == 1);
	const auto Model =
	    Assets.LoadReferenceAsync<FModelAsset>(Result->Header.Dependencies[0].Reference, Output).Get(Tasks);
	HYP_CHECK(ModelInstances(*Model).size() == 4 && Model->MaterialSlots.size() == 4);
}

void CheckExternalImageReimport()
{
	FTaskSystem Tasks(1, 1);
	FIOService IO(Tasks);
	FAssetImportService Imports(IO);
	RegisterGltfImporter(Imports);
	const auto Fixtures = std::filesystem::path(HYP_SOURCE_DIR) / "out/fixtures";
	const auto Directory = std::filesystem::absolute("image-reimport");
	for (const auto* File : {"Showcase.gltf", "Showcase.bin", "Checker.png"})
	{
		IO.WriteAsync(Directory / File, *IO.ReadAsync(Fixtures / File).Get(Tasks)).Get(Tasks);
	}
	const auto Source = Directory / "Showcase.gltf";
	const auto Output = Directory / "native/model.hasset";
	const auto First = Imports.ImportAsync(Source, Output).Get(Tasks);
	HYP_CHECK(First->Header.Import->Sources.size() == 3);
	FAssetService Assets(IO);
	const auto Before = Assets.LoadAsync<FModelAsset>(Output).Get(Tasks);
	FImage White{64, 64, EColorSpace::Srgb, std::vector<float>(64 * 64 * 4, 1)};
	IO.WriteAsync(Directory / "Checker.png", EncodePng(White)).Get(Tasks);
	const auto Second = Imports.ImportAsync(Source, Output).Get(Tasks);
	HYP_CHECK(First->Header.Id == Second->Header.Id && First->Header.Revision != Second->Header.Revision);
	Assets.Invalidate(Output);
	const auto After = Assets.LoadAsync<FModelAsset>(Output).Get(Tasks);
	HYP_CHECK(After->MaterialSlots != Before->MaterialSlots);
	HYP_CHECK(After->Primitives[0].Positions == Before->Primitives[0].Positions);
	IO.WriteAsync(Output.parent_path() / "copy.hasset", Serialize(*Before)).Get(Tasks);
	const auto Upgraded =
	    Imports.ImportAsync(Output.parent_path() / "copy.hasset", Directory / "upgraded.hasset").Get(Tasks);
	const auto Native = Assets.LoadAsync<FModelAsset>(Directory / "upgraded.hasset").GetAsset(Tasks);
	HYP_CHECK(Native->Diagnostics.empty() && Native->Header.Id == Upgraded->Header.Id);
	HYP_CHECK(Native->As<FModelAsset>()->Primitives[0].Positions == Before->Primitives[0].Positions);
}

void CheckLocalLease()
{
	FTaskSystem Tasks(1, 1);
	FIOService First(Tasks);
	FIOService Second(Tasks);
	const auto Path = std::filesystem::absolute("publication-lease/root.hasset");
	{
		const auto Lease = First.AcquireWriteLeaseAsync(Path).Get(Tasks);
		Rejects(
		    [&]
		    {
			    Second.AcquireWriteLeaseAsync(Path).Get(Tasks);
		    });
	}
	const auto Lease = Second.AcquireWriteLeaseAsync(Path).Get(Tasks);
	HYP_CHECK(static_cast<bool>(*Lease));
}
} // namespace

int main()
{
	try
	{
		CheckPublication();
		CheckPinnedNativeImport();
		CheckCrossVolumeImport();
		CheckCyclesAndOrdering();
		CheckModelToScene();
		CheckLocalLease();
		CheckExternalImageReimport();
		std::cout
		    << "Generic incremental publication, source changes, immutable generations, failure and leases passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
