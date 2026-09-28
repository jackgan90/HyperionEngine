#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/AssetImport/ModelImport.h"
#include "Hyperion/AssetImport/SkyImport.h"
#include "Hyperion/Core/ContentHash.h"
#include "Hyperion/IO/MountedFileSystem.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Support/TestSupport.h"
#include <iostream>
#include <map>

void CheckSceneSources();
void CheckModelReferences();

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
	std::filesystem::path ReplacementPath;
	FBytes Replacement;
	unsigned ReplaceOnRead{};
	unsigned ReplacementReads{};
	unsigned FailOnWrite{};
	unsigned WriteCalls{};
	std::map<std::filesystem::path, std::size_t> ReadCounts;

	std::vector<FDirectoryEntry> ListDirectory(const std::filesystem::path& InPath) override
	{
		return Memory.ListDirectory(InPath);
	}

	void Remove(const std::filesystem::path& InPath) override
	{
		Memory.Remove(InPath);
	}

	FBytes Read(const std::filesystem::path& InPath, std::size_t InLimit) override
	{
		++ReadCounts[InPath];
		if (InPath == ReplacementPath && ++ReplacementReads == ReplaceOnRead)
		{
			Memory.WriteAtomic(InPath, Replacement);
		}
		if (bDenySources && InPath.extension() != ".hasset")
		{
			throw std::runtime_error("Runtime source access forbidden");
		}
		return Memory.Read(InPath, InLimit);
	}

	void WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes) override
	{
		if (++WriteCalls == FailOnWrite)
		{
			throw std::runtime_error("Injected transaction write failure");
		}
		if (bFailDependencyWrites && InPath.filename().string().starts_with("child_"))
		{
			bFailDependencyWrites = false;
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
	HYP_CHECK(!First->bUpToDate && First->WrittenAssets == 2 && First->Header.Dependencies.size() == 2);
	HYP_CHECK(First->Header.Dependencies[0].Reference == First->Header.Dependencies[1].Reference);
	const auto Old = IO.ReadAsync(Output).Get(Tasks);
	const auto Writes = IO.Statistics().Writes.load();
	const auto Same = Imports.ImportAsync(Source, Output).Get(Tasks);
	HYP_CHECK(Same->bUpToDate && Same->WrittenAssets == 0 && IO.Statistics().Writes == Writes);
	IO.WriteAsync(Directory / "data.bin", {std::byte{2}}).Get(Tasks);
	const auto Changed = Imports.ImportAsync(Source, Output).Get(Tasks);
	HYP_CHECK(!Changed->bUpToDate && Changed->Header.Id == First->Header.Id &&
	          Changed->Header.Revision == First->Header.Revision);
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
	HYP_CHECK(OldChild->Data == std::vector<std::uint8_t>{2});
}

FAssetHeader StoreNative(FIOService& InIO, const std::filesystem::path& InPath, const FImportFixture& InObject,
                         FAssetHeader InHeader = {})
{
	auto Encoded = EncodeAsset(RecordType<FImportFixture>(), &InObject, std::move(InHeader));
	InIO.WriteAsync(InPath, std::move(Encoded.Bytes)).Get(InIO.TaskSystem());
	return Encoded.Header;
}

void CheckExternalPublicationChanges()
{
	for (const unsigned ReplaceOnRead : {2u, 3u})
	{
		FTaskSystem Tasks(1, 1);
		auto Files = std::make_shared<FImportStorage>();
		FIOService IO(Tasks, Files);
		FAssetImportService Imports(IO);
		Imports.Register(FixtureImporter());
		const auto Source = std::filesystem::absolute("external-publication/root.source");
		const auto Output = std::filesystem::absolute("external-publication/root.hasset");
		IO.WriteAsync(Source, Serialize(FImportFixture{"previous"})).Get(Tasks);
		Imports.ImportAsync(Source, Output).Get(Tasks);
		const auto Previous = IO.ReadAsync(Output).Get(Tasks);
		FImportFixture Child{"old"};
		const auto Original = EncodeAsset(RecordType<FImportFixture>(), &Child);
		Child.Name = "new";
		Files->Replacement = EncodeAsset(RecordType<FImportFixture>(), &Child, Original.Header).Bytes;
		Files->ReplacementPath = "/Game/External.hasset";
		Files->ReplaceOnRead = ReplaceOnRead;
		Files->Memory.WriteAtomic(Files->ReplacementPath, Original.Bytes);
		const FAssetRef Reference{"", "/Game/External.hasset", RecordType<FImportFixture>().Id, ""};
		IO.WriteAsync(Source, Serialize(FImportFixture{"root", "", {Reference, Reference}})).Get(Tasks);
		Rejects(
		    [&]
		    {
			    Imports.ImportAsync(Source, Output).Get(Tasks);
		    });
		HYP_CHECK(*IO.ReadAsync(Output).Get(Tasks) == *Previous);
		Files->ReplaceOnRead = 0;
		Files->Memory.WriteAtomic(Files->ReplacementPath, Original.Bytes);
		const auto Published = Imports.ImportAsync(Source, Output).Get(Tasks);
		HYP_CHECK(Published->Header.Dependencies.size() == 2);
		HYP_CHECK(Published->Header.Dependencies[0].Reference.Path == "/Game/External.hasset");
		HYP_CHECK(Published->Header.Dependencies[0].Reference.Revision.empty());
		FAssetService Assets(IO);
		Assets.Types().Register<FImportFixture>();
		HYP_CHECK(Assets.LoadGraphAsync(Output).Get(Tasks)->Failures.empty());
	}
}

void CheckExternalCachedIdentities(FTaskSystem& InTasks, FIOService& InIO, FAssetImportService& InImports,
                                   FImportStorage& InFiles, const FAssetRef& InValid)
{
	const auto Source = std::filesystem::absolute("external-cache/root.source");
	const auto Output = std::filesystem::absolute("external-cache/root.hasset");
	const auto Previous = InIO.ReadAsync(Output).Get(InTasks);
	FAssetImportOptions Options;
	Options.bForce = true;
	for (unsigned Case = 0; Case < 3; ++Case)
	{
		auto Invalid = InValid;
		if (Case == 0)
		{
			Invalid.Id[0] = Invalid.Id[0] == '0' ? '1' : '0';
		}
		else if (Case == 1)
		{
			Invalid.Revision[0] = Invalid.Revision[0] == '0' ? '1' : '0';
		}
		else
		{
			Invalid.TypeId = "test.other";
		}
		InIO.WriteAsync(Source, Serialize(FImportFixture{"root", "", {InValid, Invalid}})).Get(InTasks);
		InFiles.ReadCounts.clear();
		std::string Error;
		try
		{
			InImports.ImportAsync(Source, Output, Options).Get(InTasks);
		}
		catch (const std::runtime_error& Failure)
		{
			Error = Failure.what();
		}
		HYP_CHECK(Error.find("External asset reference identity/type/revision mismatch") != std::string::npos);
		// The first reference loads and validates the graph; the invalid cache hit prevents commit.
		HYP_CHECK(InFiles.ReadCounts.at(InValid.Path) == 2);
		HYP_CHECK(*InIO.ReadAsync(Output).Get(InTasks) == *Previous);
	}
}

void CheckExternalPublicationCache()
{
	FTaskSystem Tasks(1, 1);
	auto Files = std::make_shared<FImportStorage>();
	FIOService IO(Tasks, Files);
	FAssetImportService Imports(IO);
	Imports.Register(FixtureImporter());
	const auto Source = std::filesystem::absolute("external-cache/root.source");
	const auto Output = std::filesystem::absolute("external-cache/root.hasset");
	const auto Type = RecordType<FImportFixture>().Id;
	const std::filesystem::path LeafPath = "/Game/CachedLeaf.hasset";
	const std::filesystem::path ExternalPath = "/Game/CachedExternal.hasset";
	const auto Leaf = StoreNative(IO, LeafPath, {"leaf"});
	FImportFixture Child{"external", "", {{Leaf.Id, "/Game/CachedLeaf.hasset", Type, Leaf.Revision}}};
	const auto External = StoreNative(IO, ExternalPath, Child);
	const FAssetRef Reference{"", "/Game/CachedExternal.hasset", Type, ""};
	FAssetImportOptions Options;
	Options.bForce = true;
	for (const std::size_t Count : {1, 10, 100})
	{
		FImportFixture Root{"root", "", std::vector<FAssetRef>(Count, Reference)};
		IO.WriteAsync(Source, Serialize(Root)).Get(Tasks);
		Files->ReadCounts.clear();
		const auto Published = Imports.ImportAsync(Source, Output, Options).Get(Tasks);
		HYP_CHECK(Published->Header.Dependencies.size() == Count);
		for (const auto& Dependency : Published->Header.Dependencies)
		{
			HYP_CHECK(Dependency.Reference.Id == External.Id && Dependency.Reference.Revision.empty());
		}
		// Per graph asset: one load, one snapshot verification, one final CheckSources read.
		HYP_CHECK(Files->ReadCounts.at(ExternalPath) == 3 && Files->ReadCounts.at(LeafPath) == 3);
	}
	Child.Name = "external changed between publications";
	const auto Changed = StoreNative(IO, ExternalPath, Child, External);
	Files->ReadCounts.clear();
	const auto Republished = Imports.ImportAsync(Source, Output, Options).Get(Tasks);
	HYP_CHECK(Changed.Id == External.Id && Changed.Revision != External.Revision);
	HYP_CHECK(Republished->Header.Dependencies.front().Reference.Revision.empty());
	HYP_CHECK(Files->ReadCounts.at(ExternalPath) == 3 && Files->ReadCounts.at(LeafPath) == 3);
	CheckExternalCachedIdentities(Tasks, IO, Imports, *Files,
	                              {Changed.Id, "/Game/CachedExternal.hasset", Type, Changed.Revision});
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
	Rejects(
	    [&]
	    {
		    Imports.ImportAsync(Source, Output).Get(Tasks);
	    });
	HYP_CHECK(!IO.FileSystem()->Exists(Output));
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
	// Generated sky lights reference the Engine default sky, so scene publication resolves /Engine.
	FIOService IO(Tasks, std::make_shared<FMountedFileSystem>(std::vector<FContentMount>{
	                         {"/Engine", std::filesystem::path(HYP_SOURCE_DIR) / "Content"}}));
	FAssetImportService Imports(IO);
	RegisterGltfImporter(Imports);
	RegisterSkyImporter(Imports);
	const auto Source = std::filesystem::path(HYP_SOURCE_DIR) / "out/fixtures/Showcase.gltf";
	const auto Output = std::filesystem::absolute("publication-model-scene/scene.hasset");
	FAssetImportOptions Options;
	Options.bScene = true;
	Options.bForce = true;
	Options.Name = "Imported model scene";
	const auto Result = Imports.ImportAsync(Source, Output, Options).Get(Tasks);
	HYP_CHECK(Result->Header.TypeId == RecordType<FSceneManifest>().Id && Result->Header.Dependencies.size() == 2);
	const auto ModelDependency = std::find_if(Result->Header.Dependencies.begin(), Result->Header.Dependencies.end(),
	                                          [](const FAssetDependency& InDependency)
	                                          {
		                                          return InDependency.Reference.TypeId == RecordType<FModelAsset>().Id;
	                                          });
	HYP_CHECK(ModelDependency != Result->Header.Dependencies.end());
	HYP_CHECK(std::any_of(Result->Header.Dependencies.begin(), Result->Header.Dependencies.end(),
	                      [](const FAssetDependency& InDependency)
	                      {
		                      return InDependency.Reference.Path == DefaultSkyReference().Path;
	                      }));
	FAssetService Assets(IO);
	RegisterSceneAssetTypes(Assets.Types());
	const auto Graph = Assets.LoadGraphAsync(Output).Get(Tasks);
	HYP_CHECK(Graph->Failures.empty());
	const auto Model = Assets.LoadReferenceAsync<FModelAsset>(ModelDependency->Reference, Output).Get(Tasks);
	HYP_CHECK(ModelInstances(*Model).size() == 4 && Model->MaterialSlots.size() == 4);
	const auto& Scene = *Graph->Root->As<FSceneManifest>();
	HYP_CHECK(Scene.Nodes.size() == 3 && SceneModelCount(Scene) == 1 && Scene.DefaultCamera.empty());
	const auto Found = std::find_if(Scene.Nodes.begin(), Scene.Nodes.end(),
	                                [](const auto& InNode)
	                                {
		                                return InNode.Model.has_value();
	                                });
	HYP_CHECK(Found != Scene.Nodes.end() && Found->Id == "instance");
	HYP_CHECK(Found->Model->SourceNode.empty() && Found->Model->SourcePrimitive.empty());
}

void CheckNativeSourcesRejected()
{
	FTaskSystem Tasks(1, 1);
	auto Files = std::make_shared<FMemoryFileSystem>();
	FIOService IO(Tasks, Files);
	FAssetImportService Imports(IO);
	RegisterGltfImporter(Imports);
	RegisterSkyImporter(Imports);
	const auto Source = std::filesystem::absolute("Native.hasset");
	const auto Output = std::filesystem::absolute("Rejected.hasset");
	const FSceneManifest Scene;
	const auto Bytes = EncodeAsset(RecordType<FSceneManifest>(), &Scene).Bytes;
	IO.WriteAsync(Source, Bytes).Get(Tasks);
	for (const auto& Type :
	     {std::string{}, RecordType<FModelAsset>().Id, RecordType<FTextureAsset>().Id, RecordType<FSkyAsset>().Id,
	      RecordType<FMaterialAsset>().Id, RecordType<FSceneManifest>().Id})
	{
		FAssetImportOptions Options;
		Options.TypeId = Type;
		Rejects(
		    [&]
		    {
			    Imports.ImportAsync(Source, Output, Options).Get(Tasks);
		    });
		HYP_CHECK(!Files->Exists(Output));
		HYP_CHECK(*IO.ReadAsync(Source).Get(Tasks) == Bytes);
	}
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
	HYP_CHECK(First->Header.Id == Second->Header.Id && First->Header.Revision == Second->Header.Revision);
	Assets.Invalidate(Output);
	const auto After = Assets.LoadAsync<FModelAsset>(Output).Get(Tasks);
	HYP_CHECK(After->MaterialSlots == Before->MaterialSlots);
	HYP_CHECK(After->Primitives[0].Positions == Before->Primitives[0].Positions);
}

void CheckRollback()
{
	FTaskSystem Tasks(1, 1);
	auto Files = std::make_shared<FImportStorage>();
	FIOService IO(Tasks, Files);
	FAssetImportService Imports(IO);
	Imports.Register(FixtureImporter());
	const auto Directory = std::filesystem::absolute("rollback");
	const auto Source = Directory / "Root.source";
	const auto Output = Directory / "native/Root.hasset";
	const auto Type = RecordType<FImportFixture>().Id;
	FImportFixture Parent{"parent", "", {{"", "A.source", Type, ""}, {"", "B.source", Type, ""}}};
	IO.WriteAsync(Source, Serialize(Parent)).Get(Tasks);
	for (const auto* Name : {"A", "B", "C"})
	{
		IO.WriteAsync(Directory / (std::string(Name) + ".source"), Serialize(FImportFixture{Name})).Get(Tasks);
	}
	Imports.ImportAsync(Source, Output).Get(Tasks);
	std::map<std::filesystem::path, FBytes> Before;
	for (const auto& Path : Files->Memory.Enumerate(Output.parent_path(), true))
	{
		Before.emplace(Path, Files->Memory.Read(Path, 1024 * 1024));
	}
	Parent.Children.push_back({"", "C.source", Type, ""});
	IO.WriteAsync(Source, Serialize(Parent)).Get(Tasks);
	IO.WriteAsync(Directory / "A.source", Serialize(FImportFixture{"A", "", {}, {42}})).Get(Tasks);
	IO.WriteAsync(Directory / "B.source", Serialize(FImportFixture{"B", "", {}, {42}})).Get(Tasks);
	for (const unsigned Failure : {3u, 4u})
	{
		Files->WriteCalls = 0;
		Files->FailOnWrite = Failure;
		Rejects(
		    [&]
		    {
			    Imports.ImportAsync(Source, Output).Get(Tasks);
		    });
		HYP_CHECK(Files->Memory.Enumerate(Output.parent_path(), true).size() == Before.size());
		for (const auto& [Path, Bytes] : Before)
		{
			HYP_CHECK(Files->Memory.Read(Path, 1024 * 1024) == Bytes);
		}
	}
	Files->FailOnWrite = 0;
	Imports.ImportAsync(Source, Output).Get(Tasks);
	FAssetService Assets(IO);
	Assets.Types().Register<FImportFixture>();
	HYP_CHECK(Assets.LoadGraphAsync(Output).Get(Tasks)->Assets.size() == 4);
}

void CheckGroupedRollback()
{
	FTaskSystem Tasks(1, 1);
	auto Files = std::make_shared<FImportStorage>();
	FIOService IO(Tasks, Files);
	FAssetImportService Imports(IO);
	Imports.Register(FixtureImporter());
	const auto Directory = std::filesystem::absolute("grouped-rollback");
	const auto Source = Directory / "Root.source";
	const auto Output = Directory / "native/Root.hasset";
	const auto Type = RecordType<FImportFixture>().Id;
	IO.WriteAsync(Source, Serialize(FImportFixture{"root", "", {{"", "Child.source", Type, ""}}})).Get(Tasks);
	IO.WriteAsync(Directory / "Child.source", Serialize(FImportFixture{"child"})).Get(Tasks);
	FAssetImportOptions Options;
	Options.bCreateFolder = true;
	// Child, ownership marker, root: failure at or after the marker must roll everything back.
	for (const unsigned Failure : {2u, 3u})
	{
		Files->WriteCalls = 0;
		Files->FailOnWrite = Failure;
		Rejects(
		    [&]
		    {
			    Imports.ImportAsync(Source, Output, Options).Get(Tasks);
		    });
		HYP_CHECK(Files->Memory.Enumerate(Output.parent_path(), true).empty());
	}
	Files->FailOnWrite = 0;
	const auto Result = Imports.ImportAsync(Source, Output, Options).Get(Tasks);
	HYP_CHECK(Result->Output == Output.parent_path() / "Root/Root.hasset");
	HYP_CHECK(Result->WrittenAssets == 2);
	HYP_CHECK(Files->Memory.Read(Output.parent_path() / "Root/.import-source", 64).size() == 64);
	HYP_CHECK(Imports.ImportAsync(Source, Output, Options).Get(Tasks)->bUpToDate);
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
		const auto Work = std::filesystem::absolute("publication-current") / CreateIdentifier();
		std::filesystem::create_directories(Work);
		std::filesystem::current_path(Work);
		CheckSceneSources();
		CheckPublication();
		CheckExternalPublicationChanges();
		CheckExternalPublicationCache();
		CheckCrossVolumeImport();
		CheckCyclesAndOrdering();
		CheckModelToScene();
		CheckModelReferences();
		CheckNativeSourcesRejected();
		CheckLocalLease();
		CheckRollback();
		CheckGroupedRollback();
		CheckExternalImageReimport();
		std::cout
		    << "Generic incremental publication, source changes, current shared contents, failure and leases passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
