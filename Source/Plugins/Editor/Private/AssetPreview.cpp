#include "AssetWorkspace.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Materials/PbrMaterial.h"
#include "Hyperion/Renderer/NativeModel.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include <algorithm>
#include <bit>

namespace Hyperion
{
namespace
{
FAssetGraph DraftGraph(FAssetService& InAssets, FTaskSystem& InTasks, std::shared_ptr<const FLoadedAsset> InRoot,
                       FCancellationToken InCancellation)
{
	FAssetGraph Graph;
	Graph.Root = InRoot;
	Graph.Assets.emplace(InRoot->Path, InRoot);
	for (const auto& Dependency : InRoot->Header.Dependencies)
	{
		InCancellation.Check();
		const auto Part = InAssets.LoadGraphAsync(Dependency.Reference, InRoot->Path).Get(InTasks);
		Graph.Assets.insert(Part->Assets.begin(), Part->Assets.end());
		Graph.Failures.insert(Graph.Failures.end(), Part->Failures.begin(), Part->Failures.end());
	}
	return Graph;
}

std::shared_ptr<const FSceneModelData> PreviewModel(FAssetService& InAssets, FTaskSystem& InTasks,
                                                    FRenderResourceService& InResources, std::size_t InShape,
                                                    std::shared_ptr<const FMaterialAssetData> InMaterial,
                                                    FCancellationToken InCancellation,
                                                    std::shared_ptr<const FSceneModelData> InExisting = {})
{
	if (InExisting)
	{
		auto Data = std::make_shared<FSceneModelData>(*InExisting);
		Data->Materials = {InMaterial};
		Data->MaterialSnapshots = {InResources.PrepareMaterialAsset(InMaterial)};
		return Data;
	}
	constexpr std::array Names{"Sphere", "Plane", "Cube"};
	const auto Geometry =
	    InAssets.LoadAsync<FModelAsset>(std::string("/Engine/Models/Primitives/") + Names.at(InShape) + ".hasset")
	        .Get(InTasks);
	InCancellation.Check();
	auto Model = std::make_shared<FModelAsset>(*Geometry);
	for (auto& Primitive : Model->Primitives)
	{
		Primitive.Material = 0;
	}
	Model->MaterialSlots = {{"", "preview.hasset", RecordType<FMaterialAsset>().Id, ""}};
	auto Data = std::make_shared<FSceneModelData>(*PrepareSceneModel(Model, {InMaterial}));
	Data->MaterialSnapshots.push_back(InResources.PrepareMaterialAsset(InMaterial));
	return Data;
}

std::shared_ptr<const FMaterialAssetData> ReferenceSurface(FAssetService& InAssets, FTaskSystem& InTasks, bool bInMetal)
{
	const auto Graph = InAssets.LoadGraphAsync("/Engine/Materials/DefaultPrimitive.hasset").Get(InTasks);
	auto Data = std::make_shared<FMaterialAssetData>(*ResolveMaterialAssetGraph(*Graph, *Graph->Root, InAssets));
	auto Material = std::make_shared<FMaterialAsset>(*Data->Asset);
	Material->Name = bInMetal ? "Reflective" : "Diffuse";
	for (auto& Entry : Material->Values)
	{
		if (Entry.Name == "Pbr.MetallicFactor")
		{
			Entry.Value.Words = {std::bit_cast<std::uint32_t>(bInMetal ? 1.f : 0.f)};
		}
		if (Entry.Name == "Pbr.RoughnessFactor")
		{
			Entry.Value.Words = {std::bit_cast<std::uint32_t>(bInMetal ? .08f : .8f)};
		}
	}
	Data->Asset = Material;
	return Data;
}
} // namespace

FAssetWorkspace::FPrepared FAssetWorkspace::Prepare(const FLoadedAsset& InLoaded, FArchiveNode InDraft,
                                                    std::size_t InShape, FCancellationToken InCancellation,
                                                    std::shared_ptr<const FSceneModelData> InExisting)
{
	FPrepared Result;
	try
	{
		InCancellation.Check();
		auto Root = std::make_shared<FLoadedAsset>(InLoaded);
		Root->Object = ReadRecord(*Root->Type, InDraft);
		Root->Header.Dependencies = CollectAssetDependencies(*Root->Type, Root->Object.get());
		Result.Root = Root;
		Result.Dependencies.insert(Root->Header.Id);
		for (const auto& Dependency : Root->Header.Dependencies)
		{
			Result.Dependencies.insert(Dependency.Reference.Id);
		}
		const auto& Type = Root->Header.TypeId;
		if (Type == RecordType<FTextureAsset>().Id)
		{
			return Result;
		}
		auto& Resources = Session.GetResources();
		if (Type == RecordType<FModelAsset>().Id)
		{
			const auto Graph = DraftGraph(Assets, Tasks, Root, InCancellation);
			for (const auto& [Path, Asset] : Graph.Assets)
			{
				Result.Dependencies.insert(Asset->Header.Id);
			}
			auto Data = std::make_shared<FSceneModelData>(*ResolveModelAssetGraph(Graph, Assets));
			for (const auto& Material : Data->Materials)
			{
				InCancellation.Check();
				Data->MaterialSnapshots.push_back(Resources.PrepareMaterialAsset(Material));
			}
			Result.Model = std::move(Data);
		}
		else if (Type == RecordType<FMaterialAsset>().Id)
		{
			const auto Graph = DraftGraph(Assets, Tasks, Root, InCancellation);
			for (const auto& [Path, Asset] : Graph.Assets)
			{
				Result.Dependencies.insert(Asset->Header.Id);
			}
			const auto Material = ResolveMaterialAssetGraph(Graph, *Root, Assets);
			Result.Model = PreviewModel(Assets, Tasks, Resources, InShape, Material, InCancellation, InExisting);
		}
		else
		{
			const auto Sky = Root->As<FSkyAsset>();
			const std::array References{Sky->Radiance, Sky->Specular, Sky->Brdf};
			for (std::size_t Index = 0; Index < References.size(); ++Index)
			{
				const auto Product = Assets.LoadReferenceAsync(References[Index], Root->Path).Get(Tasks);
				Result.SkyProducts[Index] = Product->As<FTextureAsset>();
				Result.Dependencies.insert(Product->Header.Id);
			}
			Result.Model =
			    PreviewModel(Assets, Tasks, Resources, 0, ReferenceSurface(Assets, Tasks, false), InCancellation);
			Result.SecondModel =
			    PreviewModel(Assets, Tasks, Resources, 0, ReferenceSurface(Assets, Tasks, true), InCancellation);
		}
		InCancellation.Check();
	}
	catch (const std::exception& Failure)
	{
		InCancellation.Check();
		Result.Error = Failure.what();
	}
	return Result;
}

void FAssetWorkspace::Publish(FEntry& InEntry, const FPrepared& InPrepared)
{
	InEntry.Preview = InPrepared.Root;
	InEntry.Dependencies = InPrepared.Dependencies;
	InEntry.PreviewModel = InPrepared.Model;
	InEntry.SkyProducts = InPrepared.SkyProducts;
	InEntry.Error.clear();
	++InEntry.Texture.Revision;
	if (!InPrepared.Model)
	{
		return;
	}
	// A preview scene owns only transient presentation objects; the edited scene is never replaced here.
	if (!InEntry.PreviewSession)
	{
		InEntry.PreviewSession = std::make_unique<FRenderSession>(Tasks, Session.GetResources(), Capabilities);
	}
	InEntry.Scene.reset();
	InEntry.Scene = std::make_unique<FSceneInstance>(*InEntry.PreviewSession, Tasks, Assets);
	if (!InEntry.Pipeline)
	{
		InEntry.Pipeline = std::make_unique<FSceneRenderPipeline>(*InEntry.PreviewSession, Capabilities);
	}
	FSceneModel Model;
	Model.Name = "Preview";
	Model.Data = InPrepared.Model;
	if (InPrepared.SecondModel)
	{
		Model.World = Translation({-1.2f, 0, 0});
	}
	InEntry.Scene->Add(Model);
	if (InPrepared.SecondModel)
	{
		Model.Data = InPrepared.SecondModel;
		Model.World = Translation({1.2f, 0, 0});
		InEntry.Scene->Add(Model);
	}
	auto Environment = MakeSceneEnvironmentLightNode("preview-environment");
	Environment.EnvironmentLight()->YawRadians = InEntry.YawDegrees / 57.2957795f;
	FSceneSettings SceneSettings;
	if (InPrepared.Root->Header.TypeId == RecordType<FSkyAsset>().Id)
	{
		Environment.EnvironmentLight()->Source = ESceneEnvironmentSource::SkyAsset;
		Environment.EnvironmentLight()->Sky =
		    FAssetRef{InPrepared.Root->Header.Id, PathToUtf8(InEntry.Path), RecordType<FSkyAsset>().Id, {}};
	}
	else
	{
		Environment.EnvironmentLight()->Intensity = .25f;
		auto Light = MakeSceneDirectionalLightNode("preview-key");
		Light.Local() = SceneCameraTransform({0, 0, 0}, {-.5f, -1, -.7f});
		Light.DirectionalLight()->Intensity = 3;
		SceneSettings.MainDirectionalLight = InEntry.Scene->AddNode(std::move(Light));
	}
	SceneSettings.EnvironmentLight = InEntry.Scene->AddNode(std::move(Environment));
	InEntry.Scene->SetSettings(SceneSettings);
	InEntry.Scene->Tick();
	if (!InEntry.bCameraInitialized)
	{
		InEntry.Camera = MakeSceneBrowsingView(*InEntry.Scene, 1.5f);
		FitSceneCamera(InEntry.Camera, *InEntry.Scene, 1.5f, true);
		InEntry.bCameraInitialized = true;
	}
}

void FAssetWorkspace::DrawPreview(FGui& InGui, FEntry& InEntry, float InDelta, std::span<const FInputEvent> InEvents)
{
	if (!InEntry.Document)
	{
		InGui.TextWrapped(InEntry.Error.empty() ? "Loading asset..." : InEntry.Error);
		return;
	}
	if (InGui.Button("Save", !InEntry.bReadOnly && !InEntry.HasPendingEdit() && !InEntry.Document->IsSaving()))
	{
		SaveActive();
	}
	InGui.SameLine();
	if (InGui.Button("Undo", CanUndo()))
	{
		Undo();
	}
	InGui.SameLine();
	if (InGui.Button("Redo", CanRedo()))
	{
		Redo();
	}
	InGui.SameLine();
	InGui.Text(InEntry.HasPendingEdit()       ? "Preparing edit..."
	           : InEntry.Document->IsSaving() ? "Saving..."
	           : InEntry.Document->IsDirty()  ? "Unsaved changes"
	                                          : "Saved");
	if (!InEntry.Error.empty())
	{
		InGui.TextWrapped(InEntry.Error);
	}
	if (!InEntry.Document->Error.empty())
	{
		InGui.TextWrapped(InEntry.Document->Error);
	}
	if (InEntry.Document->Loaded().Header.TypeId == RecordType<FTextureAsset>().Id)
	{
		DrawTexture(InGui, InEntry, InEvents);
		return;
	}
	if (InEntry.Document->Loaded().Header.TypeId == RecordType<FMaterialAsset>().Id)
	{
		const std::array<std::string, 3> Shapes{"Sphere", "Plane", "Cube"};
		auto Shape = InEntry.Shape;
		if (InGui.Combo("Preview mesh", Shapes, Shape))
		{
			FAssetPreviewSettings Settings;
			Settings.Shape = static_cast<std::uint32_t>(Shape);
			SetPreviewSettings(InEntry, Settings);
		}
	}
	if (!InEntry.Scene)
	{
		InGui.Text("Preparing preview...");
		return;
	}
	auto Yaw = InEntry.YawDegrees;
	if (InEntry.Document->Loaded().Header.TypeId == RecordType<FSkyAsset>().Id &&
	    InGui.Slider("Orientation (preview)", Yaw, -180, 180))
	{
		FAssetPreviewSettings Settings;
		Settings.Yaw = Yaw;
		SetPreviewSettings(InEntry, Settings);
	}
	if (InGui.Button("Frame"))
	{
		FramePreview(InEntry);
	}
	InGui.SameLine();
	InGui.SetNextItemWidth(140);
	auto Exposure = InEntry.Exposure;
	if (InGui.Slider("Exposure (preview)", Exposure, .05f, 8))
	{
		FAssetPreviewSettings Settings;
		Settings.Exposure = Exposure;
		SetPreviewSettings(InEntry, Settings);
	}
	InEntry.Region = InGui.Image(InEntry.TextureId);
	Bounds["canvas"] = InEntry.Region.Bounds;
	const auto& B = InEntry.Region.Bounds;
	const auto Scale = InGui.FramebufferScale();
	const FSize Size{std::clamp(static_cast<std::uint32_t>(std::max(1.f, (B.Z - B.X) * Scale.X)), 1u, 4096u),
	                 std::clamp(static_cast<std::uint32_t>(std::max(1.f, (B.W - B.Y) * Scale.Y)), 1u, 4096u)};
	if (Size.Width != InEntry.Size.Width || Size.Height != InEntry.Size.Height)
	{
		InEntry.Size = Size;
		InEntry.Target = {ERenderTargetKind::Texture,
		                  std::make_shared<const FMaterialTextureSource>(
		                      FMaterialColorTexture{Size.Width, Size.Height, EMaterialColorFormat::Rgba8Unorm}),
		                  Session.GetResources().CreateScopeLifetime(), false};
	}
	InEntry.Navigation.Input(InEntry.Camera, InEvents, !InEntry.Region.bHovered,
	                         !InEntry.Region.bFocused || InGui.IsEditingText());
	InEntry.Navigation.Advance(InEntry.Camera, InDelta);
	const auto& Status = InEntry.Scene->GetStatus();
	if (!Status.Error.empty())
	{
		InEntry.Error = Status.Error;
	}
}

void FAssetWorkspace::PrepareFrame()
{
	for (const auto& Entry : Entries)
	{
		Entry->Seed.reset();
		if (Entry->bVisible && Entry->Scene && Entry->Target.Texture)
		{
			Entry->Scene->Tick();
			Entry->Seed = Entry->PreviewSession->FreezeSceneFrame(Entry->Scene->GetToken(), 0);
		}
	}
}

void FAssetWorkspace::Build(FRenderGraph& InGraph, std::vector<FGuiTextureBinding>& OutTextures)
{
	for (const auto& Entry : Entries)
	{
		if (!Entry->bVisible)
		{
			continue;
		}
		if (Entry->Texture.Target.Texture)
		{
			OutTextures.push_back({Entry->TextureId, Entry->Texture.Target});
		}
		if (!Entry->Seed)
		{
			continue;
		}
		FSceneViewRequest Request;
		Request.Width = Entry->Size.Width;
		Request.Height = Entry->Size.Height;
		Request.DepthConvention = EDepthConvention::Reversed;
		Request.CameraOverride = Entry->Camera;
		FScenePipelineSettings Settings;
		Settings.Exposure = Entry->Exposure;
		Entry->Pipeline->Configure(Settings);
		Entry->Pipeline->SetOutputTarget(Entry->Target);
		Entry->Pipeline->Build(InGraph, Request, Entry->Seed, {}, {.06f, .06f, .06f, 1}, {}, true);
		OutTextures.push_back({Entry->TextureId, Entry->Target});
	}
}
} // namespace Hyperion
