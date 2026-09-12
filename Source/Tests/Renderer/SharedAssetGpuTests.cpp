#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/NativeModel.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/Renderer/SceneMaterialAssets.h"
#include "Support/GraphTestSupport.h"
#include "Support/NativeAssetSupport.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <iostream>
#include <thread>

namespace
{
using namespace Hyperion;

template<class T>
FAssetRef Store(FIOService& InIO, const std::filesystem::path& InPath, const T& InValue, FAssetHeader InHeader = {})
{
	const auto Encoded = EncodeAsset(RecordType<T>(), &InValue, std::move(InHeader));
	InIO.WriteAsync(InPath, Encoded.Bytes).Get(InIO.TaskSystem());
	return {Encoded.Header.Id, InPath.filename().generic_string(), Encoded.Header.TypeId, Encoded.Header.Revision};
}

FMaterialAsset MakeMaterial(const FAssetRef& InTexture)
{
	FMaterialDescription Description;
	Description.Name = "Independent authored shader";
	FMaterialPass Pass;
	Pass.Vertex = {"SharedAsset.hlsl", "AssetVertex"};
	Pass.Pixel = {"SharedAsset.hlsl", "AssetPixel", {{"ASSET_GAIN", "1"}}};
	Pass.bSrgbTarget = true;
	Description.Passes.push_back(Pass);
	for (const auto& Entry : FMaterialParameterValues{{"Tint", FMaterialValue::Float(FVec4{1, 0, 0, 1})},
	                                                  {"ImageSampler", FMaterialValue::FromSampler({})}})
	{
		FMaterialParameterDeclaration Parameter;
		Parameter.Name = Entry.Name;
		Parameter.Type = Entry.Value.Type;
		Parameter.Targets = {Entry.Name == "Tint" ? "Surface.Tint" : Entry.Name};
		Parameter.Default = Entry.Value;
		Description.Parameters.push_back(std::move(Parameter));
	}
	FMaterialParameterDeclaration Texture;
	Texture.Name = "ImageTexture";
	Texture.Type = FMaterialParameterType::Resource(EMaterialValueKind::Texture2D);
	Description.Parameters.push_back(Texture);
	auto Asset = PersistMaterialDescription(Description);
	FMaterialAssetValue Value;
	Value.Type = Texture.Type;
	Value.Texture = InTexture;
	Asset.Values.push_back({Texture.Name, Value});
	return Asset;
}

FModelAsset MakeModel(FAssetRef InMaterial, std::string InName)
{
	FModelAsset Model;
	Model.Name = std::move(InName);
	Model.MaterialSlots.push_back(std::move(InMaterial));
	FModelPrimitive Primitive;
	Primitive.Positions = {-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0};
	Primitive.Indices = {0, 1, 2, 0, 2, 3};
	Primitive.TexCoords0 = {0, 1, 1, 1, 1, 0, 0, 0};
	Primitive.Material = 0;
	Model.Primitives.push_back(Primitive);
	FModelNode Node;
	Node.Primitives = {0};
	Model.Nodes.push_back(Node);
	Model.Roots = {0};
	return Model;
}

struct FFixture
{
	FTaskSystem Tasks{1, 1};
	FWindow Window{"Native shared material assets", {320, 240}, true};
	std::shared_ptr<FNativeOnlyFileSystem> Files = std::make_shared<FNativeOnlyFileSystem>();
	FIOService IO{Tasks, Files};
	FAssetService Assets{IO};
	FShaderCompiler Compiler{std::filesystem::path(HYP_SOURCE_DIR) / "shaders", "shared-asset-shaders"};
	std::filesystem::path Directory = std::filesystem::absolute("shared-asset-gpu");
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::unique_ptr<FRenderSession> Session;
	FAssetRef Material;

	FFixture()
	{
		RegisterSceneAssetTypes(Assets.Types());
		const auto Texture =
		    Store(IO, Directory / "White.hasset",
		          BuildTextureAsset("Shared white", EMaterialTextureEncoding::Srgb, {1, 1, {255, 255, 255, 255}}));
		Material = Store(IO, Directory / "Material.hasset", MakeMaterial(Texture));
		const auto A = Store(IO, Directory / "A.hasset", MakeModel(Material, "Geometry A"));
		const auto B = Store(IO, Directory / "B.hasset", MakeModel(Material, "Geometry B"));
		FSceneManifest Manifest;
		Manifest.Assets = {{"a", A}, {"b", B}};
		Manifest.Instances = {{"left", "a"}, {"right", "b"}};
		for (std::size_t Index = 0; Index < Manifest.Instances.size(); ++Index)
		{
			auto& Entry = Manifest.Instances[Index];
			Entry.Transform = Multiply(Translation({Index == 0 ? -.7f : .7f, 0, 0}), Scale({.4f, .4f, 1}));
			Entry.Surface.Reference = Material;
		}
		Store(IO, Directory / "Scene.hasset", Manifest);
		const auto Surface = Window.Surface();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          FRHIBackendRegistry Registry;
			                          RegisterD3D12RHIBackend(Registry);
			                          Device = Registry.CreateDevice(ERHIBackend::D3D12);
			                          Swapchain = Device->CreateSwapchain({Surface, {320, 240}});
		                          }));
		Session = std::make_unique<FRenderSession>(Tasks, *Device, Compiler);
	}

	~FFixture()
	{
		Assets.Drain();
		Session.reset();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Device->WaitIdle();
			                          Swapchain.reset();
			                          Device.reset();
		                          }));
	}

	void Await(FSceneInstance& InScene)
	{
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
		do
		{
			InScene.Tick();
			if (!InScene.GetStatus().Error.empty())
			{
				throw std::runtime_error(InScene.GetStatus().Error);
			}
			for (const auto& Entry : InScene.GetModels())
			{
				if (const auto Error = InScene.GetError(Entry.Handle); !Error.empty())
				{
					throw std::runtime_error(Error);
				}
			}
			if (InScene.GetStatus().bReady)
			{
				return;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		} while (std::chrono::steady_clock::now() < Deadline);
		throw std::runtime_error("Native shared scene did not become ready");
	}

	FImage Frame(FSceneInstance& InScene)
	{
		Await(InScene);
		Window.Poll();
		FImage Image;
		const auto MaterialFrame = Session->FreezeFrame(0);
		Tasks.Wait(Tasks.Dispatch(
		    {EDomain::Render},
		    [&]
		    {
			    FRenderGraph Graph;
			    auto Clear = MakeColorPass(Graph, "Clear");
			    Clear.Color->Actions.Load = EAttachmentLoad::Clear;
			    Graph.Add(Clear);
			    FRenderView View{
			        Multiply(Perspective(1, 4.f / 3, .1f, 10), LookAt({0, 0, 3}, {0, 0, 0})), {0, 0, 3}, 320, 240};
			    HYP_CHECK(Session->BuildViews(Graph, std::span(&View, 1), Session->FrameTargets(), MaterialFrame, 1,
			                                  false, true) == 2);
			    Image = ExecuteGraph(std::move(Graph), Tasks, *Swapchain, {320, 240}, false, true);
			    Session->CompleteViews();
		    }));
		return Image;
	}
};

void Pixel(const FImage& InImage, unsigned InX, FVec3 InExpected)
{
	const auto Offset = (std::size_t(120) * InImage.Width + InX) * 4;
	HYP_CHECK(std::abs(InImage.Rgba[Offset] - InExpected.X) < .025f);
	HYP_CHECK(std::abs(InImage.Rgba[Offset + 1] - InExpected.Y) < .025f);
	HYP_CHECK(std::abs(InImage.Rgba[Offset + 2] - InExpected.Z) < .025f);
}

void CheckSharingAndSave(FFixture& InFixture)
{
	auto& F = InFixture;
	FSceneInstance Scene(*F.Session, F.Tasks, F.Assets);
	Scene.Load(F.Directory / "Scene.hasset");
	const auto Initial = F.Frame(Scene);
	Pixel(Initial, 110, {1, 0, 0});
	Pixel(Initial, 210, {1, 0, 0});
	const auto Left = Scene.GetModels()[0].Handle;
	const auto Right = Scene.GetModels()[1].Handle;
	const auto A = Scene.Find(Left);
	const auto B = Scene.Find(Right);
	HYP_CHECK(A->Data->Asset != B->Data->Asset);
	HYP_CHECK(A->Data->Materials[0]->Asset == B->Data->Materials[0]->Asset);
	HYP_CHECK(A->Surface.Snapshot == B->Surface.Snapshot);
	const auto Before = F.Session->GetResources().Statistics();
	HYP_CHECK(Before.GeometryUploads == 2 && Before.AssetMaterials.MaterialPreparations == 1);
	HYP_CHECK(Before.AssetMaterials.TextureSources == 1 && Before.Materials.TextureUploads == 1);
	auto Changed = *A;
	Changed.Surface.Instance = std::make_shared<FMaterialInstance>(A->Surface.Snapshot);
	Changed.Surface.Snapshot.reset();
	Changed.Surface.Instance->Set("Tint", FMaterialValue::Float(FVec4{0, 1, 0, 1}));
	HYP_CHECK(Scene.Update(Left, Changed));
	const auto Edited = F.Frame(Scene);
	Pixel(Edited, 110, {0, 1, 0});
	Pixel(Edited, 210, {1, 0, 0});
	const auto After = F.Session->GetResources().Statistics();
	HYP_CHECK(After.GeometryUploads == Before.GeometryUploads);
	HYP_CHECK(After.Materials.TextureUploads == Before.Materials.TextureUploads);
	HYP_CHECK(After.Materials.PipelinesCreated == Before.Materials.PipelinesCreated);
	HYP_CHECK(After.AssetMaterials.MaterialPreparations == Before.AssetMaterials.MaterialPreparations);
	const auto Destination = F.Directory / "saved/Scene.hasset";
	const auto Snapshot = Scene.Snapshot(Destination);
	HYP_CHECK(Snapshot.Instances[0].Surface.Values.size() == 1);
	HYP_CHECK(Snapshot.Instances[0].Surface.Reference->Path == "../Material.hasset");
	Store(F.IO, Destination, Snapshot);
	Scene.Close();
	F.Assets.ClearCache();
	Scene.Load(Destination);
	const auto Reloaded = F.Frame(Scene);
	Pixel(Reloaded, 110, {0, 1, 0});
	Pixel(Reloaded, 210, {1, 0, 0});
	HYP_CHECK(Reloaded.Rgba == Edited.Rgba);
	std::cout << "Two model assets: geometry_uploads=" << Before.GeometryUploads
	          << " material_preparations=" << Before.AssetMaterials.MaterialPreparations
	          << " texture_sources=" << Before.AssetMaterials.TextureSources
	          << " texture_uploads=" << Before.Materials.TextureUploads
	          << "; numeric edit: geometry/texture/pipeline deltas=0; Save As pixels identical\n";
}

void CheckSectionVariants(FFixture& InFixture)
{
	auto& F = InFixture;
	FSceneInstance Scene(*F.Session, F.Tasks, F.Assets);
	Scene.Load(F.Directory / "Scene.hasset");
	F.Frame(Scene);
	const auto Before = F.Session->GetResources().Statistics();
	const auto Srgb =
	    Store(F.IO, F.Directory / "GraySrgb.hasset",
	          BuildTextureAsset("gray color", EMaterialTextureEncoding::Srgb, {1, 1, {128, 128, 128, 255}}));
	const auto Linear =
	    Store(F.IO, F.Directory / "GrayLinear.hasset",
	          BuildTextureAsset("gray data", EMaterialTextureEncoding::Linear, {1, 1, {128, 128, 128, 255}}));
	FSceneMaterialAsset Selection;
	Selection.Reference = F.Material;
	Selection.Values.push_back({"Tint", PersistMaterialValue(FMaterialValue::Float(FVec4{1, 1, 1, 1}))});
	FMaterialAssetValue Texture;
	Texture.Type = FMaterialParameterType::Resource(EMaterialValueKind::Texture2D);
	Texture.Texture = Srgb;
	Selection.Values.push_back({"ImageTexture", Texture});
	for (std::size_t Index = 0; Index < Scene.GetModels().size(); ++Index)
	{
		if (Index == 1)
		{
			FMaterialSampler Nearest;
			Nearest.bMinLinear = false;
			Nearest.bMagLinear = false;
			Nearest.bMipLinear = false;
			Selection.Overrides.push_back({"ImageSampler", PersistMaterialValue(FMaterialValue::FromSampler(Nearest))});
		}
		const auto Handle = Scene.GetModels()[Index].Handle;
		auto Model = *Scene.Find(Handle);
		Model.SectionSurfaces[0] = *LoadSceneMaterialSelection(F.Assets, F.Tasks, F.Session->GetResources(), Selection,
		                                                       F.Directory / "Scene.hasset")
		                                .Get(F.Tasks);
		HYP_CHECK(Scene.Update(Handle, std::move(Model)));
	}
	const auto SameTexture = F.Frame(Scene);
	Pixel(SameTexture, 110, {.502f, .502f, .502f});
	Pixel(SameTexture, 210, {.502f, .502f, .502f});
	const auto Shared = F.Session->GetResources().Statistics();
	HYP_CHECK(Shared.Materials.TextureUploads == Before.Materials.TextureUploads + 1);
	HYP_CHECK(Shared.Materials.SamplersCreated == Before.Materials.SamplersCreated + 1);
	HYP_CHECK(Shared.GeometryUploads == Before.GeometryUploads);
	Selection.Values[1].Value.Texture = Linear;
	const auto Right = Scene.GetModels()[1].Handle;
	auto Model = *Scene.Find(Right);
	Model.SectionSurfaces[0] = *LoadSceneMaterialSelection(F.Assets, F.Tasks, F.Session->GetResources(), Selection,
	                                                       F.Directory / "Scene.hasset")
	                                .Get(F.Tasks);
	Scene.Update(Right, Model);
	const auto DifferentEncoding = F.Frame(Scene);
	Pixel(DifferentEncoding, 110, {.502f, .502f, .502f});
	Pixel(DifferentEncoding, 210, {.737f, .737f, .737f});
	HYP_CHECK(F.Session->GetResources().Statistics().Materials.TextureUploads == Shared.Materials.TextureUploads + 1);
	const auto Destination = F.Directory / "variants/Scene.hasset";
	const auto Saved = Scene.Snapshot(Destination);
	HYP_CHECK(Saved.Instances[1].SectionSurfaces[0].Material.Overrides.size() == 1);
	Store(F.IO, Destination, Saved);
	Scene.Close();
	Scene.Load(Destination);
	const auto Reloaded = F.Frame(Scene);
	HYP_CHECK(Reloaded.Rgba == DifferentEncoding.Rgba);
	std::cout << "Section assets: sampler-only change shares one upload; sRGB/linear pixels 0.502/0.737; typed "
	             "texture/sampler Save As round-trip passed\n";
}

void CheckRawCacheUpgrade(FFixture& InFixture, FSceneInstance& InScene,
                          const std::shared_ptr<const FSceneModelData>& InData)
{
	auto& F = InFixture;
	auto& Resources = F.Session->GetResources();
	const auto Before = Resources.Statistics().AssetMaterials;
	const auto Geometry = Resources.RequestModel(InData);
	const auto Raw = Geometry->GetDescription()->Materials[0].Surface;
	HYP_CHECK(!Raw->Schema->IsPrepared());
	{
		std::shared_ptr<const FMaterialSnapshot> Prepared;
		F.Tasks.Wait(F.Tasks.Dispatch({EDomain::Worker},
		                              [&]
		                              {
			                              Prepared = Resources.PrepareMaterialAsset(InData->Materials[0]);
		                              }));
		HYP_CHECK(Prepared->Schema->IsPrepared() && Prepared->Definition == Raw->Definition);
		HYP_CHECK(Prepared->Overrides == Raw->Overrides);
	}
	const auto Copy = InScene.Add({"same raw material after temporary preparation", InData});
	F.Await(InScene);
	HYP_CHECK(Resources.Statistics().AssetMaterials.MaterialPreparations == Before.MaterialPreparations);
	HYP_CHECK(Resources.Statistics().AssetMaterials.TextureSources == Before.TextureSources);
	HYP_CHECK(InScene.Remove(Copy));
	InScene.Tick();
	std::cout << "Raw material survives temporary prepared snapshot release with one definition/texture source\n";
}

void CheckDependencyRevision(FFixture& InFixture, bool bInPrepared)
{
	auto& F = InFixture;
	const auto Before = F.Session->GetResources().Statistics();
	const auto MaterialPath =
	    F.Directory / (bInPrepared ? "VersionedMaterial.hasset" : "DefaultVersionedMaterial.hasset");
	const auto ModelPath = F.Directory / (bInPrepared ? "VersionedModel.hasset" : "DefaultVersionedModel.hasset");
	auto Material = *F.Assets.LoadAsync<FMaterialAsset>(F.Directory / "Material.hasset").Get(F.Tasks);
	auto Reference = Store(F.IO, MaterialPath, Material);
	Reference.Id.clear();
	Reference.Revision.clear();
	Store(F.IO, ModelPath, MakeModel(Reference, "One geometry, two dependency revisions"));
	auto* Resources = bInPrepared ? &F.Session->GetResources() : nullptr;
	const auto Old = LoadNativeModel(F.Assets, F.Tasks, ModelPath, {}, Resources).Get(F.Tasks);
	FSceneInstance Scene(*F.Session, F.Tasks, F.Assets);
	Scene.Add({"old", Old, Multiply(Translation({-.7f, 0, 0}), Scale({.4f, .4f, 1}))});
	F.Await(Scene);
	if (!bInPrepared)
	{
		CheckRawCacheUpgrade(F, Scene, Old);
	}
	Material.Parameters.front().Default = PersistMaterialValue(FMaterialValue::Float(FVec4{0, 0, 1, 1}));
	F.Assets.SaveAsync(MaterialPath, std::make_shared<const FMaterialAsset>(Material)).Get(F.Tasks);
	const auto New = LoadNativeModel(F.Assets, F.Tasks, ModelPath, {}, Resources).Get(F.Tasks);
	HYP_CHECK(Old->Asset == New->Asset && Old->Materials[0]->Asset != New->Materials[0]->Asset);
	if (bInPrepared)
	{
		HYP_CHECK(Old->MaterialSnapshots[0]->Definition != New->MaterialSnapshots[0]->Definition);
	}
	else
	{
		HYP_CHECK(Old->MaterialSnapshots.empty() && New->MaterialSnapshots.empty());
	}
	Scene.Add({"new", New, Multiply(Translation({.7f, 0, 0}), Scale({.4f, .4f, 1}))});
	const auto Image = F.Frame(Scene);
	Pixel(Image, 110, {1, 0, 0});
	Pixel(Image, 210, {0, 0, 1});
	HYP_CHECK(F.Session->GetResources().Statistics().GeometryUploads == Before.GeometryUploads + 1);
	HYP_CHECK(F.Session->GetResources().Statistics().AssetMaterials.MaterialPreparations ==
	          Before.AssetMaterials.MaterialPreparations + 2);
	std::cout << (bInPrepared ? "Prepared API: " : "Default API: ")
	          << "New material dependency revision: old red/new blue coexist with one geometry upload\n";
}

void CheckTextureCacheBound(FFixture& InFixture)
{
	std::map<FAssetRef, std::shared_ptr<const FTextureAsset>> Textures;
	FMaterialAssetValues Values;
	for (std::size_t Index = 0; Index < 4097; ++Index)
	{
		FAssetRef Ref{"", "texture-" + std::to_string(Index) + ".hasset", RecordType<FTextureAsset>().Id, ""};
		Textures.emplace(Ref, std::make_shared<const FTextureAsset>(BuildTextureAsset(
		                          "bounded", EMaterialTextureEncoding::Linear, {1, 1, {255, 255, 255, 255}})));
		FMaterialAssetValue Value;
		Value.Type = FMaterialParameterType::Resource(EMaterialValueKind::Texture2D);
		Value.Texture = Ref;
		Values.push_back({std::to_string(Index), Value});
	}
	const auto Resolved = InFixture.Session->GetResources().PrepareAssetValues(Values, Textures);
	HYP_CHECK(Resolved.size() == 4097);
	HYP_CHECK(InFixture.Session->GetResources().Statistics().AssetMaterials.TextureEntries <= 4096);
	HYP_CHECK(Resolved.front().Value.Texture->GetMips()[0].Bytes[0] == 255);
	std::cout << "4097 live CPU texture sources: metadata bounded to 4096; evicted sources remain valid\n";
}

void CheckMissingAndRevision(FFixture& InFixture)
{
	auto& F = InFixture;
	FSceneMaterialAsset Invalid;
	Invalid.Reference = F.Material;
	Invalid.Reference->Path = "Missing.hasset";
	bool bRejected{};
	try
	{
		LoadSceneMaterialSelection(F.Assets, F.Tasks, F.Session->GetResources(), Invalid, F.Directory / "Scene.hasset")
		    .Get(F.Tasks);
	}
	catch (const std::exception& Error)
	{
		bRejected = std::string(Error.what()).find("Missing.hasset") != std::string::npos;
	}
	HYP_CHECK(bRejected);
	Invalid.Reference = F.Material;
	Invalid.Reference->Revision = std::string(64, '0');
	bRejected = false;
	try
	{
		LoadSceneMaterialSelection(F.Assets, F.Tasks, F.Session->GetResources(), Invalid, F.Directory / "Scene.hasset")
		    .Get(F.Tasks);
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
	HYP_CHECK(F.Files->DeniedReads == 0);
}
} // namespace

int main()
{
	try
	{
		FFixture Fixture;
		CheckSharingAndSave(Fixture);
		CheckSectionVariants(Fixture);
		CheckDependencyRevision(Fixture, true);
		CheckDependencyRevision(Fixture, false);
		CheckMissingAndRevision(Fixture);
		CheckTextureCacheBound(Fixture);
		std::cout << "Native-only graph, authored shader path/entry/define, one Worker, instance isolation and missing "
		             "pins passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
