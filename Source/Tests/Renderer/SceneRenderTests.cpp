#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/CascadedShadowMap.h"
#include "Hyperion/Renderer/Model.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/SceneBridge.h"
#include "Support/GraphTestSupport.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <source_location>
#include <thread>

namespace
{
using namespace Hyperion;

FModelAsset Quad(FVec4 InColor = {1, 1, 1, 1}, float InDepth = 0, bool bInBlend = false)
{
	FModelAsset Asset;
	FModelPrimitive Primitive;
	Primitive.Positions = {-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0};
	Primitive.Indices = {0, 1, 2, 0, 2, 3};
	Primitive.Material = 0;
	Asset.Primitives.push_back(Primitive);
	FModelMaterial Material;
	Material.BaseColor = InColor;
	Material.bUnlit = true;
	Material.AlphaMode = bInBlend ? EAlphaMode::Blend : EAlphaMode::Opaque;
	Asset.Materials.push_back(Material);
	FModelNode Node;
	Node.Primitives = {0};
	Node.Local = Translation({0, 0, InDepth});
	Asset.Nodes.push_back(Node);
	Asset.Roots = {0};
	return Asset;
}

void AwaitModel(const FModel& InModel)
{
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (!InModel.IsReady() && InModel.GetError().empty() && std::chrono::steady_clock::now() < Deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if (!InModel.GetError().empty())
	{
		throw std::runtime_error(InModel.GetError());
	}
	HYP_CHECK(InModel.IsReady());
}

void Pixel(const FImage& InImage, unsigned InX, FVec3 InExpected,
           std::source_location InLocation = std::source_location::current())
{
	const auto Offset = (std::size_t(120) * InImage.Width + InX) * 4;
	if (std::abs(InImage.Rgba[Offset] - InExpected.X) >= .025f ||
	    std::abs(InImage.Rgba[Offset + 1] - InExpected.Y) >= .025f ||
	    std::abs(InImage.Rgba[Offset + 2] - InExpected.Z) >= .025f)
	{
		std::cerr << "Pixel caller line " << InLocation.line() << ": actual " << InImage.Rgba[Offset] << ','
		          << InImage.Rgba[Offset + 1] << ',' << InImage.Rgba[Offset + 2] << " expected " << InExpected.X << ','
		          << InExpected.Y << ',' << InExpected.Z << '\n';
	}
	HYP_CHECK(std::abs(InImage.Rgba[Offset] - InExpected.X) < .025f);
	HYP_CHECK(std::abs(InImage.Rgba[Offset + 1] - InExpected.Y) < .025f);
	HYP_CHECK(std::abs(InImage.Rgba[Offset + 2] - InExpected.Z) < .025f);
}

struct FSceneFixture
{
	FTaskSystem Tasks{1, 2};
	FWindow Window{"Shared render primitives", {320, 240}, true};
	FShaderCompiler Compiler{std::filesystem::path(HYP_SOURCE_DIR) / "shaders",
	                         std::filesystem::absolute("scene-render-shader-cache")};
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::unique_ptr<FRenderSession> Session;
	FSceneVisibilityStats LastStatistics;
	std::uint64_t FrameNumber{};

	FSceneFixture()
	{
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

	~FSceneFixture()
	{
		Session->Close();
		Session.reset();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Device->WaitIdle();
			                          Swapchain.reset();
			                          Device.reset();
		                          }));
	}

	FImage Frame(std::size_t InExpectedItems, ESceneCullingMode InMode = ESceneCullingMode::Bvh,
	             const std::function<void(FRenderView&)>& InConfigure = {})
	{
		Window.Poll();
		FImage Image;
		const auto MaterialFrame = Session->FreezeFrame(0);
		FrameNumber = MaterialFrame->Frame;
		Tasks.Wait(Tasks.Dispatch(
		    {EDomain::Render},
		    [&]
		    {
			    FRenderGraph Graph;
			    auto Clear = MakeColorPass(Graph, "pending");
			    Clear.Color->Actions.Load = EAttachmentLoad::Clear;
			    Clear.Name = "Clear";
			    Graph.Add(Clear);
			    FRenderView View{
			        Multiply(Perspective(1, 4.f / 3, .1f, 10), LookAt({0, 0, 3}, {0, 0, 0})), {0, 0, 3}, 320, 240};
			    View.CullingMode = InMode;
			    if (InConfigure)
			    {
				    InConfigure(View);
			    }
			    HYP_CHECK(Session->BuildViews(Graph, std::span(&View, 1), Session->FrameTargets(), MaterialFrame, 1,
			                                  false, true) == InExpectedItems);
			    Image = ExecuteGraph(std::move(Graph), Tasks, *Swapchain, {320, 240}, false, true);
			    Session->CompleteViews();
			    LastStatistics = Session->Statistics();
		    }));
		return Image;
	}

	void AwaitRetirement()
	{
		Tasks.Wait(Session->GetScene().Flush());
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (Session->GetResources().Statistics().LiveResources && std::chrono::steady_clock::now() < Deadline)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		HYP_CHECK(Session->GetResources().Statistics().LiveResources == 0);
	}
};

void CheckSharedModels(FSceneFixture& InFixture)
{
	auto& Scene = InFixture.Session->GetScene();
	auto& Resources = InFixture.Session->GetResources();
	const auto Before = Resources.Statistics();
	auto Asset = std::make_shared<const FModelAsset>(Quad());
	FModel Left(Scene, Resources, Asset);
	FModel Right(Scene, Resources, Asset);
	HYP_CHECK(Left.GetResource() == Right.GetResource());
	Left.SetTransform(Multiply(Translation({-.7f, 0, 0}), Scale({.4f, .4f, 1})));
	Right.SetTransform(Multiply(Translation({.7f, 0, 0}), Scale({.4f, .4f, 1})));
	FMaterialOverride Red;
	Red.BaseColor = FVec4{1, 0, 0, 1};
	FMaterialOverride Blue;
	Blue.BaseColor = FVec4{0, 0, 1, 1};
	Left.SetMaterial(Red);
	Right.SetMaterial(Blue);
	AwaitModel(Left);
	AwaitModel(Right);
	HYP_CHECK(Resources.Statistics().GeometryUploads == Before.GeometryUploads + 1);
	auto Image = InFixture.Frame(2);
	Pixel(Image, 110, {1, 0, 0});
	Pixel(Image, 210, {0, 0, 1});
	// Exact incremental upload counts require the previous GPU cache's owners to remain live.
	FRenderSceneSnapshot Previous;
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              FRenderView View;
		                                              View.CullingMode = ESceneCullingMode::None;
		                                              Previous = Scene.Collect(View);
	                                              }));
	Left.SetTransform(Multiply(Translation({-.65f, 0, 0}), Scale({.4f, .4f, 1})));
	InFixture.Frame(2);
	HYP_CHECK(InFixture.LastStatistics.Batches.PackedRecords == 1);
	HYP_CHECK(InFixture.LastStatistics.Batches.AssembledBlocks == 1);
	HYP_CHECK(InFixture.LastStatistics.Batches.ReusedBlocks == 1);
	const auto Compiled = Left.GetResource()->GetMaterial(0)->GetCompiled();
	const auto& Bindings = Compiled->GetPass("Forward", "Instance").Bindings;
	std::size_t ObjectBytes{};
	for (const auto& Binding : Bindings)
	{
		if (Binding.InstanceStride && Binding.Resource.Name == "HyperionObjectV1")
		{
			ObjectBytes = Binding.InstanceStride * 2;
		}
	}
	HYP_CHECK(ObjectBytes && InFixture.LastStatistics.Batches.UploadBytes == ObjectBytes);
	Previous = {};
	Left.SetVisible(false);
	Image = InFixture.Frame(1);
	Pixel(Image, 110, {0, 0, 0});
	Pixel(Image, 210, {0, 0, 1});
	Left.SetVisible(true);
	Left.SetTransform(Multiply(Translation({-.7f, .8f, 0}), Scale({.4f, .4f, 1})));
	Image = InFixture.Frame(2);
	Pixel(Image, 110, {0, 0, 0});
	Pixel(Image, 210, {0, 0, 1});
	Left.Remove();
	Image = InFixture.Frame(1);
	Pixel(Image, 210, {0, 0, 1});
	HYP_CHECK(Resources.Statistics().GeometryUploads == Before.GeometryUploads + 1);
	Right.Remove();
	InFixture.AwaitRetirement(); // No new frame after removing the last instance.
}

void CheckSceneDepth(FSceneFixture& InFixture)
{
	auto& Scene = InFixture.Session->GetScene();
	auto& Resources = InFixture.Session->GetResources();
	FModel Near(Scene, Resources, std::make_shared<const FModelAsset>(Quad({1, 0, 0, 1}, .5f)));
	FModel Far(Scene, Resources, std::make_shared<const FModelAsset>(Quad({0, 0, 1, 1}, 0)));
	AwaitModel(Near);
	AwaitModel(Far);
	Pixel(InFixture.Frame(2), 160, {1, 0, 0}); // The second model must preserve the first model's depth.
	Near.Remove();
	Far.Remove();
	InFixture.AwaitRetirement();
}

FModelAsset Pair(FVec4 InNear, float InNearDepth, FVec4 InFar, float InFarDepth)
{
	auto Asset = Quad(InNear, InNearDepth, true);
	auto Far = Quad(InFar, InFarDepth, true);
	Far.Primitives[0].Material = 1;
	Far.Nodes[0].Primitives = {1};
	Asset.Primitives.push_back(Far.Primitives[0]);
	Asset.Materials.push_back(Far.Materials[0]);
	Asset.Nodes.push_back(Far.Nodes[0]);
	Asset.Roots.push_back(1);
	return Asset;
}

void CheckGlobalBlend(FSceneFixture& InFixture)
{
	auto& Scene = InFixture.Session->GetScene();
	auto& Resources = InFixture.Session->GetResources();
	FModel A(Scene, Resources, std::make_shared<const FModelAsset>(Pair({1, 0, 0, .5f}, 1, {0, 1, 0, .5f}, -.5f)));
	FModel B(Scene, Resources, std::make_shared<const FModelAsset>(Pair({0, 0, 1, .5f}, .5f, {1, 1, 0, .5f}, -1)));
	AwaitModel(A);
	AwaitModel(B);
	const auto Srgb = [](float InValue)
	{
		return 1.055f * std::pow(InValue, 1 / 2.4f) - .055f;
	};
	Pixel(InFixture.Frame(4), 160, {Srgb(.5625f), Srgb(.1875f), Srgb(.25f)});
	InFixture.Frame(4, ESceneCullingMode::Bvh,
	                [](FRenderView& InView)
	                {
		                InView.ViewProjection = Multiply(Translation({.01f, 0, 0}), InView.ViewProjection);
	                });
	HYP_CHECK(InFixture.LastStatistics.MembershipReuses == 0);
	HYP_CHECK(InFixture.LastStatistics.Batches.IncrementalItemReuses == 0);
	A.Remove();
	B.Remove();
	InFixture.AwaitRetirement();
}

void CheckManyPrimitives(FSceneFixture& InFixture)
{
	auto Asset = std::make_shared<const FModelAsset>(Quad());
	std::vector<std::unique_ptr<FModel>> Models;
	for (unsigned Index = 0; Index < 40; ++Index)
	{
		Models.push_back(
		    std::make_unique<FModel>(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Asset));
	}
	for (const auto& Model : Models)
	{
		AwaitModel(*Model);
	}
	Pixel(InFixture.Frame(40), 160, {1, 1, 1});
	Models.clear();
	InFixture.AwaitRetirement();
}

void AwaitBridge(FSceneRenderBridge& InBridge, FSceneHandle InHandle)
{
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (!InBridge.IsReady(InHandle) && InBridge.GetError(InHandle).empty() &&
	       std::chrono::steady_clock::now() < Deadline)
	{
		InBridge.Flush();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	HYP_CHECK(InBridge.IsReady(InHandle));
}

std::vector<FRenderItem> CollectItems(FSceneFixture& InFixture)
{
	std::vector<FRenderItem> Result;
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              FRenderView View;
		                                              View.CullingMode = ESceneCullingMode::None;
		                                              const auto Collected =
		                                                  InFixture.Session->GetScene().Collect(View);
		                                              Result.assign(Collected.Items.begin(), Collected.Items.end());
	                                              }));
	return Result;
}

std::shared_ptr<FMaterialInstance> SharedSurface(FSceneFixture& InFixture, std::shared_ptr<const FModelAsset> InAsset)
{
	FModel Warm(InFixture.Session->GetScene(), InFixture.Session->GetResources(), InAsset);
	AwaitModel(Warm);
	const auto Surface = Warm.GetResource()->GetMaterial(0);
	auto Instance = std::make_shared<FMaterialInstance>(Surface->GetCompiled()->Interface);
	for (const auto& Parameter : Surface->GetSnapshot()->Overrides)
	{
		Instance->Set(Parameter.Name, Parameter.Value);
	}
	return Instance;
}

void CheckMaterialAtomicity(FSceneFixture& InFixture, FScene& InScene, FSceneRenderBridge& InBridge,
                            FSceneHandle InFirst, FSceneHandle InSecond)
{
	const auto Before = CollectItems(InFixture);
	auto First = *InScene.Find(InFirst);
	First.World = Translation({-.5f, 0, 0});
	InScene.Update(InFirst, First);
	auto Second = *InScene.Find(InSecond);
	Second.Surface.Overrides = {{"Pbr.BaseColorFactor", FMaterialValue::Uint(1)}};
	InScene.Update(InSecond, Second);
	bool bRejected{};
	try
	{
		InBridge.Flush();
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected && InScene.GetChanges().size() == 2);
	const auto After = CollectItems(InFixture);
	HYP_CHECK(Before.size() == After.size());
	for (std::size_t Index = 0; Index < Before.size(); ++Index)
	{
		HYP_CHECK(Before[Index].State.Revision == After[Index].State.Revision);
		HYP_CHECK(Before[Index].State.World.Values == After[Index].State.World.Values);
	}
	Second.Surface.Overrides.clear();
	InScene.Update(InSecond, Second);
	InBridge.Flush();
	HYP_CHECK(InScene.GetChanges().empty());
	const auto Recovered = CollectItems(InFixture);
	HYP_CHECK(Recovered[0].State.Revision > Before[0].State.Revision);
	// A delayed result for the old publication must not overwrite the new revision.
	Before[0].Report({999999, 1, 1, Before[0].State.Revision, 1, "Forward", false, "old frame"});
	HYP_CHECK(InBridge.GetDrawResults(InFirst)[0].Error.empty());
}

void CheckSharedMaterialPublication(FSceneFixture& InFixture)
{
	auto Asset = std::make_shared<const FModelAsset>(Quad());
	auto Shared = SharedSurface(InFixture, Asset);
	FScene Scene;
	FSceneRenderBridge Bridge(Scene, *InFixture.Session, InFixture.Tasks);
	FSceneModel Left{"left", PrepareSceneModel(Asset)};
	Left.World = Multiply(Translation({-.7f, 0, 0}), Scale({.4f, .4f, 1}));
	Left.Surface.Instance = Shared;
	FSceneModel Right = Left;
	Right.Data = PrepareSceneModel(std::make_shared<const FModelAsset>(Quad()));
	Right.World = Multiply(Translation({.7f, 0, 0}), Scale({.4f, .4f, 1}));
	const auto A = Scene.Add(Left);
	const auto B = Scene.Add(Right);
	Bridge.Flush();
	AwaitBridge(Bridge, A);
	AwaitBridge(Bridge, B);
	InFixture.Frame(2);
	const auto Warm = InFixture.Session->GetResources().Statistics();
	Shared->SetSemantic("Pbr.BaseColorFactor", FMaterialValue::Float(FVec4{1, 0, 0, 1}));
	HYP_CHECK(Scene.GetChanges().empty()); // Material-only edits have no logical Scene revision.
	Bridge.Flush();
	AwaitBridge(Bridge, A);
	AwaitBridge(Bridge, B);
	{
		const auto Items = CollectItems(InFixture);
		HYP_CHECK(Items.size() == 2);
		HYP_CHECK(Items[0].State.Surface->GetSnapshot() == Items[1].State.Surface->GetSnapshot());
		HYP_CHECK(Items[0].State.Surface->GetSnapshot()->Revision == Shared->GetRevision());
	}
	const auto Image = InFixture.Frame(2);
	Pixel(Image, 110, {1, 0, 0});
	Pixel(Image, 210, {1, 0, 0});
	const auto Changed = InFixture.Session->GetResources().Statistics();
	HYP_CHECK(Changed.GeometryUploads == Warm.GeometryUploads);
	HYP_CHECK(Changed.Materials.PipelinesCreated == Warm.Materials.PipelinesCreated);
	HYP_CHECK(Changed.Materials.SetsCreated == Warm.Materials.SetsCreated);
	CheckMaterialAtomicity(InFixture, Scene, Bridge, A, B);
	Right.Material.BaseColor = FVec4{0, 0, 1, 1};
	Right.Surface.Overrides = {{"Pbr.BaseColorFactor", FMaterialValue::Float(FVec4{1, 0, 0, 1})}};
	Right.SectionSurfaces[0].Overrides = {{"Pbr.BaseColorFactor", FMaterialValue::Float(FVec4{0, 1, 0, 1})}};
	Scene.Update(B, Right);
	Bridge.Flush();
	Pixel(InFixture.Frame(2), 210, {0, 1, 0}); // section > object > legacy override > instance.
	Right.Surface = {};
	Right.SectionSurfaces.clear();
	Scene.Update(B, Right);
	Scene.Remove(A);
	Bridge.Flush();
	Shared->SetSemantic("Pbr.BaseColorFactor", FMaterialValue::Float(FVec4{1, 1, 0, 1}));
	Bridge.Flush();
	Pixel(InFixture.Frame(1), 210, {0, 0, 1}); // Removed subscriptions cannot affect the inherited material.
	Scene.Clear();
	Bridge.Flush();
	Bridge.Close();
	Shared.reset();
	Left = {};
	Right = {};
	InFixture.AwaitRetirement();
}

void CheckLogicalAttachment(FSceneFixture& InFixture)
{
	FScene Scene;
	FSceneRenderBridge Bridge(Scene, *InFixture.Session, InFixture.Tasks);
	bool bRejected = false;
	try
	{
		FSceneRenderBridge Duplicate(Scene, *InFixture.Session, InFixture.Tasks);
	}
	catch (const std::logic_error&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
	const auto Data = PrepareSceneModel(std::make_shared<const FModelAsset>(Quad()));
	const auto Before = InFixture.Session->GetResources().Statistics();
	const auto Stale = Scene.Add({"loading"});
	Scene.Remove(Stale);
	FSceneModel Model{"outside", Data};
	Model.World = Translation({100, 0, 0});
	const auto Handle = Scene.Add(Model);
	HYP_CHECK(!Scene.Update(Stale, Model));
	Bridge.Flush();
	AwaitBridge(Bridge, Handle);
	Pixel(InFixture.Frame(0), 160, {0, 0, 0}); // Initial state must never expose an identity frame.
	Model.World = Identity();
	Scene.Update(Handle, Model);
	const auto Shared = Scene.Add(Model);
	Bridge.Flush();
	AwaitBridge(Bridge, Shared);
	const auto BvhImage = InFixture.Frame(2);
	HYP_CHECK(BvhImage.Rgba == InFixture.Frame(2, ESceneCullingMode::Linear).Rgba);
	HYP_CHECK(BvhImage.Rgba == InFixture.Frame(2, ESceneCullingMode::None).Rgba);
	HYP_CHECK(InFixture.Session->GetResources().Statistics().GeometryUploads == Before.GeometryUploads + 1);
	Model.bVisible = false;
	Scene.Update(Handle, Model);
	Scene.Remove(Shared);
	Bridge.Flush();
	Pixel(InFixture.Frame(0), 160, {0, 0, 0});
	Bridge.Close();
	InFixture.AwaitRetirement();
	FSceneRenderBridge Reattached(Scene, *InFixture.Session, InFixture.Tasks);
	Reattached.Flush();
	AwaitBridge(Reattached, Handle);
	HYP_CHECK(Reattached.PrimitiveCount(Handle) == 1);
	Scene.Clear();
	Reattached.Flush();
	Reattached.Close();
	InFixture.AwaitRetirement();
	FSceneRenderBridge Pending(Scene, *InFixture.Session, InFixture.Tasks);
	const auto Loading = Scene.Add({"pending", PrepareSceneModel(std::make_shared<const FModelAsset>(Quad()))});
	Pending.Flush();
	Scene.Remove(Loading);
	Pending.Flush();
	Pending.Close();
	InFixture.AwaitRetirement(); // Removal and upload cleanup without another frame.
}

void CheckQueuedViews(FSceneFixture& InFixture)
{
	const auto First = InFixture.Session->FreezeFrame(0);
	const auto Second = InFixture.Session->FreezeFrame(0);
	std::array<FImage, 2> Images;
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch(
	    {EDomain::Render},
	    [&]
	    {
		    FRenderView View{
		        Multiply(Perspective(1, 4.f / 3, .1f, 10), LookAt({0, 0, 3}, {0, 0, 0})), {0, 0, 3}, 320, 240};
		    View.CullingMode = ESceneCullingMode::None;
		    std::array<FRenderGraph, 2> Graphs;
		    InFixture.Session->BuildViews(Graphs[0], std::span(&View, 1), InFixture.Session->FrameTargets(FVec4{}),
		                                  First, 1, false, true);
		    View.ViewProjection = Multiply(Translation({1.2f, 0, 0}), View.ViewProjection);
		    InFixture.Session->BuildViews(Graphs[1], std::span(&View, 1), InFixture.Session->FrameTargets(FVec4{}),
		                                  Second, 1, false, true);
		    Images[0] =
		        ExecuteGraph(std::move(Graphs[0]), InFixture.Tasks, *InFixture.Swapchain, {320, 240}, false, true);
		    Images[1] =
		        ExecuteGraph(std::move(Graphs[1]), InFixture.Tasks, *InFixture.Swapchain, {320, 240}, false, true);
		    InFixture.Session->CompleteViews();
	    }));
	Pixel(Images[0], 160, {1, 0, 0});
	Pixel(Images[1], 160, {0, 0, 0}); // The second preparation cannot overwrite the first graph's constants.
}

void CheckRetainedViewChanges(FSceneFixture& InFixture)
{
	InFixture.Frame(1, ESceneCullingMode::None);
	const auto Move = [](FRenderView& InView)
	{
		InView.Eye.X = .2f;
		InView.ViewProjection = Multiply(Perspective(1, 4.f / 3, .1f, 10), LookAt(InView.Eye, {0, 0, 0}));
	};
	InFixture.Frame(1, ESceneCullingMode::None, Move);
	HYP_CHECK(InFixture.LastStatistics.CollectionReuses == 1 && InFixture.LastStatistics.PreparationReuses == 0);
	InFixture.Frame(1, ESceneCullingMode::None, Move);
	HYP_CHECK(InFixture.LastStatistics.PreparationReuses == 1 && InFixture.LastStatistics.PacketReuses == 1);
	const auto Viewport = [](FRenderView& InView)
	{
		InView.Viewport = FViewport{0, 0, 320, 240, .1f, .8f};
	};
	InFixture.Frame(1, ESceneCullingMode::None, Viewport);
	HYP_CHECK(InFixture.LastStatistics.PacketReuses == 0);
	InFixture.Frame(1, ESceneCullingMode::None, Viewport);
	HYP_CHECK(InFixture.LastStatistics.PacketReuses == 1);
	InFixture.Frame(1, ESceneCullingMode::None,
	                [&](FRenderView& InView)
	                {
		                Viewport(InView);
		                InView.Viewport->MaxDepth = .9f;
	                });
	HYP_CHECK(InFixture.LastStatistics.PacketReuses == 0);
	InFixture.Frame(1);
	InFixture.Frame(1, ESceneCullingMode::Bvh,
	                [](FRenderView& InView)
	                {
		                InView.bInstanceBatching = false;
	                });
	HYP_CHECK(InFixture.LastStatistics.PreparationReuses == 0);
}

void CheckVisibilityReturn(FSceneFixture& InFixture)
{
	const auto Parameters = [](FRenderView& InView)
	{
		InView.Parameters = DefaultShadowParameters();
	};
	InFixture.Frame(1, ESceneCullingMode::Bvh, Parameters);
	const auto Original = InFixture.Frame(1, ESceneCullingMode::Bvh, Parameters);
	for (unsigned Index = 0; Index < 3; ++Index)
	{
		Pixel(InFixture.Frame(0, ESceneCullingMode::Bvh,
		                      [&](FRenderView& InView)
		                      {
			                      Parameters(InView);
			                      InView.ViewProjection = Multiply(Translation({10, 0, 0}), InView.ViewProjection);
		                      }),
		      160, {0, 0, 0});
		HYP_CHECK(InFixture.LastStatistics.RetainedSceneItems == 1);
		const auto Restored = InFixture.Frame(1, ESceneCullingMode::Bvh, Parameters);
		HYP_CHECK(Restored.Rgba == Original.Rgba);
		HYP_CHECK(InFixture.LastStatistics.RetainedItemRestores == 1);
	}
}

void CheckLocalPackets(FSceneFixture& InFixture)
{
	const auto Asset = std::make_shared<const FModelAsset>(Quad({1, 0, 0, 1}));
	FModel A(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Asset);
	FModel B(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Asset);
	A.SetTransform(Multiply(Translation({-.7f, 0, 0}), Scale({.4f, .4f, 1})));
	B.SetTransform(Multiply(Translation({.7f, 0, 0}), Scale({.4f, .4f, 1})));
	AwaitModel(A);
	AwaitModel(B);
	const auto Parameters = [](FRenderView& InView)
	{
		InView.Parameters = DefaultShadowParameters();
	};
	InFixture.Frame(2, ESceneCullingMode::None, Parameters);
	InFixture.Frame(2, ESceneCullingMode::None, Parameters);
	const auto Moving = [&](FRenderView& InView)
	{
		Parameters(InView);
		InView.Eye.X = .01f;
		InView.ViewProjection = Multiply(Perspective(1, 4.f / 3, .1f, 10), LookAt(InView.Eye, {0, 0, 0}));
	};
	const auto Batched = InFixture.Frame(2, ESceneCullingMode::None, Moving);
	HYP_CHECK(InFixture.LastStatistics.Batches.LocalPlanReuses == 1);
	HYP_CHECK(InFixture.LastStatistics.Batches.LocalPacketReuses == 1);
	HYP_CHECK(InFixture.LastStatistics.Batches.InstancedItems == 2 && InFixture.LastStatistics.Draws == 1);
	const auto Ordinary = InFixture.Frame(2, ESceneCullingMode::None,
	                                      [&](FRenderView& InView)
	                                      {
		                                      Moving(InView);
		                                      InView.bInstanceBatching = false;
	                                      });
	HYP_CHECK(Batched.Rgba == Ordinary.Rgba && InFixture.LastStatistics.Batches.LocalPacketReuses == 0);
	FMaterialOverride Blue;
	Blue.BaseColor = FVec4{0, 0, 1, 1};
	B.SetMaterial(Blue);
	Pixel(InFixture.Frame(2, ESceneCullingMode::None, Moving), 210, {0, 0, 1});
	HYP_CHECK(InFixture.LastStatistics.Batches.LocalPacketReuses == 0);
	A.Remove();
	B.Remove();
	InFixture.AwaitRetirement();
}

void CheckLocalVisibilityInputs(FSceneFixture& InFixture)
{
	const auto Asset = std::make_shared<const FModelAsset>(Quad({1, 0, 0, 1}));
	std::array<std::unique_ptr<FModel>, 3> Models;
	for (std::size_t Index = 0; Index < Models.size(); ++Index)
	{
		Models[Index] =
		    std::make_unique<FModel>(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Asset);
		Models[Index]->SetTransform(Multiply(Translation({(float(Index) - 1) * .8f, 0, 0}), Scale({.2f, .2f, 1})));
		AwaitModel(*Models[Index]);
	}
	const auto Full = [](FRenderView& InView)
	{
		InView.Parameters = DefaultShadowParameters();
		InView.CullingViewProjection = Identity();
	};
	const auto Partial = [&](FRenderView& InView)
	{
		Full(InView);
		InView.CullingViewProjection = Translation({-.5f, 0, 0});
	};
	InFixture.Frame(3, ESceneCullingMode::Bvh, Full);
	const auto Original = InFixture.Frame(3, ESceneCullingMode::Bvh, Full);
	for (unsigned Iteration = 0; Iteration < 4; ++Iteration)
	{
		InFixture.Frame(2, ESceneCullingMode::Bvh, Partial);
		HYP_CHECK(InFixture.LastStatistics.Batches.LocalInputReuses == 2);
		HYP_CHECK(InFixture.LastStatistics.SharedMaterialGroups == 1);
		HYP_CHECK(InFixture.LastStatistics.Batches.LocalCompatibilityReuses == 0);
		HYP_CHECK(InFixture.LastStatistics.Batches.IncrementalItemReuses == 2);
		HYP_CHECK(InFixture.LastStatistics.Batches.LocalRecordReuses > 0);
		HYP_CHECK(InFixture.LastStatistics.RetainedSceneItems == 1);
		const auto Restored = InFixture.Frame(3, ESceneCullingMode::Bvh, Full);
		HYP_CHECK(Restored.Rgba == Original.Rgba);
		HYP_CHECK(InFixture.LastStatistics.Batches.LocalInputReuses == 3);
		HYP_CHECK(InFixture.LastStatistics.Batches.LocalCompatibilityReuses == 1);
		HYP_CHECK(InFixture.LastStatistics.Batches.IncrementalItemReuses == 2);
		HYP_CHECK(InFixture.LastStatistics.Batches.LocalRecordReuses > 0);
		HYP_CHECK(InFixture.LastStatistics.Batches.PackedRecords == 0);
		HYP_CHECK(InFixture.LastStatistics.RetainedItemRestores == 1);
	}
	FMaterialOverride Blue;
	Blue.BaseColor = FVec4{0, 0, 1, 1};
	Models[1]->SetMaterial(Blue);
	Pixel(InFixture.Frame(3, ESceneCullingMode::Bvh, Full), 160, {0, 0, 1});
	HYP_CHECK(InFixture.LastStatistics.Batches.LocalInputReuses == 0);
	HYP_CHECK(InFixture.LastStatistics.Batches.LocalRecordReuses == 0);
	Models = {};
	InFixture.AwaitRetirement();
}

void CheckQueuedMembership(FSceneFixture& InFixture, std::size_t InCount,
                           const std::function<void(FRenderView&)>& InFull,
                           const std::function<void(FRenderView&)>& InPartial, const FImage& InExpectedFull,
                           const FImage& InExpectedPartial)
{
	const auto First = InFixture.Session->FreezeFrame(0);
	const auto Second = InFixture.Session->FreezeFrame(0);
	std::array<FImage, 2> Images;
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch(
	    {EDomain::Render},
	    [&]
	    {
		    FRenderView View{
		        Multiply(Perspective(1, 4.f / 3, .1f, 10), LookAt({0, 0, 3}, {0, 0, 0})), {0, 0, 3}, 320, 240};
		    std::array<FRenderGraph, 2> Graphs;
		    InFull(View);
		    HYP_CHECK(InFixture.Session->BuildViews(Graphs[0], std::span(&View, 1),
		                                            InFixture.Session->FrameTargets(FVec4{}), First, 1, false,
		                                            true) == InCount);
		    InPartial(View);
		    HYP_CHECK(InFixture.Session->BuildViews(Graphs[1], std::span(&View, 1),
		                                            InFixture.Session->FrameTargets(FVec4{}), Second, 1, false,
		                                            true) == InCount - 1);
		    Images[1] =
		        ExecuteGraph(std::move(Graphs[1]), InFixture.Tasks, *InFixture.Swapchain, {320, 240}, false, true);
		    Images[0] =
		        ExecuteGraph(std::move(Graphs[0]), InFixture.Tasks, *InFixture.Swapchain, {320, 240}, false, true);
		    InFixture.Session->CompleteViews();
	    }));
	HYP_CHECK(Images[0].Rgba == InExpectedFull.Rgba && Images[1].Rgba == InExpectedPartial.Rgba);
}

std::shared_ptr<const FRenderMaterial> RetirementMaterial(FSceneFixture& InFixture, const FRenderMaterial& InOriginal,
                                                          std::weak_ptr<const FMaterialTextureSource>& OutTexture)
{
	auto Description = InOriginal.GetSnapshot()->Definition->GetDescription();
	Description.Name = "Incremental retirement test";
	const auto Texture = std::make_shared<const FMaterialTextureSource>(
	    EMaterialTextureEncoding::Linear, std::vector<FMaterialTextureMip>{{1, 1, {100, 150, 200, 255}}});
	OutTexture = Texture;
	FMaterialParameterDeclaration Parameter;
	Parameter.Name = "Retirement.DefaultTexture";
	Parameter.Type = FMaterialParameterType::Resource(EMaterialValueKind::Texture2D);
	Parameter.bRequired = false;
	Parameter.bActive = false;
	Parameter.Default = FMaterialValue::FromTexture(Texture);
	Description.Parameters.push_back(std::move(Parameter));
	FMaterialInstance Instance(std::make_shared<const FMaterialDefinition>(std::move(Description)));
	for (const auto& Override : InOriginal.GetSnapshot()->Overrides)
	{
		Instance.Set(Override.Name, Override.Value);
	}
	auto Result = InFixture.Session->GetResources().RequestMaterial(Instance.Freeze());
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (Result->GetStatus() != ERenderMaterialStatus::Ready &&
	       Result->GetStatus() != ERenderMaterialStatus::Failed && std::chrono::steady_clock::now() < Deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	HYP_CHECK(Result->GetStatus() == ERenderMaterialStatus::Ready);
	return Result;
}

void CheckIncrementalRetirement(FSceneFixture& InFixture)
{
	auto& Scene = InFixture.Session->GetScene();
	auto& Batches = InFixture.Session->GetBatchSystem();
	FModel Template(Scene, InFixture.Session->GetResources(), std::make_shared<const FModelAsset>(Quad({1, 0, 0, 1})));
	AwaitModel(Template);
	auto Resource = Template.GetResource();
	Template.Remove();
	std::weak_ptr<const FMaterialTextureSource> Texture;
	auto Surface = RetirementMaterial(InFixture, *Resource->GetMaterial(0), Texture);
	const std::weak_ptr<const FCompiledMaterialDefinition> Program = Surface->GetCompiled();
	std::vector<FRenderPrimitiveState> States(2);
	for (auto& State : States)
	{
		State.Resource = Resource;
		State.Surface = Surface;
		State.World = Scale({.2f, .2f, 1});
	}
	auto Bindings = Scene.CreateBatch(States);
	InFixture.Tasks.Wait(Scene.Flush());
	const auto Configure = [](FRenderView& InView)
	{
		InView.Identity = 7101;
		InView.Parameters = DefaultShadowParameters();
	};
	const auto Original = InFixture.Frame(2, ESceneCullingMode::None, Configure);
	HYP_CHECK(InFixture.LastStatistics.Batches.AffectedBatches == 1);
	FRenderGraph Retained;
	const auto Frame = InFixture.Session->FreezeFrame(0);
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch(
	    {EDomain::Render},
	    [&]
	    {
		    FRenderView View{
		        Multiply(Perspective(1, 4.f / 3, .1f, 10), LookAt({0, 0, 3}, {0, 0, 0})), {0, 0, 3}, 320, 240};
		    Configure(View);
		    View.CullingMode = ESceneCullingMode::None;
		    HYP_CHECK(InFixture.Session->BuildViews(Retained, std::span(&View, 1),
		                                            InFixture.Session->FrameTargets(FVec4{}), Frame, 1, false,
		                                            true) == 2);
	    }));
	Bindings.clear();
	States.clear();
	Surface.reset();
	Resource.reset();
	InFixture.Tasks.Wait(Scene.Flush());
	InFixture.Frame(0, ESceneCullingMode::None,
	                [](FRenderView& InView)
	                {
		                InView.Identity = 7102;
	                });
	HYP_CHECK(!Texture.expired() && !Program.expired());
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              const auto Image =
		                                                  ExecuteGraph(std::move(Retained), InFixture.Tasks,
		                                                               *InFixture.Swapchain, {320, 240}, false, true);
		                                              HYP_CHECK(Image.Rgba == Original.Rgba);
		                                              // The old graph has now relinquished its valid ownership. Force
		                                              // the existing complete sweep through another view to isolate
		                                              // persistent history from candidate/input/chunk retirement.
		                                              FRenderSceneSnapshot Empty;
		                                              Empty.View.Identity = 7102;
		                                              Batches.Build(Empty);
	                                              }));
	InFixture.AwaitRetirement();
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while ((!Texture.expired() || !Program.expired()) && std::chrono::steady_clock::now() < Deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	HYP_CHECK(Texture.expired() && Program.expired());
}

void CheckIncrementalHistoryCapacity(FSceneFixture& InFixture, std::size_t InCount,
                                     const std::function<void(FRenderView&)>& InConfigure)
{
	const auto Reference = InFixture.Frame(InCount, ESceneCullingMode::Bvh, InConfigure);
	for (std::uint64_t ViewId = 100; ViewId < 124; ++ViewId)
	{
		const auto Current = InFixture.Frame(InCount, ESceneCullingMode::Bvh,
		                                     [&](FRenderView& InView)
		                                     {
			                                     InConfigure(InView);
			                                     InView.Identity = ViewId;
		                                     });
		HYP_CHECK(Current.Rgba == Reference.Rgba);
		HYP_CHECK(InFixture.LastStatistics.Batches.CachedPlanItems <= 4096);
		HYP_CHECK(InFixture.LastStatistics.Batches.CachedPlanBlocks <= 512);
		HYP_CHECK(InFixture.LastStatistics.Batches.CachedBytes <= 16 * 1024 * 1024);
	}
	const auto Revisited = InFixture.Frame(InCount, ESceneCullingMode::Bvh,
	                                       [&](FRenderView& InView)
	                                       {
		                                       InConfigure(InView);
		                                       ++InView.Revision; // Re-enter planning after history eviction.
	                                       });
	HYP_CHECK(Revisited.Rgba == Reference.Rgba);
	HYP_CHECK(InFixture.LastStatistics.Batches.AffectedBatches == 3);
}

void CheckIncrementalBlocks(FSceneFixture& InFixture)
{
	auto& Scene = InFixture.Session->GetScene();
	FModel Template(Scene, InFixture.Session->GetResources(), std::make_shared<const FModelAsset>(Quad({1, 0, 0, 1})));
	AwaitModel(Template);
	auto Resource = Template.GetResource();
	const auto* Pass = Resource->GetMaterial(0)->GetCompiled()->FindInstancePass("Forward");
	HYP_CHECK(Pass && Pass->InstanceCapacity >= 2);
	const auto Capacity = Pass->InstanceCapacity;
	const auto Count = std::size_t(Capacity) * 3;
	Template.Remove();
	FRenderPrimitiveState State;
	State.Resource = Resource;
	std::vector<FRenderPrimitiveState> States(Count, State);
	const auto Center = [](std::size_t InIndex)
	{
		return Multiply(Translation({(float(InIndex % 16) - 8) * .03f, (float(InIndex / 16) - 8) * .02f, .5f}),
		                Scale({.025f, .025f, 1}));
	};
	for (std::size_t Index = 0; Index < Count; ++Index)
	{
		States[Index].World = Center(Index);
	}
	auto Bindings = Scene.CreateBatch(States);
	InFixture.Tasks.Wait(Scene.Flush());
	const auto Full = [](FRenderView& InView)
	{
		InView.Parameters = DefaultShadowParameters();
		InView.CullingViewProjection = Identity();
	};
	const auto Partial = [&](FRenderView& InView)
	{
		Full(InView);
		InView.CullingViewProjection = Translation({.15f, 0, 0});
	};
	std::size_t Previous{};
	for (const auto Selected : {std::size_t(0), Count / 2, Count - 1})
	{
		States[Previous].World = Center(Previous);
		++States[Previous].Revision;
		InFixture.Tasks.Wait(Scene.Update({{Bindings[Previous].GetHandle(), States[Previous]}}));
		States[Selected].World = Multiply(Translation({.98f, 0, .5f}), Scale({.025f, .025f, 1}));
		++States[Selected].Revision;
		InFixture.Tasks.Wait(Scene.Update({{Bindings[Selected].GetHandle(), States[Selected]}}));
		InFixture.Frame(Count, ESceneCullingMode::Bvh, Full);
		const auto Original = InFixture.Frame(Count, ESceneCullingMode::Bvh, Full);
		HYP_CHECK(InFixture.LastStatistics.Batches.InstancedDraws == 3);
		HYP_CHECK(InFixture.LastStatistics.Batches.CapacitySplits == 2);
		const auto Culled = InFixture.Frame(Count - 1, ESceneCullingMode::Bvh, Partial);
		const auto& Stats = InFixture.LastStatistics;
		HYP_CHECK(Stats.MembershipRemoved == 1 && Stats.MembershipAdded == 0);
		std::cout << "Incremental member " << Selected << "/" << Count
		          << ": updates=" << Stats.Batches.IncrementalPlanUpdates
		          << " retained items=" << Stats.Batches.IncrementalItemReuses
		          << " affected blocks=" << Stats.Batches.AffectedBatches
		          << " retained blocks=" << Stats.Batches.RetainedBatches
		          << " input builds=" << Stats.Batches.PreparedInputBuilds
		          << " compatibility builds=" << Stats.Batches.CompatibilityBuilds << '\n';
		HYP_CHECK(Stats.Batches.IncrementalPlanUpdates == 1 && Stats.Batches.IncrementalItemReuses == Count - 1);
		HYP_CHECK(Stats.Batches.AffectedBatches == 1 && Stats.Batches.RetainedBatches == 2);
		HYP_CHECK(Stats.Batches.RebuiltChunks == 1 && Stats.Batches.ReusedChunks == 2);
		HYP_CHECK(Stats.Batches.CapacitySplits == 2);
		HYP_CHECK(Stats.Batches.BatchAdmissionReuses == 2 && Stats.Batches.PackedRecords == 0);
		const auto Restored = InFixture.Frame(Count, ESceneCullingMode::Bvh, Full);
		HYP_CHECK(Restored.Rgba == Original.Rgba);
		HYP_CHECK(InFixture.LastStatistics.Batches.AffectedBatches == 1);
		HYP_CHECK(InFixture.LastStatistics.Batches.RetainedBatches == 2);
		HYP_CHECK(InFixture.LastStatistics.Batches.CapacitySplits == 2);
		CheckQueuedMembership(InFixture, Count, Full, Partial, Original, Culled);
		const auto Ordinary = InFixture.Frame(Count - 1, ESceneCullingMode::Bvh,
		                                      [&](FRenderView& InView)
		                                      {
			                                      Partial(InView);
			                                      InView.bInstanceBatching = false;
		                                      });
		HYP_CHECK(Ordinary.Rgba == Culled.Rgba);
		Previous = Selected;
	}
	CheckIncrementalHistoryCapacity(InFixture, Count, Full);
	for (std::size_t Block = 0; Block < 3; ++Block)
	{
		for (std::size_t Index = Block * Capacity; Index < (Block + 1) * Capacity; ++Index)
		{
			Bindings[Index].Remove();
		}
		InFixture.Tasks.Wait(Scene.Flush());
		const auto Remaining = Count - (Block + 1) * Capacity;
		InFixture.Frame(Remaining, ESceneCullingMode::Bvh, Full);
		HYP_CHECK(InFixture.LastStatistics.Batches.CapacitySplits == (Remaining ? Remaining / Capacity - 1 : 0));
	}
	Bindings.clear();
	States.clear();
	State = {};
	Resource.reset();
	InFixture.AwaitRetirement();
}

std::shared_ptr<const FRenderMaterial> SharedOverrideMaterial(FSceneFixture& InFixture,
                                                              const FRenderMaterial& InOriginal)
{
	auto Description = InOriginal.GetSnapshot()->Definition->GetDescription();
	Description.Name = "Shared split test";
	Description.Parameters = InOriginal.GetCompiled()->Interface.Schema->GetParameters();
	for (auto& Parameter : Description.Parameters)
	{
		if (Parameter.Name == "Engine.View.ViewProjection")
		{
			Parameter.OverridePolicy = EMaterialOverridePolicy::AllowOverride;
			Parameter.OverrideScopes = MaterialScopeBit(EMaterialScope::Object);
		}
	}
	FMaterialInstance Instance(std::make_shared<const FMaterialDefinition>(std::move(Description)));
	for (const auto& Override : InOriginal.GetSnapshot()->Overrides)
	{
		Instance.Set(Override.Name, Override.Value);
	}
	auto Result = InFixture.Session->GetResources().RequestMaterial(Instance.Freeze());
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (Result->GetStatus() != ERenderMaterialStatus::Ready &&
	       Result->GetStatus() != ERenderMaterialStatus::Failed && std::chrono::steady_clock::now() < Deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if (!Result->GetError().empty())
	{
		throw std::runtime_error(Result->GetError());
	}
	HYP_CHECK(Result->GetStatus() == ERenderMaterialStatus::Ready);
	return Result;
}

void CheckIncrementalSharedSplit(FSceneFixture& InFixture)
{
	auto& Scene = InFixture.Session->GetScene();
	FModel Template(Scene, InFixture.Session->GetResources(), std::make_shared<const FModelAsset>(Quad({1, 0, 0, 1})));
	AwaitModel(Template);
	auto Resource = Template.GetResource();
	Template.Remove();
	auto Surface = SharedOverrideMaterial(InFixture, *Resource->GetMaterial(0));
	const auto InitialView = Multiply(Perspective(1, 4.f / 3, .1f, 10), LookAt({0, 0, 3}, {0, 0, 0}));
	std::vector<FRenderPrimitiveState> States(2);
	for (std::size_t Index = 0; Index < States.size(); ++Index)
	{
		States[Index].Resource = Resource;
		States[Index].Surface = Surface;
		States[Index].World = Multiply(Translation({Index ? .7f : -.7f, 0, 0}), Scale({.25f, .25f, 1}));
	}
	States[0].ObjectParameters = {{"Engine.View.ViewProjection", FMaterialValue::Matrix(InitialView)}};
	auto Bindings = Scene.CreateBatch(States);
	InFixture.Tasks.Wait(Scene.Flush());
	for (const auto& Binding : Bindings)
	{
		const auto Status = Binding.GetStatus();
		if (!Status.Error.empty())
		{
			throw std::runtime_error(Status.Error);
		}
		HYP_CHECK(Status.State == ERenderPrimitiveStatus::Ready);
	}
	const auto Initial = [](FRenderView& InView)
	{
		InView.Parameters = DefaultShadowParameters();
	};
	const auto Changed = [&](FRenderView& InView)
	{
		Initial(InView);
		InView.ViewProjection = Multiply(Translation({.2f, 0, 0}), InitialView);
	};
	InFixture.Frame(2, ESceneCullingMode::None, Initial);
	const auto Original = InFixture.Frame(2, ESceneCullingMode::None, Initial);
	HYP_CHECK(InFixture.LastStatistics.Batches.InstancedDraws == 1);
	// These sources share equal numeric view values initially, but only one follows the next camera.
	const auto Split = InFixture.Frame(2, ESceneCullingMode::None, Changed);
	HYP_CHECK(InFixture.LastStatistics.Batches.InstancedDraws == 0);
	HYP_CHECK(InFixture.LastStatistics.Batches.IncrementalItemReuses == 0);
	HYP_CHECK(Split.Rgba != Original.Rgba);
	const auto Ordinary = InFixture.Frame(2, ESceneCullingMode::None,
	                                      [&](FRenderView& InView)
	                                      {
		                                      Changed(InView);
		                                      InView.bInstanceBatching = false;
	                                      });
	HYP_CHECK(Ordinary.Rgba == Split.Rgba);
	const auto Restored = InFixture.Frame(2, ESceneCullingMode::None, Initial);
	HYP_CHECK(Restored.Rgba == Original.Rgba && InFixture.LastStatistics.Batches.InstancedDraws == 1);
	Bindings.clear();
	States = {};
	Resource.reset();
	Surface.reset();
	InFixture.AwaitRetirement();
}

void CheckRetainedFrames(FSceneFixture& InFixture)
{
	FScene Scene;
	FSceneRenderBridge Bridge(Scene, *InFixture.Session, InFixture.Tasks);
	FSceneModel Model{"retained", PrepareSceneModel(std::make_shared<const FModelAsset>(Quad({1, 0, 0, 1})))};
	const auto Handle = Scene.Add(Model);
	Bridge.Flush();
	AwaitBridge(Bridge, Handle);
	for (unsigned Index = 0; Index < 5; ++Index)
	{
		InFixture.Frame(1);
	}
	Bridge.Flush();
	const auto StatusRevision = Bridge.GetStatusRevision();
	const auto Warm = InFixture.Session->GetResources().Statistics();
	for (unsigned Index = 0; Index < 6; ++Index)
	{
		Bridge.Flush();
		Pixel(InFixture.Frame(1), 160, {1, 0, 0});
		HYP_CHECK(InFixture.LastStatistics.CollectionReuses == 1);
		HYP_CHECK(InFixture.LastStatistics.PreparationReuses == 1 && InFixture.LastStatistics.PacketReuses == 1);
		HYP_CHECK(InFixture.LastStatistics.VisitedNodes == 0 && InFixture.LastStatistics.GroupTests == 0);
		const auto Results = Bridge.GetDrawResults(Handle);
		HYP_CHECK(Results.size() == 1 && Results[0].bReady && Results[0].Frame == InFixture.FrameNumber);
		const auto Unused = InFixture.Session->FreezeFrame(0); // Unconsumed scope destruction must remain idle.
	}
	const auto Idle = InFixture.Session->GetResources().Statistics();
	HYP_CHECK(Idle.MaintenanceTasks == Warm.MaintenanceTasks && Idle.MaintenanceTicks == Warm.MaintenanceTicks);
	HYP_CHECK(Idle.Constants.UploadBytes == Warm.Constants.UploadBytes && Bridge.GetStatusRevision() == StatusRevision);
	CheckRetainedViewChanges(InFixture);
	CheckVisibilityReturn(InFixture);
	CheckQueuedViews(InFixture);
	Model.World = Translation({100, 0, 0});
	Scene.Update(Handle, Model);
	Bridge.Flush();
	Pixel(InFixture.Frame(0), 160, {0, 0, 0});
	HYP_CHECK(InFixture.LastStatistics.CollectionReuses == 0);
	Scene.Clear();
	Bridge.Flush();
	Bridge.Close();
	InFixture.AwaitRetirement();
}

void CheckReceiptViewChanges(FSceneFixture& InFixture)
{
	FModel Model(InFixture.Session->GetScene(), InFixture.Session->GetResources(),
	             std::make_shared<const FModelAsset>(Quad({1, 0, 0, 1})));
	AwaitModel(Model);
	const auto Run = [&](std::vector<std::uint64_t> InIds, bool bInExpectReuse)
	{
		const auto Frame = InFixture.Session->FreezeFrame(0);
		InFixture.Tasks.Wait(InFixture.Tasks.Dispatch(
		    {EDomain::Render},
		    [&]
		    {
			    std::vector<FRenderView> Views;
			    for (const auto Id : InIds)
			    {
				    FRenderView View{
				        Multiply(Perspective(1, 4.f / 3, .1f, 10), LookAt({0, 0, 3}, {0, 0, 0})), {0, 0, 3}, 320, 240};
				    View.Identity = Id;
				    View.CullingMode = ESceneCullingMode::None;
				    Views.push_back(View);
			    }
			    FRenderGraph Graph;
			    InFixture.Session->BuildViews(Graph, Views, InFixture.Session->FrameTargets(FVec4{}), Frame, 1, false,
			                                  true);
			    ExecuteGraph(std::move(Graph), InFixture.Tasks, *InFixture.Swapchain, {320, 240}, false, false);
			    InFixture.Session->CompleteViews();
			    for (const auto& Stats : InFixture.Session->ViewStatistics())
			    {
				    HYP_CHECK(Stats.Visibility.PacketReuses == unsigned(bInExpectReuse));
			    }
		    }));
		const auto Receipt = Model.GetDrawResults().at(0);
		HYP_CHECK(Receipt.Frame == Frame->Frame && Receipt.View == InIds.back() && Receipt.Family == 1);
		HYP_CHECK(Receipt.bReady && Receipt.Error.empty() && Receipt.Usage == "Forward");
	};
	Run({11, 22}, false);
	Run({11, 22}, true);
	Run({22, 11}, true);
	Run({11}, true);
	Run({11}, true);
	Model.Remove();
	InFixture.AwaitRetirement();
}
} // namespace

int main()
{
	try
	{
		FSceneFixture Fixture;
		CheckSharedModels(Fixture);
		CheckSceneDepth(Fixture);
		CheckGlobalBlend(Fixture);
		CheckManyPrimitives(Fixture);
		CheckLogicalAttachment(Fixture);
		CheckSharedMaterialPublication(Fixture);
		CheckRetainedFrames(Fixture);
		CheckReceiptViewChanges(Fixture);
		CheckLocalPackets(Fixture);
		CheckLocalVisibilityInputs(Fixture);
		CheckIncrementalRetirement(Fixture);
		CheckIncrementalBlocks(Fixture);
		CheckIncrementalSharedSplit(Fixture);
		Fixture.Tasks.Wait(Fixture.Tasks.Dispatch({EDomain::Rhi, 0},
		                                          [&]
		                                          {
			                                          HYP_CHECK(Fixture.Device->Statistics().ValidationErrors == 0);
		                                          }));
		std::cout << "Shared models, instance isolation, scene depth, interleaved blend and 40 primitives passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
