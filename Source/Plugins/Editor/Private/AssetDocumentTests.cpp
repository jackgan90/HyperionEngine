#include "AssetDocument.h"
#include "Hyperion/Math/AffineTransform.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Hyperion/Textures/TextureAsset.h"
#include <chrono>
#include <iostream>
#include <source_location>
#include <thread>

namespace Hyperion
{
void WriteAssetEditorFixture(const std::filesystem::path& InRoot);
}

namespace
{
using namespace Hyperion;

void Check(bool bInCondition, std::source_location InLocation = std::source_location::current())
{
	if (!bInCondition)
	{
		throw std::runtime_error("Asset document check failed: " + std::to_string(InLocation.line()));
	}
}

void WaitForSave(FAssetEditorDocument& InDocument, FTaskSystem& InTasks)
{
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	while (InDocument.IsSaving())
	{
		InTasks.PumpMain();
		InDocument.PollSave();
		Check(std::chrono::steady_clock::now() < Deadline);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

void CheckDocuments(FAssetService& InAssets, FTaskSystem& InTasks, FMemoryFileSystem& InFiles)
{
	const auto Path = InAssets.NormalizePath("asset-document.hasset");
	FMaterialTextureMip Base{2, 2, {0, 0, 0, 255, 255, 255, 255, 255, 128, 128, 128, 255, 64, 64, 64, 255}};
	const auto Texture = BuildTextureAsset("Original", EMaterialTextureEncoding::Linear, Base);
	const auto Encoded = EncodeAsset(RecordType<FTextureAsset>(), &Texture);
	InFiles.WriteAtomic(Path, Encoded.Bytes);
	InAssets.Types().Register<FTextureAsset>();
	FAssetEditorDocument A(InAssets.LoadAsync(Path).Get(InTasks));
	FAssetEditorDocument B(InAssets.LoadAsync(Path).Get(InTasks));
	A.Set("name", WriteValue(std::string("First")), 41);
	A.Set("name", WriteValue(std::string("Second")), 41);
	Check(A.IsDirty() && !B.IsDirty());
	Check(A.Undo() && !A.IsDirty() && !A.CanUndo());
	Check(A.Redo() && ReadValue<std::string>(A.Get("name")) == "Second");
	A.Set("name", WriteValue(std::string("Cancelled")), 42);
	A.CancelInteraction();
	Check(ReadValue<std::string>(A.Get("name")) == "Second" && !A.CanRedo());
	A.Save(InAssets);
	A.Set("name", WriteValue(std::string("Edited during save")), 43);
	WaitForSave(A, InTasks);
	Check(A.Error.empty() && A.IsDirty());
	Check(InAssets.LoadAsync<FTextureAsset>(Path).Get(InTasks)->Name == "Second");
	Check(A.Undo() && !A.IsDirty());
	Check(A.Redo() && A.IsDirty());
	A.Save(InAssets);
	WaitForSave(A, InTasks);
	Check(!A.IsDirty() && A.Error.empty());
	B.Set("name", WriteValue(std::string("Stale")));
	B.Save(InAssets);
	WaitForSave(B, InTasks);
	Check(B.IsDirty() && B.Error.find("changed on disk") != std::string::npos);
	const auto Before = Serialize(ReadValue<FTextureAsset>(A.Snapshot()));
	const auto Rebuilt = BuildTextureAsset("Encoded", EMaterialTextureEncoding::Srgb, Base);
	A.Set({}, WriteValue(Rebuilt));
	Check(ReadValue<FTextureAsset>(A.Snapshot()).Mips.front() == Base);
	Check(A.Undo() && Serialize(ReadValue<FTextureAsset>(A.Snapshot())) == Before);
	Check(A.Redo() && ReadValue<FTextureAsset>(A.Snapshot()).Mips == Rebuilt.Mips);
	InFiles.Remove(Path);
	A.Save(InAssets);
	WaitForSave(A, InTasks);
	Check(A.IsDirty() && A.Error.find("removed from disk") != std::string::npos);
}

void CheckModelEdits(FAssetService& InAssets, FTaskSystem& InTasks, FMemoryFileSystem& InFiles)
{
	RegisterSceneAssetTypes(InAssets.Types());
	FModelAsset Model;
	Model.Name = "Triangle";
	FMaterialDescription Description;
	Description.Name = "Model test material";
	FMaterialPass Pass;
	Pass.Vertex = {"ModelTest.hlsl", "Vertex"};
	Pass.Pixel = {"ModelTest.hlsl", "Pixel"};
	Description.Passes.push_back(Pass);
	const auto Material = PersistMaterialDescription(Description);
	const auto MaterialBytes = EncodeAsset(RecordType<FMaterialAsset>(), &Material);
	InFiles.WriteAtomic(InAssets.NormalizePath("model-material.hasset"), MaterialBytes.Bytes);
	Model.MaterialSlots.push_back({MaterialBytes.Header.Id, "model-material.hasset", MaterialBytes.Header.TypeId, {}});
	FModelPrimitive Primitive;
	Primitive.Material = 0;
	Primitive.Positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
	Primitive.Indices = {0, 1, 2};
	Model.Primitives.push_back(Primitive);
	FAffineTransform Transform;
	Transform.Shear = {.25f, .5f, .75f};
	Model.Nodes.push_back({"Root", ComposeAffine(Transform), {0}, {}, "stable-node"});
	Model.Roots = {0};
	AssignModelSubresourceIds(Model);
	const auto Path = InAssets.NormalizePath("model-document.hasset");
	InFiles.WriteAtomic(Path, EncodeAsset(RecordType<FModelAsset>(), &Model).Bytes);
	FAssetEditorDocument Document(InAssets.LoadAsync(Path).Get(InTasks));
	auto Nodes = ReadValue<std::vector<FModelNode>>(Document.Get("nodes"));
	auto Edited = DecomposeAffine(Nodes.front().Local);
	Edited.Position = {3, 4, 5};
	Edited.Rotation = {.1f, .2f, .3f};
	Edited.Scale = {2, 3, 4};
	Nodes.front().Name = "Renamed";
	Nodes.front().Local = ComposeAffine(Edited);
	ValidateNodeHierarchy(Nodes);
	Document.Set("nodes", WriteValue(Nodes));
	Check(Document.Undo() && !Document.IsDirty());
	Check(Serialize(ReadValue<FModelAsset>(Document.Snapshot())) == Serialize(Model));
	Check(Document.Redo());
	Document.Save(InAssets);
	WaitForSave(Document, InTasks);
	Check(!Document.IsDirty() && Document.Error.empty());
	const auto Saved = InAssets.LoadAsync<FModelAsset>(Path).Get(InTasks);
	const auto Actual = DecomposeAffine(Saved->Nodes.front().Local);
	Check(Saved->Nodes.front().Id == "stable-node" && Saved->Nodes.front().Name == "Renamed");
	Check(Saved->Primitives.front().Id == Model.Primitives.front().Id);
	Check(Length(Subtract(Actual.Position, Edited.Position)) < .0001f &&
	      Length(Subtract(Actual.Shear, Edited.Shear)) < .0001f);
	Check(Saved->Primitives.front().Positions == Primitive.Positions);
}

void CheckObsoleteCatalog(FAssetService& InAssets, FTaskSystem& InTasks, FMemoryFileSystem& InFiles)
{
	RegisterSceneAssetTypes(InAssets.Types());
	auto Type = RecordType<FTextureAsset>();
	Type.Id = "hyperion.assetcatalog";
	const auto Texture = BuildTextureAsset("Obsolete", EMaterialTextureEncoding::Linear, {1, 1, {0, 0, 0, 255}});
	const auto Encoded = EncodeAsset(Type, &Texture);
	const auto Path = InAssets.NormalizePath("obsolete-catalog.hasset");
	InFiles.WriteAtomic(Path, Encoded.Bytes);
	bool bRejected{};
	try
	{
		InAssets.LoadAsync(Path).Get(InTasks);
	}
	catch (const std::exception& Failure)
	{
		bRejected = std::string(Failure.what()).find("hyperion.assetcatalog") != std::string::npos;
	}
	Check(bRejected && InFiles.Read(Path, 1024 * 1024) == Encoded.Bytes);
}
} // namespace

int main()
{
	try
	{
		FTaskSystem Tasks(2, 1);
		auto Files = std::make_shared<FMemoryFileSystem>();
		FIOService IO(Tasks, Files);
		FAssetService Assets(IO);
		CheckDocuments(Assets, Tasks, *Files);
		CheckModelEdits(Assets, Tasks, *Files);
		CheckObsoleteCatalog(Assets, Tasks, *Files);
		WriteAssetEditorFixture(std::filesystem::current_path() / "editor-asset-tests");
		Assets.Drain();
		Tasks.Shutdown();
		std::cout << "Asset document transactions, isolation, save races and conflicts passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
