#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include "Support/ModelAssetSupport.h"
#include "Support/ShaderSourceSupport.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

using namespace Hyperion;

namespace
{
std::shared_ptr<FModelSource> MakeModel()
{
	auto Model = std::make_shared<FModelSource>();
	FModelPrimitive Primitive;
	Primitive.Positions = {-1, -1, 0, 1, -1, 0, 0, 1, 0};
	Primitive.Indices = {0, 1, 2};
	Primitive.Material = 0;
	Model->Primitives.push_back(Primitive);
	Model->Materials.push_back({});
	FModelNode Node;
	Node.Primitives = {0};
	Model->Nodes.push_back(Node);
	Model->Roots = {0};
	return Model;
}

class FGateFileSystem final : public IFileSystem
{
public:
	std::atomic<bool> bRelease{false};
	std::atomic<bool> bEntered{false};
	bool bEnabled{};
	std::string GateName = "SceneRuntime.model.hasset";
	FLocalFileSystem Local;

	FBytes Read(const std::filesystem::path& InPath, std::size_t InLimit) override
	{
		if (bEnabled && InPath.filename() == GateName)
		{
			bEntered = true;
			while (!bRelease)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		return Local.Read(InPath, InLimit);
	}

	void WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes) override
	{
		Local.WriteAtomic(InPath, InBytes);
	}
};

struct FSceneFixture
{
	FTaskSystem Tasks{2, 1};
	std::shared_ptr<FGateFileSystem> Files = std::make_shared<FGateFileSystem>();
	FIOService IO{Tasks, Files};
	FAssetService Assets{IO};
	FShaderCompiler Compiler{TestShaderRoot(), "scene-instance-shaders"};
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<FRenderSession> Session;

	FSceneFixture()
	{
		RegisterSceneAssetTypes(Assets.Types());
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          FRHIBackendRegistry Registry;
			                          RegisterD3D12RHIBackend(Registry);
			                          Device = Registry.CreateDevice(ERHIBackend::D3D12);
		                          }));
		Session = std::make_unique<FRenderSession>(Tasks, *Device, Compiler);
		auto Split = SplitModelSource(*MakeModel());
		for (const auto& Product : Split.Products)
		{
			auto Object = ReadRecord(*Product.Type, WriteRecord(*Product.Type, Product.Object.get()));
			VisitRecord(*Product.Type, Object.get(),
			            [](const FRecordDescriptor& InType, const void* InValue, std::string_view)
			            {
				            if (InType.CppType == typeid(FAssetRef))
				            {
					            const_cast<FAssetRef*>(static_cast<const FAssetRef*>(InValue))->Path += ".hasset";
				            }
			            });
			IO.WriteAsync("@" + Product.Key + ".hasset", EncodeAsset(*Product.Type, Object.get()).Bytes).Get(Tasks);
		}
		for (auto& Reference : Split.Model.MaterialSlots)
		{
			Reference.Path += ".hasset";
		}
		IO.WriteAsync("SceneRuntime.model.hasset", EncodeAsset(RecordType<FModelAsset>(), &Split.Model).Bytes)
		    .Get(Tasks);
		FLegacySceneManifest Manifest;
		Manifest.Assets = {{"good", {"", "SceneRuntime.model.hasset", RecordType<FModelAsset>().Id, ""}},
		                   {"bad", {"", "MissingRuntime.hasset", RecordType<FModelAsset>().Id, ""}}};
		Manifest.Instances = {{"one", "good"}, {"two", "good"}, {"failed", "bad"}};
		Manifest.Eye = {0, 1, 7};
		IO.WriteAsync("SceneRuntime.hasset", EncodeAsset(RecordType<FLegacySceneManifest>(), &Manifest).Bytes)
		    .Get(Tasks);
		Files->bEnabled = true;
	}

	~FSceneFixture()
	{
		Files->bRelease = true;
		Assets.Drain();
		Session.reset();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Device.reset();
		                          }));
	}
};

template<typename Predicate> void Await(FSceneInstance& InScene, const Predicate& InPredicate)
{
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (!InPredicate() && std::chrono::steady_clock::now() < Deadline)
	{
		InScene.Tick();
		HYP_CHECK(InScene.GetStatus().Error.empty());
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	HYP_CHECK(InPredicate());
}

void CheckLoadingEdits(FSceneFixture& InFixture)
{
	FSceneInstance Scene(*InFixture.Session, InFixture.Tasks, InFixture.Assets, true);

	// Release the IO gate before Scene unwinds even when an assertion fails.
	struct FRelease
	{
		std::atomic<bool>& bFlag;

		~FRelease()
		{
			bFlag = true;
		}
	} Release{InFixture.Files->bRelease};

	Scene.Load("SceneRuntime.hasset");
	Await(Scene,
	      [&]
	      {
		      return Scene.GetModels().size() == 3 && InFixture.Files->bEntered;
	      });
	const auto Removed = Scene.GetModels()[0].Handle;
	const auto Kept = Scene.GetModels()[1].Handle;
	HYP_CHECK(Scene.Raycast({{0, 0, 5}, {0, 0, -1}, 0, 10}).Status == ESceneRayStatus::Unavailable);
	const auto Failed = Scene.GetModels()[2].Handle;
	HYP_CHECK(Scene.Remove(Removed));
	const auto Added = Scene.Add({"replacement"}, "good");
	HYP_CHECK(Added.Slot == Removed.Slot && Added.Generation != Removed.Generation);
	auto Model = *Scene.Find(Kept);
	Model.World = Translation({4, 0, 0});
	Model.World.Values[4] = .35f;
	Model.Material.Roughness = .27f;
	Model.bVisible = false;
	HYP_CHECK(Scene.Update(Kept, Model));
	const auto Overridden = Scene.Add({"explicit data"}, "good");
	auto Explicit = *Scene.Find(Overridden);
	Explicit.Data = PrepareSourceModel(MakeModel());
	HYP_CHECK(Scene.Update(Overridden, Explicit));
	InFixture.Files->bRelease = true;
	Await(Scene,
	      [&]
	      {
		      return Scene.GetStatus().ReadyModels == 3 && Scene.GetStatus().FailedModels == 1;
	      });
	HYP_CHECK(!Scene.Find(Removed) && !Scene.Remove(Removed));
	HYP_CHECK(Scene.Find(Kept)->World.Values[12] == 4 && !Scene.Find(Kept)->bVisible);
	HYP_CHECK(Scene.Find(Kept)->Data == Scene.Find(Added)->Data);
	HYP_CHECK(Scene.Find(Kept)->Data->QueryGeometry);
	HYP_CHECK(Scene.Find(Kept)->Data->QueryGeometry == Scene.Find(Added)->Data->QueryGeometry);
	const auto QueryHit = Scene.Raycast({{0, 0, 5}, {0, 0, -1}, 0, 10}, {true});
	HYP_CHECK(QueryHit.Status == ESceneRayStatus::Hit && QueryHit.Handle == Added && QueryHit.bIncomplete);
	HYP_CHECK(Scene.Find(Overridden)->Data == Explicit.Data);
	bool bSnapshotRejected{};
	try
	{
		Scene.Snapshot("edited.hasset");
	}
	catch (const std::runtime_error&)
	{
		bSnapshotRejected = true;
	}
	HYP_CHECK(bSnapshotRejected);
	HYP_CHECK(Scene.Remove(Overridden));
	const auto Failure = Scene.GetError(Failed);
	HYP_CHECK(!Failure.empty());
	const auto Assets = Scene.GetAssets();
	HYP_CHECK(std::any_of(Assets.begin(), Assets.end(),
	                      [&](const auto& InAsset)
	                      {
		                      return InAsset.Id == "bad" && InAsset.Error == Failure;
	                      }));
	HYP_CHECK(Scene.GetError(Kept).empty());
	HYP_CHECK(Scene.Remove(Failed));
	const auto Reused = Scene.Add({"reused failed slot"}, "good");
	HYP_CHECK(Reused.Slot == Failed.Slot && Reused.Generation != Failed.Generation);
	HYP_CHECK(Scene.GetError(Failed).empty() && Scene.GetError(Reused).empty());
	HYP_CHECK(Scene.Remove(Reused));
	Scene.Tick();
	HYP_CHECK(Scene.GetStatus().bReady && Scene.GetStatus().Models == 2);
	const auto Snapshot = Scene.Snapshot("subdirectory/edited.hasset");
	HYP_CHECK(Snapshot.Nodes[0].Id == "two" && Snapshot.Nodes[0].Transform.Values[4] == .35f);
	HYP_CHECK(Snapshot.Nodes[0].Model->Material.Roughness == .27f && !Snapshot.Nodes[0].Model->bVisible);
	HYP_CHECK(Snapshot.Assets.size() == 1 && Snapshot.Assets[0].Reference.Path == "../SceneRuntime.model.hasset");
	auto Generic = *Scene.Find(Kept);
	// Snapshot validates representation even when a generic selection cannot be rendered.
	FSceneMaterialSelection Unbacked;
	Unbacked.Snapshot =
	    InFixture.Session->GetResources().PrepareMaterialAsset(Scene.Find(Kept)->Data->Materials.front());
	Generic.SectionSurfaces.emplace(0, std::move(Unbacked));
	Scene.Update(Kept, Generic);
	bSnapshotRejected = false;
	try
	{
		Scene.Snapshot("edited.hasset");
	}
	catch (const std::runtime_error&)
	{
		bSnapshotRejected = true;
	}
	HYP_CHECK(bSnapshotRejected);
	Scene.Close();
	Scene.Close();
	HYP_CHECK(Scene.GetStatus().bClosed && Scene.GetModels().empty());
	Scene.Load("SceneRuntime.hasset");
	Await(Scene,
	      [&]
	      {
		      return Scene.GetStatus().ReadyModels == 2 && Scene.GetStatus().FailedModels == 1;
	      });
	HYP_CHECK(!Scene.Find(Kept));
}

void CheckClosePending(FSceneFixture& InFixture)
{
	InFixture.Assets.ClearCache();
	InFixture.Files->bRelease = false;
	InFixture.Files->bEntered = false;
	FSceneInstance Scene(*InFixture.Session, InFixture.Tasks, InFixture.Assets, true);
	std::jthread Release(
	    [&]
	    {
		    // An admitted file read completes before scene shutdown drains preparation.
		    std::this_thread::sleep_for(std::chrono::milliseconds(100));
		    InFixture.Files->bRelease = true;
	    });
	Scene.Load("SceneRuntime.hasset");
	Await(Scene,
	      [&]
	      {
		      return !Scene.GetModels().empty();
	      });
	Scene.Close();
	Scene.Tick();
	HYP_CHECK(Scene.GetStatus().bClosed && Scene.GetModels().empty());
}

void RejectPendingSnapshot(FSceneInstance& InScene)
{
	bool bRejected{};
	try
	{
		InScene.Snapshot("SavedPending.hasset");
	}
	catch (const std::runtime_error& Error)
	{
		bRejected = std::string(Error.what()).find("pending or failed material selection") != std::string::npos;
	}
	HYP_CHECK(bRejected);
}

void CheckPendingMaterialEdits(FSceneFixture& InFixture)
{
	auto& F = InFixture;
	F.Assets.ClearCache();
	F.Files->bRelease = false;
	F.Files->bEntered = false;
	FLegacySceneManifest Manifest;
	Manifest.Assets = {{"good", {"", "SceneRuntime.model.hasset", RecordType<FModelAsset>().Id, ""}}};
	Manifest.Instances = {{"one", "good"}, {"cleared", "good"}};
	for (auto& Entry : Manifest.Instances)
	{
		Entry.Surface.Reference = FAssetRef{"", "@material-0.hasset", RecordType<FMaterialAsset>().Id, ""};
		Entry.Surface.Overrides = {{"Pbr.RoughnessFactor", PersistMaterialValue(FMaterialValue::Float(.15f))}};
		Entry.SectionSurfaces = {{0, Entry.Surface}};
	}
	F.IO.WriteAsync("PendingMaterial.hasset", EncodeAsset(RecordType<FLegacySceneManifest>(), &Manifest).Bytes)
	    .Get(F.Tasks);
	FSceneInstance Scene(*F.Session, F.Tasks, F.Assets);

	struct FRelease
	{
		std::atomic<bool>& bFlag;

		~FRelease()
		{
			bFlag = true;
		}
	} Release{F.Files->bRelease};

	Scene.Load("PendingMaterial.hasset");
	Await(Scene,
	      [&]
	      {
		      return Scene.GetModels().size() == 2 && F.Files->bEntered;
	      });
	RejectPendingSnapshot(Scene);
	const auto Handle = Scene.GetModels()[0].Handle;
	auto Edited = *Scene.Find(Handle);
	Edited.World = Translation({2, 0, 0});
	Edited.Surface.Overrides = {{"Pbr.RoughnessFactor", FMaterialValue::Float(.62f)}};
	Edited.SectionSurfaces[0].Overrides = {{"Pbr.MetallicFactor", FMaterialValue::Float(.4f)}};
	HYP_CHECK(Scene.Update(Handle, Edited));
	const auto Cleared = Scene.GetModels()[1].Handle;
	HYP_CHECK(Scene.Update(Cleared, Edited));
	Edited.Surface = {};
	Edited.SectionSurfaces.clear();
	HYP_CHECK(Scene.Update(Cleared, Edited));
	F.Files->bRelease = true;
	Await(Scene,
	      [&]
	      {
		      return Scene.GetStatus().ReadyModels == 2;
	      });
	HYP_CHECK(Scene.Find(Handle)->Surface.Overrides[0].Value == FMaterialValue::Float(.62f));
	HYP_CHECK(Scene.Find(Handle)->SectionSurfaces.at(0).Overrides[0].Value == FMaterialValue::Float(.4f));
	HYP_CHECK(Scene.Find(Handle)->World.Values[12] == 2);
	HYP_CHECK(!Scene.Find(Handle)->Data->QueryGeometry);
	HYP_CHECK(!Scene.Find(Cleared)->Surface.Reference && Scene.Find(Cleared)->Surface.Overrides.empty());
	HYP_CHECK(Scene.Find(Cleared)->SectionSurfaces.empty());
	const auto Snapshot = Scene.Snapshot("SavedMaterialEdits.hasset");
	HYP_CHECK(Snapshot.Nodes[0].Model->Surface.Overrides[0].Value.Words == FMaterialValue::Float(.62f).Words);
	HYP_CHECK(Snapshot.Nodes[0].Model->SectionSurfaces.size() == 1 && Snapshot.Nodes[1].Model->SectionSurfaces.empty());
	Scene.Close();
	Manifest.Instances.resize(1);
	Manifest.Instances[0].Surface.Reference->Path = "MissingMaterial.hasset";
	F.IO.WriteAsync("FailedMaterial.hasset", EncodeAsset(RecordType<FLegacySceneManifest>(), &Manifest).Bytes)
	    .Get(F.Tasks);
	Scene.Load("FailedMaterial.hasset");
	Await(Scene,
	      [&]
	      {
		      return Scene.GetStatus().FailedModels == 1;
	      });
	RejectPendingSnapshot(Scene);
	const auto FailedSource = Scene.GetModels()[0].Handle;
	HYP_CHECK(CanInitializeSceneBrowsingView(Scene));
	const auto FailedCopy = Scene.DuplicateNode(FailedSource);
	Scene.Tick();
	HYP_CHECK(Scene.GetStatus().FailedModels == 2);
	RejectPendingSnapshot(Scene);
	HYP_CHECK(Scene.Remove(FailedSource));
	Scene.Tick();
	HYP_CHECK(Scene.GetStatus().FailedModels == 1 && Scene.Find(FailedCopy));
	RejectPendingSnapshot(Scene);
	HYP_CHECK(Scene.Remove(FailedCopy));
	HYP_CHECK(SceneModelCount(Scene.Snapshot("RemovedFailedMaterial.hasset")) == 0);
	std::cout << "Pending/failed material saves reject; pending whole/section edits and clears survive publication\n";
}

std::array<FSceneHandle, 3> CopyPendingModels(FSceneInstance& InScene)
{
	const auto Source = InScene.GetModels()[0].Handle;
	const auto Plain = InScene.DuplicateNode(Source);
	auto Model = *InScene.FindNode(Source)->Model();
	Model.Surface.Overrides = {{"Pbr.RoughnessFactor", FMaterialValue::Float(.62f)}};
	Model.SectionSurfaces[0].Overrides = {{"Pbr.MetallicFactor", FMaterialValue::Float(.4f)}};
	InScene.SetModelComponent(Source, Model);
	const auto Edited = InScene.DuplicateNode(Source);
	const auto Cleared = InScene.DuplicateNode(Plain);
	InScene.SetModelComponent(Cleared, Model);
	Model.Surface = {};
	Model.SectionSurfaces.clear();
	InScene.SetModelComponent(Cleared, Model);
	const auto Removed = InScene.DuplicateNode(Plain);
	InScene.RemoveSubtree(Removed);
	FSceneNode Group;
	Group.Id = "replacement";
	const auto Replacement = InScene.AddNode(Group);
	HYP_CHECK(Replacement.Slot == Removed.Slot && Replacement.Generation != Removed.Generation);
	InScene.RemoveSubtree(Source);
	HYP_CHECK(!InScene.DuplicateNode(Source).Scene);
	RejectPendingSnapshot(InScene);
	return {Plain, Edited, Cleared};
}

void CheckCopiedMaterialValues(FSceneInstance& InScene, const std::array<FSceneHandle, 3>& InHandles)
{
	const auto* Plain = InScene.Find(InHandles[0]);
	const auto* Edited = InScene.Find(InHandles[1]);
	const auto* Cleared = InScene.Find(InHandles[2]);
	HYP_CHECK(Plain && Edited && Cleared);
	HYP_CHECK(Plain->Surface.Reference && Plain->Surface.Overrides[0].Value == FMaterialValue::Float(.15f));
	HYP_CHECK(Plain->SectionSurfaces.at(0).Reference &&
	          Plain->SectionSurfaces.at(0).Overrides[0].Value == FMaterialValue::Float(.15f));
	HYP_CHECK(Edited->Surface.Overrides[0].Value == FMaterialValue::Float(.62f));
	HYP_CHECK(Edited->SectionSurfaces.at(0).Overrides[0].Value == FMaterialValue::Float(.4f));
	HYP_CHECK(!Cleared->Surface.Reference && Cleared->Surface.Overrides.empty() && Cleared->SectionSurfaces.empty());
}

void CheckPendingMaterialCopies(FSceneFixture& InFixture)
{
	auto& F = InFixture;
	for (unsigned Case = 0; Case < 2; ++Case)
	{
		if (Case == 0)
		{
			F.Assets.ClearCache();
		}
		F.Files->bRelease = false;
		F.Files->bEntered = false;
		const auto MaterialPath = "DuplicateMaterial" + std::to_string(Case) + ".hasset";
		const auto ManifestPath = "DuplicatePending" + std::to_string(Case) + ".hasset";
		F.Files->GateName = Case == 0 ? "SceneRuntime.model.hasset" : MaterialPath;
		F.IO.WriteAsync(MaterialPath, *F.IO.ReadAsync("@material-0.hasset").Get(F.Tasks)).Get(F.Tasks);
		FLegacySceneManifest Manifest;
		Manifest.Assets = {{"good", {"", "SceneRuntime.model.hasset", RecordType<FModelAsset>().Id, ""}}};
		Manifest.Instances = {{"original", "good"}};
		auto& Entry = Manifest.Instances[0];
		Entry.Surface.Reference = FAssetRef{"", MaterialPath, RecordType<FMaterialAsset>().Id, ""};
		Entry.Surface.Overrides = {{"Pbr.RoughnessFactor", PersistMaterialValue(FMaterialValue::Float(.15f))}};
		Entry.SectionSurfaces = {{0, Entry.Surface}};
		F.IO.WriteAsync(ManifestPath, EncodeAsset(RecordType<FLegacySceneManifest>(), &Manifest).Bytes).Get(F.Tasks);
		FSceneInstance Scene(*F.Session, F.Tasks, F.Assets);

		struct FRelease
		{
			std::atomic<bool>& bFlag;

			~FRelease()
			{
				bFlag = true;
			}
		} Release{F.Files->bRelease};

		std::cout << "Pending copy case=" << Case << " stage=wait-gate\n";
		Scene.Load(ManifestPath);
		Await(Scene,
		      [&]
		      {
			      return Scene.GetModels().size() == 1 && F.Files->bEntered;
		      });
		if (Case == 1)
		{
			std::cout << "Pending copy stage=wait-model-ready\n";
			Await(Scene,
			      [&]
			      {
				      return bool(Scene.GetAssets()[0].Data);
			      });
		}
		std::cout << "Pending copy stage=duplicate\n";
		// CPU asset readiness alone is insufficient while selected materials block geometry publication.
		HYP_CHECK(!CanInitializeSceneBrowsingView(Scene));
		auto Settings = Scene.GetSettings();
		Settings.InitialView = FSceneCameraView{};
		Scene.SetSettings(Settings);
		HYP_CHECK(CanInitializeSceneBrowsingView(Scene));
		Settings.InitialView.reset();
		Scene.SetSettings(Settings);
		const auto Copies = CopyPendingModels(Scene);
		F.Files->bRelease = true;
		Await(Scene,
		      [&]
		      {
			      return Scene.GetStatus().ReadyModels == 3;
		      });
		CheckCopiedMaterialValues(Scene, Copies);
		HYP_CHECK(CanInitializeSceneBrowsingView(Scene));
		const auto Snapshot = Scene.Snapshot("DuplicateSaved.hasset");
		F.Assets.SaveAsync("DuplicateSaved.hasset", std::make_shared<const FSceneManifest>(Snapshot)).Get(F.Tasks);
		std::array<std::string, 3> Ids;
		for (std::size_t Index = 0; Index < Ids.size(); ++Index)
		{
			Ids[Index] = Scene.FindNode(Copies[Index])->Id;
		}
		std::cout << "Pending copy stage=reload\n";
		Scene.Load("DuplicateSaved.hasset");
		Await(Scene,
		      [&]
		      {
			      return Scene.GetStatus().bReady;
		      });
		CheckCopiedMaterialValues(Scene,
		                          {Scene.FindHandle(Ids[0]), Scene.FindHandle(Ids[1]), Scene.FindHandle(Ids[2])});
		Scene.Close();
	}
	F.Files->GateName = "SceneRuntime.model.hasset";
}

void CheckNodeOnlySnapshot(FSceneFixture& InFixture)
{
	auto& F = InFixture;
	FSceneInstance Scene(*F.Session, F.Tasks, F.Assets);
	Scene.Tick();
	const auto EmptyToken = Scene.GetToken();
	HYP_CHECK(EmptyToken.PublicationSerial == 1);
	F.Tasks.Wait(Scene.GetReceipt());
	Scene.Tick();
	HYP_CHECK(Scene.GetToken() == EmptyToken && Scene.GetStatus().bReady && !Scene.GetStatus().bHasActiveCamera);
	FSceneNode Rig;
	Rig.Id = "rig";
	Rig.Local() = Translation({1, 2, 3});
	const auto Parent = Scene.AddNode(Rig);
	auto CameraNode = MakeSceneCameraNode("camera", {0, 0, 8}, {});
	CameraNode.Parent() = "rig";
	const auto Camera = Scene.AddNode(CameraNode);
	const auto Other = Scene.AddNode(MakeSceneCameraNode("other", {2, 0, 8}, {}));
	const auto Sun = Scene.AddNode(MakeSceneDirectionalLightNode("sun"));
	const auto Ambient = Scene.AddNode(MakeSceneEnvironmentLightNode("ambient"));
	Scene.SetSettings({Camera, Sun, Ambient});
	Scene.Tick();
	HYP_CHECK(Scene.GetStatus().Nodes == 5 && Scene.GetStatus().Models == 0 && Scene.GetStatus().Cameras == 2);
	const auto Frozen = Scene.Snapshot("NodeOnly.hasset");
	HYP_CHECK(Frozen.Assets.empty() && Frozen.Nodes.size() == 5 && Frozen.DefaultCamera == "camera");
	Scene.SetCamera(Camera, {.9f, .02f, 70.f, 4.f});
	Scene.Reparent(Camera, {}, ESceneReparentMode::KeepWorld);
	HYP_CHECK(Scene.RemoveNodeKeepChildren(Parent));
	HYP_CHECK(Frozen.Nodes[1].Parent == "rig" && Frozen.Nodes[1].Camera->FocusDistance == 8);
	Scene.SetSettings({Other, Sun, Ambient});
	Scene.Tick();
	const auto Saved = Scene.Snapshot("NodeOnly.hasset");
	F.IO.WriteAsync("NodeOnly.hasset", EncodeAsset(RecordType<FSceneManifest>(), &Saved).Bytes).Get(F.Tasks);
	const auto Previous = Scene.GetToken();
	Scene.Load("NodeOnly.hasset");
	Await(Scene,
	      [&]
	      {
		      return Scene.GetStatus().bLoaded && Scene.GetStatus().Nodes == 4;
	      });
	HYP_CHECK(Scene.GetToken().AttachmentEpoch != Previous.AttachmentEpoch && !Scene.FindNode(Camera));
	HYP_CHECK(Scene.FindNode(*Scene.GetSettings().DefaultCamera)->Id == "other");
	HYP_CHECK(Serialize(Scene.Snapshot("NodeOnly.hasset")) == Serialize(Saved));
}

void CheckSceneFrameTokens(FSceneFixture& InFixture)
{
	auto& F = InFixture;
	FSceneInstance Scene(*F.Session, F.Tasks, F.Assets);
	const auto Camera = Scene.AddNode(MakeSceneCameraNode("camera", {0, 0, 8}, {}));
	const auto Sun = Scene.AddNode(MakeSceneDirectionalLightNode("sun"));
	Scene.SetSettings({Camera, Sun, {}});
	Scene.Tick();
	const auto InitialToken = Scene.GetToken();
	const auto Seed = F.Session->FreezeSceneFrame(InitialToken);
	FSceneViewRequest Request;
	Request.Width = 400;
	Request.Height = 200;
	FResolvedSceneFrame Initial;
	std::optional<std::uint64_t> Geometry;
	F.Tasks.Wait(F.Tasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              Initial = F.Session->ResolveSceneFrame(*Seed, Request);
		                              Geometry = F.Session->GetScene().GetCollectionRevision();
	                              }));
	HYP_CHECK(Initial.HasCamera() && Initial.View.Eye.Z == 8 && Initial.Frame->CastsSceneShadows());
	Scene.SetWorldTransform(Camera, Translation({1, 0, 8}));
	Scene.Tick();
	const auto Next = F.Session->FreezeSceneFrame(Scene.GetToken());
	FResolvedSceneFrame Moved;
	bool bRejected{};
	F.Tasks.Wait(F.Tasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              try
		                              {
			                              F.Session->ResolveSceneFrame(*Seed, Request);
		                              }
		                              catch (const std::invalid_argument&)
		                              {
			                              bRejected = true;
		                              }
		                              Moved = F.Session->ResolveSceneFrame(*Next, Request);
		                              HYP_CHECK(F.Session->GetScene().GetCollectionRevision() == Geometry);
		                              Request.Identity = 2;
		                              const auto Other = F.Session->ResolveSceneFrame(*Next, Request);
		                              HYP_CHECK(Other.Frame == Moved.Frame);
	                              }));
	HYP_CHECK(bRejected && Moved.View.Eye.X == 1 && Initial.View.Eye.X == 0);
	const auto Index = static_cast<std::size_t>(EMaterialScope::Scene);
	HYP_CHECK(Initial.Frame->Inputs.Scopes[Index].Key == Moved.Frame->Inputs.Scopes[Index].Key);
	HYP_CHECK(Initial.Frame->Inputs.Scopes[Index].Lifetime == Moved.Frame->Inputs.Scopes[Index].Lifetime);
	Scene.SetEnabled(Sun, false);
	Scene.SetEnabled(Camera, false);
	Scene.Tick();
	const auto Disabled = F.Session->FreezeSceneFrame(Scene.GetToken());
	F.Tasks.Wait(F.Tasks.Dispatch(
	    {EDomain::Render},
	    [&]
	    {
		    const auto Frame = F.Session->ResolveSceneFrame(*Disabled, Request);
		    HYP_CHECK(!Frame.HasCamera() && !Frame.Frame->CastsSceneShadows());
		    HYP_CHECK(*Frame.Frame->Inputs.Find(EMaterialScope::Scene, "Engine.Scene.MainDirectionalLightColor") ==
		              FMaterialValue::Float(FVec3{}));
		    HYP_CHECK(*Frame.Frame->Inputs.Find(EMaterialScope::Scene, "Engine.Scene.AmbientColor") ==
		              FMaterialValue::Float(FVec3{}));
		    HYP_CHECK(F.Session->GetScene().GetCollectionRevision() == Geometry);
	    }));
}

void ExpectInputRejection(const std::function<void()>& InAction)
{
	bool bRejected{};
	try
	{
		InAction();
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

void CheckStrictCameraPreview(FSceneFixture& InFixture)
{
	auto& F = InFixture;
	FSceneInstance Scene(*F.Session, F.Tasks, F.Assets);
	const auto Default = Scene.AddNode(MakeSceneCameraNode("default"));
	FSceneNode Parent;
	Parent.Id = "parent";
	const auto ParentHandle = Scene.AddNode(Parent);
	auto Node = MakeSceneCameraNode("preview", {4, 2, 8}, {});
	Node.Parent() = Parent.Id;
	const auto Preview = Scene.AddNode(Node);
	Scene.SetSettings({Default, {}, {}});
	FSceneViewRequest Request;
	Request.Width = 400;
	Request.Height = 200;
	Request.Camera = Preview;
	Request.bAllowCameraFallback = false;
	const auto CheckView = [&](bool bInAvailable)
	{
		Scene.Tick();
		const auto Seed = F.Session->FreezeSceneFrame(Scene.GetToken());
		F.Tasks.Wait(
		    F.Tasks.Dispatch({EDomain::Render},
		                     [&]
		                     {
			                     const auto Frame = F.Session->ResolveSceneFrame(*Seed, Request);
			                     HYP_CHECK(Frame.HasCamera() == bInAvailable);
			                     if (bInAvailable)
			                     {
				                     HYP_CHECK(Frame.Camera == Preview && Frame.View.Eye.X == 4);
				                     HYP_CHECK(Frame.View.Camera->VerticalRadians == Node.Camera()->VerticalRadians);
			                     }
			                     else
			                     {
				                     auto Fallback = Request;
				                     Fallback.bAllowCameraFallback = true;
				                     HYP_CHECK(F.Session->ResolveSceneFrame(*Seed, Fallback).Camera == Default);
			                     }
		                     }));
	};
	CheckView(true);
	Node.Camera()->VerticalRadians = .8f;
	Scene.EditNode(Preview, Node, Scene.GetRevision());
	CheckView(true);
	Scene.SetEnabled(Preview, false);
	CheckView(false);
	Scene.SetEnabled(Preview, true);
	Scene.SetEnabled(ParentHandle, false);
	CheckView(false);
	Scene.SetEnabled(ParentHandle, true);
	auto NoCamera = Node;
	NoCamera.Camera().reset();
	Scene.EditNode(Preview, NoCamera, Scene.GetRevision());
	CheckView(false);
	Scene.EditNode(Preview, Node, Scene.GetRevision());
	CheckView(true);
	Scene.RemoveSubtree(Preview);
	const auto Replacement = Scene.AddNode(MakeSceneCameraNode("replacement"));
	HYP_CHECK(Replacement != Preview);
	CheckView(false);
	const auto Revision = Scene.GetRevision();
	const auto Browsing = MakeSceneBrowsingView(Scene, 2);
	HYP_CHECK(Scene.GetRevision() == Revision && !Scene.GetSettings().InitialView);
	auto Settings = Scene.GetSettings();
	Settings.InitialView = FSceneCameraView{FSceneCamera{}, Translation({90, 20, 10})};
	Scene.SetSettings(Settings);
	HYP_CHECK(MakeSceneBrowsingView(Scene, 1) == *Settings.InitialView);
	HYP_CHECK(Scene.Snapshot("view.hasset").InitialView == Settings.InitialView);
	HYP_CHECK(Browsing.World.Values != Settings.InitialView->World.Values);
}

void CheckSceneViewSelection(FSceneFixture& InFixture)
{
	auto& F = InFixture;
	FSceneInstance Scene(*F.Session, F.Tasks, F.Assets);
	const auto A = Scene.AddNode(MakeSceneCameraNode("a", {0, 0, 8}, {}));
	const auto B = Scene.AddNode(MakeSceneCameraNode("b", {3, 0, 8}, {3, 0, 0}));
	Scene.SetSettings({A, {}, {}});
	Scene.Tick();
	FSceneViewRequest Request;
	Request.Width = 400;
	Request.Height = 200;
	const FSceneCameraView EditorCamera{*Scene.FindNode(A)->Camera(), Translation({9, 0, 8})};
	const auto AuthoredRevision = Scene.GetRevision();
	const auto Seed = F.Session->FreezeSceneFrame(Scene.GetToken());
	F.Tasks.Wait(F.Tasks.Dispatch(
	    {EDomain::Render},
	    [&]
	    {
		    const auto First = F.Session->ResolveSceneFrame(*Seed, Request);
		    Request.Camera = B;
		    Request.Identity = 2;
		    Request.Viewport = FViewport{10, 10, 160, 160};
		    Request.DepthConvention = EDepthConvention::Reversed;
		    const auto Second = F.Session->ResolveSceneFrame(*Seed, Request);
		    HYP_CHECK(First.Frame == Second.Frame && First.View.Eye.X == 0 && Second.View.Eye.X == 3);
		    HYP_CHECK(First.View.ViewProjection.Values != Second.View.ViewProjection.Values);
		    auto EditorRequest = Request;
		    EditorRequest.CameraOverride = EditorCamera;
		    const auto EditorFrame = F.Session->ResolveSceneFrame(*Seed, EditorRequest);
		    HYP_CHECK(EditorFrame.HasCamera() && !EditorFrame.Camera && EditorFrame.View.Eye.X == 9);
		    auto Invalid = Request;
		    Invalid.Camera->Scene += 100;
		    ExpectInputRejection(
		        [&]
		        {
			        F.Session->ResolveSceneFrame(*Seed, Invalid);
		        });
		    Invalid = Request;
		    Invalid.Viewport->MinDepth = -.1f;
		    ExpectInputRejection(
		        [&]
		        {
			        F.Session->ResolveSceneFrame(*Seed, Invalid);
		        });
		    Invalid = Request;
		    Invalid.CullingMode = static_cast<ESceneCullingMode>(99);
		    ExpectInputRejection(
		        [&]
		        {
			        F.Session->ResolveSceneFrame(*Seed, Invalid);
		        });
		    Invalid = Request;
		    Invalid.DepthConvention = static_cast<EDepthConvention>(99);
		    ExpectInputRejection(
		        [&]
		        {
			        F.Session->ResolveSceneFrame(*Seed, Invalid);
		        });
		    Invalid = Request;
		    Invalid.Viewport->Width = 0;
		    HYP_CHECK(F.Session->ResolveSceneFrame(*Seed, Invalid).CameraStatus == ESceneCameraStatus::EmptyViewport);
	    }));
	HYP_CHECK(Scene.GetRevision() == AuthoredRevision);
	Scene.RemoveSubtree(B);
	Scene.Tick();
	const auto Fallback = F.Session->FreezeSceneFrame(Scene.GetToken());
	F.Tasks.Wait(F.Tasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              const auto Frame = F.Session->ResolveSceneFrame(*Fallback, Request);
		                              HYP_CHECK(Frame.CameraStatus == ESceneCameraStatus::DefaultFallback &&
		                                        Frame.Camera == A);
	                              }));
	Scene.RemoveSubtree(A);
	Scene.Tick();
	const auto Empty = F.Session->FreezeSceneFrame(Scene.GetToken());
	F.Tasks.Wait(F.Tasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              HYP_CHECK(!F.Session->ResolveSceneFrame(*Empty, Request).HasCamera());
	                              }));
	std::cout << "Two cameras share one immutable frame; viewport/depth, fallback, foreign handles and invalid "
	             "requests checked\n";
}

void CheckSceneMaterialGuards(FSceneFixture& InFixture)
{
	auto& F = InFixture;
	constexpr std::array<const char*, 5> Names{"Engine.Scene.MainDirectionalLightDirection",
	                                           "Engine.Scene.MainDirectionalLightColor", "Engine.Scene.AmbientColor",
	                                           "Engine.View.ViewProjection", "Engine.View.CameraPosition"};
	for (const auto Name : Names)
	{
		FRenderSession Session(F.Tasks, *F.Device, F.Compiler);
		const auto Scope = GetStandardMaterialSemantics()->Find(Name).Scope;
		Session.GetProviders().Register({Name, MaterialScopeBit(Scope), [](const FMaterialProviderInputs&)
		                                 {
			                                 return FMaterialValue::Float(FVec3{9, 9, 9});
		                                 }});
		FSceneInstance Scene(Session, F.Tasks, F.Assets);
		Scene.Tick();
		ExpectInputRejection(
		    [&]
		    {
			    Session.FreezeSceneFrame(Scene.GetToken());
		    });
		Scene.Close();
	}
	FSceneInstance Scene(*F.Session, F.Tasks, F.Assets);
	Scene.Tick();
	const auto Token = Scene.GetToken();
	F.Session->SetSceneParameters({{"Custom.Scene", FMaterialValue::Float(7)}});
	F.Session->SetGlobalParameters({{"Custom.Global", FMaterialValue::Float(8)}});
	for (const auto Name : Names)
	{
		FMaterialParameterValues Values{{"Custom.Scene", FMaterialValue::Float(999)},
		                                {Name, FMaterialValue::Float(999)}};
		ExpectInputRejection(
		    [&]
		    {
			    F.Session->SetSceneParameters(Values);
		    });
		ExpectInputRejection(
		    [&]
		    {
			    F.Session->SetGlobalParameters(Values);
		    });
		ExpectInputRejection(
		    [&]
		    {
			    F.Session->FreezeSceneFrame(Token, 0, Values);
		    });
		const auto Seed = F.Session->FreezeSceneFrame(Token);
		F.Tasks.Wait(F.Tasks.Dispatch(
		    {EDomain::Render},
		    [&]
		    {
			    FSceneViewRequest Request;
			    Request.Width = 320;
			    Request.Height = 240;
			    const auto Frame = F.Session->ResolveSceneFrame(*Seed, Request);
			    HYP_CHECK(*Frame.Frame->Inputs.Find(EMaterialScope::Scene, "Custom.Scene") == FMaterialValue::Float(7));
			    HYP_CHECK(*Frame.Frame->Inputs.Find(EMaterialScope::Global, "Custom.Global") ==
			              FMaterialValue::Float(8));
			    Request.Parameters = Values;
			    ExpectInputRejection(
			        [&]
			        {
				        F.Session->ResolveSceneFrame(*Seed, Request);
			        });
			    Request.Parameters.clear();
			    Request.PassParameters = Values;
			    ExpectInputRejection(
			        [&]
			        {
				        F.Session->ResolveSceneFrame(*Seed, Request);
			        });
		    }));
	}
	Scene.Close();
	F.Session->SetSceneParameters({});
	F.Session->SetGlobalParameters({});
	// Only the three historical session light defaults may be stripped on first binding.
	for (const auto Name : {Names[3], Names[4]})
	{
		F.Session->SetSceneParameters({{Name, FMaterialValue::Float(999)}});
		FSceneInstance Bound(*F.Session, F.Tasks, F.Assets);
		Bound.Tick();
		ExpectInputRejection(
		    [&]
		    {
			    F.Session->FreezeSceneFrame(Bound.GetToken());
		    });
		Bound.Close();
	}
	F.Session->SetSceneParameters({});
	std::cout << "All five protected providers and Global/Frame/View/pass inputs reject; batches remain atomic\n";
}

void CheckModelStatusCache()
{
	FSceneFixture F;
	FSceneInstance Scene(*F.Session, F.Tasks, F.Assets);
	const auto Data = PrepareSourceModel(MakeModel());
	std::array<FSceneHandle, 8> Models;
	for (auto& Handle : Models)
	{
		Handle = Scene.Add({"cached", Data});
	}
	const auto Camera = Scene.AddNode(MakeSceneCameraNode("camera", {0, 0, 3}, {}));
	const auto Light = Scene.AddNode(MakeSceneDirectionalLightNode("sun"));
	Scene.SetSettings({Camera, Light, {}});
	Await(Scene,
	      [&]
	      {
		      return Scene.GetStatus().bReady;
	      });
	F.Tasks.Wait(Scene.GetReceipt());
	Scene.Tick();
	const auto Refreshes = Scene.GetStatus().ModelStatusRefreshes;
	for (unsigned Index = 0; Index < 64; ++Index)
	{
		Scene.SetWorldTransform(Camera, Translation({float(Index) * .01f, 0, 3}));
		Scene.SetCamera(Camera, {1, .01f, 100, 3 + float(Index) * .01f});
		Scene.SetDirectionalLight(Light, {{1, 1, 1}, 1 + float(Index), true});
		Scene.SetEnabled(Camera, Index % 2 == 0);
		Scene.Tick();
		F.Tasks.Wait(Scene.GetReceipt());
		Scene.Tick();
		const auto& Status = Scene.GetStatus();
		HYP_CHECK(Status.bReady && Status.ReadyModels == Models.size() && Status.FailedModels == 0);
		HYP_CHECK(Status.bHasActiveCamera == (Index % 2 == 0));
		HYP_CHECK(Status.ModelStatusRefreshes == Refreshes && Status.Nodes == Models.size() + 2);
	}
	Scene.Reparent(Models[0], Camera, ESceneReparentMode::KeepWorld);
	Scene.Tick();
	F.Tasks.Wait(Scene.GetReceipt());
	Scene.Tick();
	const auto Parented = Scene.GetStatus().ModelStatusRefreshes;
	Scene.SetWorldTransform(Camera, Translation({1, 0, 3}));
	Scene.Tick();
	F.Tasks.Wait(Scene.GetReceipt());
	Scene.Tick();
	HYP_CHECK(Scene.GetStatus().ModelStatusRefreshes > Parented);
	Scene.Remove(Models[0]);
	Scene.Tick();
	HYP_CHECK(Scene.GetStatus().Models == Models.size() - 1);
	std::cout << "64 camera/light updates reused model status; inherited model transforms and deletion invalidate\n";
}

void CheckLargeCoordinateView(FSceneFixture& InFixture)
{
	FSceneInstance Scene(*InFixture.Session, InFixture.Tasks, InFixture.Assets);
	const auto Camera = Scene.AddNode(MakeSceneCameraNode("large-coordinate", {0, 0, 3}, {}));
	Scene.SetWorldTransform(Camera, Translation({33554432.f, 33554432.f, 33554432.f}));
	Scene.SetSettings({Camera, {}, {}});
	Scene.Tick();
	const auto Seed = InFixture.Session->FreezeSceneFrame(Scene.GetToken());
	InFixture.Tasks.Wait(
	    InFixture.Tasks.Dispatch({EDomain::Render},
	                             [&]
	                             {
		                             FSceneViewRequest Request;
		                             Request.Width = 320;
		                             Request.Height = 240;
		                             const auto Frame = InFixture.Session->ResolveSceneFrame(*Seed, Request);
		                             for (const auto Value : Frame.View.ViewProjection.Values)
		                             {
			                             HYP_CHECK(std::isfinite(Value));
		                             }
		                             HYP_CHECK(Frame.HasCamera() && Frame.View.Camera->Forward.Z == -1);
	                             }));
}

void CheckNavigationPrecision(FSceneFixture& InFixture)
{
	FSceneInstance Scene(*InFixture.Session, InFixture.Tasks, InFixture.Assets);
	const FVec3 Eye{0, 6, 16};
	const FVec3 Target{0, 0, -8};
	const auto Camera = Scene.AddNode(MakeSceneCameraNode("precision", Eye, Target));
	Scene.SetSettings({Camera, {}, {}});
	const auto Offset = Subtract(Eye, Target);
	const float Distance = Length(Offset);
	const float Pitch = std::asin(Offset.Y / Distance);
	float Yaw{};
	float LastMouse{};
	float MaximumEyeError{};
	float MaximumForwardError{};
	FSceneCameraPose Pose;
	FVec3 ExpectedEye;
	for (unsigned Frame = 240; Frame < 840; ++Frame)
	{
		const unsigned Phase = Frame % 40;
		const float Mouse = float(Phase < 20 ? Phase : 40 - Phase);
		const float Delta = -(Mouse - LastMouse) * .006f;
		LastMouse = Mouse;
		Yaw += Delta;
		OrbitSceneCamera(Scene, Delta, 0);
		const FVec3 Direction{std::sin(Yaw) * std::cos(Pitch), std::sin(Pitch), std::cos(Yaw) * std::cos(Pitch)};
		ExpectedEye = Add(Target, ScaleVector(Direction, Distance));
		Scene.GetCameraPose(Camera, Pose);
		MaximumEyeError = std::max(MaximumEyeError, Length(Subtract(Pose.Eye, ExpectedEye)));
		MaximumForwardError = std::max(MaximumForwardError, Length(Add(Pose.Forward, Direction)));
	}
	std::cout << "600-frame legacy orbit oracle max eye error=" << MaximumEyeError
	          << " max unit forward error=" << MaximumForwardError << " final eye=" << Pose.Eye.X << "," << Pose.Eye.Y
	          << "," << Pose.Eye.Z << " oracle=" << ExpectedEye.X << "," << ExpectedEye.Y << "," << ExpectedEye.Z
	          << "\n";
	HYP_CHECK(MaximumEyeError < .001f && MaximumForwardError < .0001f);
}

void CheckSceneNavigation(FSceneFixture& InFixture)
{
	FSceneInstance Scene(*InFixture.Session, InFixture.Tasks, InFixture.Assets);
	FSceneNode Rig;
	Rig.Id = "navigation-rig";
	Rig.Local() = Translation({2, 0, 0});
	const auto Parent = Scene.AddNode(Rig);
	auto Node = MakeSceneCameraNode("navigation", {0, 0, 8}, {});
	Node.Parent() = Rig.Id;
	const auto Camera = Scene.AddNode(Node);
	Scene.SetSettings({Camera, {}, {}});
	FSceneCameraPose Pose;
	Scene.SetWorldTransform(Camera, Translation({4, 1, 8}));
	DollySceneCamera(Scene, .5f);
	HYP_CHECK(Scene.GetCameraPose(Camera, Pose));
	HYP_CHECK(std::abs(Pose.Eye.X - 4) < .0001f && std::abs(Pose.Eye.Z - 4) < .0001f);
	HYP_CHECK(Scene.FindNode(Camera)->Camera()->FocusDistance == 4);
	OrbitSceneCamera(Scene, 1.57079632679f, 0);
	HYP_CHECK(Scene.GetCameraPose(Camera, Pose));
	HYP_CHECK(std::abs(Pose.Eye.X - 8) < .0001f && std::abs(Pose.Eye.Z) < .0001f);
	const auto Pivot = GetSceneNavigationPivot(Scene);
	HYP_CHECK(std::abs(Pivot.X - 4) < .0001f && std::abs(Pivot.Y - 1) < .0001f);
	const auto Other = Scene.AddNode(MakeSceneCameraNode("other-navigation", {0, 0, 10}, {}));
	Scene.SetSettings({Other, {}, {}});
	PanSceneCamera(Scene, {1, 0, 0});
	Scene.GetCameraPose(Other, Pose);
	HYP_CHECK(std::abs(Pose.Eye.X - .8f) < .0001f);
	Scene.SetSettings({Camera, {}, {}});
	Scene.SetWorldTransform(Camera, Translation({0, 0, 8}));
	Scene.SetLocalTransform(Parent, Scale({0, 1, 1}));
	const auto Before = Serialize(Scene.Snapshot("Navigation.hasset"));
	bool bRejected{};
	try
	{
		DollySceneCamera(Scene, .5f);
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected && Before == Serialize(Scene.Snapshot("Navigation.hasset")));
	Scene.SetEnabled(Camera, false);
	PanSceneCamera(Scene, {1, 0, 0});
	HYP_CHECK(!GetSceneNavigationCamera(Scene));
}

void CheckPendingHierarchy()
{
	FSceneFixture F;
	FSceneInstance Scene(*F.Session, F.Tasks, F.Assets);

	struct FRelease
	{
		std::atomic<bool>& bFlag;

		~FRelease()
		{
			bFlag = true;
		}
	} Release{F.Files->bRelease};

	Scene.Load("SceneRuntime.hasset");
	Await(Scene,
	      [&]
	      {
		      return Scene.GetModels().size() == 3 && F.Files->bEntered;
	      });
	const auto Removed = Scene.FindHandle("one");
	const auto Kept = Scene.FindHandle("two");
	FSceneNode Rig;
	Rig.Id = "pending-rig";
	const auto Parent = Scene.AddNode(Rig);
	Scene.Reparent(Kept, Parent, ESceneReparentMode::KeepLocal);
	Scene.SetLocalTransform(Kept, Translation({4, 0, 0}));
	auto CameraNode = MakeSceneCameraNode("pending-camera", {0, 0, 8}, {});
	CameraNode.Parent() = "pending-rig";
	const auto Camera = Scene.AddNode(CameraNode);
	const auto Light = Scene.AddNode(MakeSceneDirectionalLightNode("pending-light"));
	Scene.SetCamera(Camera, {.9f, .02f, 70, 6});
	Scene.SetDirectionalLight(Light, {{.2f, .4f, .6f}, 2, false});
	Scene.SetLocalTransform(Parent, Translation({3, 0, 0}));
	Rig.Id = "deleted-rig";
	const auto DeletedParent = Scene.AddNode(Rig);
	Scene.Reparent(Removed, DeletedParent, ESceneReparentMode::KeepLocal);
	Scene.RemoveSubtree(DeletedParent);
	const auto Replacement = Scene.Add({"replacement after subtree"}, "good");
	Scene.SetSettings({Camera, Light, {}});
	Scene.Tick();
	HYP_CHECK(Scene.GetStatus().bHasActiveCamera && !Scene.FindNode(Kept)->Model()->Data);
	F.Files->bRelease = true;
	Await(Scene,
	      [&]
	      {
		      return Scene.GetStatus().ReadyModels == 2 && Scene.GetStatus().FailedModels == 1;
	      });
	FSceneNodeView View;
	HYP_CHECK(Scene.GetNodeView(Kept, View) && View.World.Values[12] == 7);
	HYP_CHECK(Scene.FindNode(Kept)->Local().Values[12] == 4 && Scene.FindNode(Replacement)->Model()->Data);
	HYP_CHECK(!Scene.FindNode(Removed) && !Scene.FindNode(DeletedParent));
	FSceneCameraPose Pose;
	HYP_CHECK(Scene.GetCameraPose(Camera, Pose) && Pose.Eye.X == 3 && Pose.Eye.Z == 8);
	HYP_CHECK(Scene.FindNode(Camera)->Camera()->FocusDistance == 6);
	HYP_CHECK(Scene.FindNode(Light)->DirectionalLight()->Color.Z == .6f);
	HYP_CHECK(Scene.GetSettings().MainDirectionalLight == Light);
	std::cout << "Gated model completion preserves parent, camera/light edits and subtree tombstones\n";
}

void CheckClosedDependencies()
{
	auto Fixture = std::make_unique<FSceneFixture>();
	auto Scene = std::make_unique<FSceneInstance>(*Fixture->Session, Fixture->Tasks, Fixture->Assets);
	Scene->Close();
	Fixture.reset();
	Scene->Close();
	Scene.reset();
}
} // namespace

int main()
{
	try
	{
		FSceneFixture Fixture;
		CheckLoadingEdits(Fixture);
		CheckClosePending(Fixture);
		CheckPendingMaterialEdits(Fixture);
		CheckPendingMaterialCopies(Fixture);
		CheckNodeOnlySnapshot(Fixture);
		CheckSceneFrameTokens(Fixture);
		CheckSceneViewSelection(Fixture);
		CheckStrictCameraPreview(Fixture);
		CheckSceneMaterialGuards(Fixture);
		CheckSceneNavigation(Fixture);
		CheckNavigationPrecision(Fixture);
		CheckLargeCoordinateView(Fixture);
		CheckModelStatusCache();
		CheckClosedDependencies();
		CheckPendingHierarchy();
		std::cout << "Independent scene loading, shared models, generation-safe edits and close passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
