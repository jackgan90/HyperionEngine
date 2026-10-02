#include "Hyperion/AssetEditing/AssetDocument.h"
#include "Hyperion/AssetEditing/AssetEditWorkflow.h"
#include "Hyperion/AssetEditing/AssetProperties.h"
#include "Hyperion/AssetEditing/ModelProperties.h"
#include "Hyperion/Environment/SkyAsset.h"
#include "Hyperion/Math/AffineTransform.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Hyperion/Textures/TextureAsset.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <source_location>
#include <thread>
#include <variant>

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

void WaitForSave(FAssetEditDocument& InDocument, FTaskSystem& InTasks)
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

void RebuildEncoding(FTaskSystem& InTasks, const std::shared_ptr<FAssetEditDocument>& InDocument,
                     EMaterialTextureEncoding InEncoding)
{
	const auto Work = FAssetEditWorkflow::Encoding(InTasks, InDocument, InDocument->Generation(), InEncoding);
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	while (!Work->Poll(InDocument))
	{
		InTasks.PumpMain();
		Check(std::chrono::steady_clock::now() < Deadline);
		std::this_thread::yield();
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
	const auto Document = std::make_shared<FAssetEditDocument>(InAssets.LoadAsync(Path).Get(InTasks));
	auto& A = *Document;
	FAssetEditDocument B(InAssets.LoadAsync(Path).Get(InTasks));
	const auto InitialPreview = A.PreviewGeneration();
	A.Set("name", WriteValue(std::string("First")), 41);
	A.Set("name", WriteValue(std::string("Second")), 41);
	Check(A.IsDirty() && !B.IsDirty());
	Check(A.Undo() && !A.IsDirty() && !A.CanUndo());
	Check(A.Redo() && ReadValue<std::string>(A.Get("name")) == "Second");
	Check(A.PreviewGeneration() == InitialPreview);
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
	const auto Rebuilt = BuildTextureAsset(ReadValue<std::string>(A.Get("name")), EMaterialTextureEncoding::Srgb, Base);
	RebuildEncoding(InTasks, Document, EMaterialTextureEncoding::Srgb);
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
	FAssetEditDocument Document(InAssets.LoadAsync(Path).Get(InTasks));
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
	const auto Preview = Document.PreviewGeneration();
	Nodes.front().Name = "Metadata only";
	Document.Set("nodes", WriteValue(Nodes));
	Check(Document.IsDirty() && Document.PreviewGeneration() == Preview);
	Check(Document.Undo() && !Document.IsDirty() && Document.PreviewGeneration() == Preview);
	Check(Document.Redo() && Document.PreviewGeneration() == Preview);
}

void CheckTextureHistorySharing(FAssetService& InAssets, FTaskSystem& InTasks, FMemoryFileSystem& InFiles)
{
	const auto Path = InAssets.NormalizePath("texture-history.hasset");
	FMaterialTextureMip Base{128, 128, std::vector<std::uint8_t>(128 * 128 * 4, 127)};
	const auto Texture = BuildTextureAsset("Shared history", EMaterialTextureEncoding::Linear, Base);
	InFiles.WriteAtomic(Path, EncodeAsset(RecordType<FTextureAsset>(), &Texture).Bytes);
	const auto SharedDocument = std::make_shared<FAssetEditDocument>(InAssets.LoadAsync(Path).Get(InTasks));
	auto& Document = *SharedDocument;
	const auto Generation = Document.Generation();
	const auto Preview = Document.PreviewGeneration();
	const auto BaseStorage = [&]()
	{
		const auto& Mip = std::get<FArchiveNode::FArray>(Document.Get("mips").Value).front();
		const auto& Fields =
		    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Mip.Value).at("fields").Value);
		return std::get<FBulkData>(Fields.at("bytes").Value).Storage;
	};
	const auto Original = BaseStorage();
	std::vector<std::string> Hashes{HashArchive(Document.Snapshot())};
	for (int Index = 0; Index < 6; ++Index)
	{
		const auto Encoding = Index % 2 ? EMaterialTextureEncoding::Linear : EMaterialTextureEncoding::Srgb;
		RebuildEncoding(InTasks, SharedDocument, Encoding);
		Check(Document.Generation() == Generation + Index + 1 && Document.PreviewGeneration() == Preview + Index + 1);
		Check(BaseStorage() == Original);
		Check(ReadValue<FTextureAsset>(Document.Snapshot()).Mips ==
		      BuildTextureAsset(Texture.Name, Encoding, Base).Mips);
		Hashes.push_back(HashArchive(Document.Snapshot()));
	}
	for (std::size_t Index = Hashes.size() - 1; Index > 0; --Index)
	{
		Check(Document.Undo() && HashArchive(Document.Snapshot()) == Hashes[Index - 1]);
		Check(BaseStorage() == Original);
	}
	for (std::size_t Index = 1; Index < Hashes.size(); ++Index)
	{
		Check(Document.Redo() && HashArchive(Document.Snapshot()) == Hashes[Index]);
		Check(BaseStorage() == Original);
	}
}

void CheckEqualAndCoalescedHistory(FAssetService& InAssets, FTaskSystem& InTasks)
{
	FAssetEditDocument Document(InAssets.LoadAsync("model-document.hasset").Get(InTasks));
	const auto Initial = Document.Snapshot();
	const auto Generation = Document.Generation();
	const auto Preview = Document.PreviewGeneration();
	CommitAssetField(Document, "name", Document.Get("name"));
	Check(Document.Generation() == Generation + 1 && Document.PreviewGeneration() == Preview);
	Check(Document.IsDirty() && Document.CanUndo() && EqualInspectionValue(Document.Snapshot(), Initial));
	Check(Document.Undo() && !Document.IsDirty() && !Document.CanUndo());
	Check(Document.Generation() == Generation + 2 && Document.PreviewGeneration() == Preview);
	auto Nodes = ReadValue<std::vector<FModelNode>>(Document.Get("nodes"));
	Nodes.front().Name = "Metadata in drag";
	CommitAssetField(Document, "nodes", WriteValue(Nodes), 71);
	Nodes.front().Local = Translation({7, 8, 9});
	CommitAssetField(Document, "nodes", WriteValue(Nodes), 71);
	Nodes = ReadValue<FModelAsset>(Initial).Nodes;
	CommitAssetField(Document, "nodes", WriteValue(Nodes), 71);
	Check(Document.Generation() == Generation + 5 && Document.PreviewGeneration() == Preview + 2);
	Check(Document.IsDirty() && Document.CanUndo() && !Document.CanRedo());
	Check(EqualInspectionValue(Document.Snapshot(), Initial));
	Check(Document.Undo() && !Document.IsDirty() && !Document.CanUndo());
	Check(Document.Generation() == Generation + 6 && Document.PreviewGeneration() == Preview + 3);
	Check(Document.Redo() && Document.IsDirty() && !Document.CanRedo());
	Check(Document.Generation() == Generation + 7 && Document.PreviewGeneration() == Preview + 4);
	Nodes.front().Name = "Cancelled metadata";
	CommitAssetField(Document, "nodes", WriteValue(Nodes), 72);
	Nodes.front().Local = Translation({11, 12, 13});
	CommitAssetField(Document, "nodes", WriteValue(Nodes), 72);
	Document.CancelInteraction(72);
	Check(Document.Generation() == Generation + 10 && Document.PreviewGeneration() == Preview + 6);
	Check(Document.IsDirty() && Document.CanUndo() && !Document.CanRedo());
	Check(EqualInspectionValue(Document.Snapshot(), Initial));
	Check(Document.Undo() && !Document.IsDirty() && !Document.CanUndo());
	Check(Document.Generation() == Generation + 11 && Document.PreviewGeneration() == Preview + 7);
}

template<class TFunction> void Reject(TFunction InFunction)
{
	bool bRejected{};
	try
	{
		InFunction();
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	catch (const std::runtime_error&)
	{
		bRejected = true;
	}
	catch (const std::bad_variant_access&)
	{
		bRejected = true;
	}
	Check(bRejected);
}

void CheckFieldPolicyIdentity()
{
	auto Type = RecordType<FModelAsset>();
	const auto Nodes = ResolveAssetFieldPolicy(Type, &FModelAsset::Nodes);
	Check(Nodes.Field().FieldId == "nodes" && Nodes.Field().TypeId == "hyperion.modelasset");
	Check(Nodes.Route() == EAssetFieldRoute::Field);
	Check(ResolveAssetFieldPolicy(Type, &FModelAsset::Primitives).Route() == EAssetFieldRoute::ModelPrimitives);
	Check(ResolveAssetFieldPolicy(Type, &FModelAsset::Roots).Route() == EAssetFieldRoute::ReadOnly);
	Check(ResolveAssetFieldPolicy(RecordType<FTextureAsset>(), &FTextureAsset::Encoding).Route() ==
	      EAssetFieldRoute::TextureEncoding);
	Check(ResolveAssetFieldPolicy(RecordType<FSkyAsset>(), &FSkyAsset::Radiance).Route() == EAssetFieldRoute::ReadOnly);
	std::reverse(Type.Members.begin(), Type.Members.end());
	Check(ResolveAssetFieldPolicy(Type, Nodes.Field()).Field() == Nodes.Field());
	const auto Found = std::find_if(Type.Members.begin(), Type.Members.end(),
	                                [](const auto& InMember)
	                                {
		                                return InMember.Id == "nodes";
	                                });
	Found->Options.Aliases.push_back("oldNodes");
	Reject(
	    [&]
	    {
		    (void)ResolveAssetFieldPolicy(Type, "oldNodes");
	    });
	Found->Association = FRecordMemberAssociation(&FModelAsset::Roots);
	Reject(
	    [&]
	    {
		    (void)ResolveAssetFieldPolicy(Type, Nodes.Field());
	    });
	Type = RecordType<FModelAsset>();
	Type.Definition = std::make_shared<const int>(0);
	Reject(
	    [&]
	    {
		    (void)ResolveAssetFieldPolicy(Type, Nodes.Field());
	    });
	const auto Foreign = ResolveRecordMember(Type, &FModelAsset::Nodes);
	Reject(
	    [&]
	    {
		    (void)ResolveAssetFieldPolicy(Type, Foreign);
	    });
	Reject(
	    [&]
	    {
		    (void)ResolveAssetFieldPolicy(RecordType<FTextureAsset>(), Nodes.Field());
	    });
	std::vector<FModelNode> FModelAsset::* NullMember{};
	Reject(
	    [&]
	    {
		    (void)ResolveAssetFieldPolicy(RecordType<FModelAsset>(), NullMember);
	    });
	Type = RecordType<FModelAsset>();
	Type.Id = "custom.model";
	Reject(
	    [&]
	    {
		    (void)AssetNamePolicy(Type);
	    });
}

template<class TFunction> void RejectUnchanged(FAssetEditDocument& InDocument, TFunction InFunction)
{
	const auto Before = InDocument.Snapshot();
	const auto Generation = InDocument.Generation();
	const auto Preview = InDocument.PreviewGeneration();
	Reject(InFunction);
	Check(EqualInspectionValue(InDocument.Snapshot(), Before));
	Check(InDocument.Generation() == Generation && InDocument.PreviewGeneration() == Preview);
	Check(!InDocument.IsEditing() && !InDocument.IsDirty() && !InDocument.CanUndo() && !InDocument.CanRedo());
}

void CheckFieldProtections(FAssetService& InAssets, FTaskSystem& InTasks)
{
	FAssetEditDocument Model(InAssets.LoadAsync("model-document.hasset").Get(InTasks));
	RejectUnchanged(Model,
	                [&]
	                {
		                Model.Set({}, Model.Snapshot());
	                });
	RejectUnchanged(Model,
	                [&]
	                {
		                Model.Set("primitives", Model.Get("primitives"));
	                });
	RejectUnchanged(Model,
	                [&]
	                {
		                Model.Set("roots", Model.Get("roots"));
	                });
	RejectUnchanged(Model,
	                [&]
	                {
		                Model.Set("name", WriteValue(3));
	                });
	auto Nodes = ReadValue<std::vector<FModelNode>>(Model.Get("nodes"));
	Nodes.front().Id = "changed-id";
	RejectUnchanged(Model,
	                [&]
	                {
		                Model.Set("nodes", WriteValue(Nodes));
	                });
	Nodes = ReadValue<std::vector<FModelNode>>(Model.Get("nodes"));
	Nodes.front().Children.push_back(0);
	RejectUnchanged(Model,
	                [&]
	                {
		                Model.Set("nodes", WriteValue(Nodes));
	                });
	auto Primitives = DescribeModelPrimitives(Model);
	Primitives.front().Vertices += 1;
	RejectUnchanged(Model,
	                [&]
	                {
		                SetModelPrimitives(Model, Primitives);
	                });
	Primitives = DescribeModelPrimitives(Model);
	Primitives.front().Material = -2;
	RejectUnchanged(Model,
	                [&]
	                {
		                SetModelPrimitives(Model, Primitives);
	                });
	Primitives.front().Material = 1;
	RejectUnchanged(Model,
	                [&]
	                {
		                SetModelPrimitives(Model, Primitives);
	                });
	FAssetEditDocument Texture(InAssets.LoadAsync("texture-history.hasset").Get(InTasks));
	RejectUnchanged(Texture,
	                [&]
	                {
		                Texture.Set("mips", Texture.Get("mips"));
	                });
	RejectUnchanged(Texture,
	                [&]
	                {
		                Texture.Set("encoding", Texture.Get("encoding"));
	                });
	FAssetEditDocument Material(InAssets.LoadAsync("model-material.hasset").Get(InTasks));
	RejectUnchanged(Material,
	                [&]
	                {
		                Material.Set("parameters", Material.Get("parameters"));
	                });
	RejectUnchanged(Material,
	                [&]
	                {
		                Material.Set("passes", Material.Get("passes"));
	                });
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
		CheckEqualAndCoalescedHistory(Assets, Tasks);
		CheckTextureHistorySharing(Assets, Tasks, *Files);
		CheckFieldPolicyIdentity();
		CheckFieldProtections(Assets, Tasks);
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
