#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/Model.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/SceneBridge.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <cmath>
#include <iostream>
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

void Pixel(const FImage& InImage, unsigned InX, FVec3 InExpected)
{
	const auto Offset = (std::size_t(120) * InImage.Width + InX) * 4;
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
			    FColorPass Clear;
			    Clear.Load = EColorLoad::Clear;
			    Clear.Commands.Name = "Clear";
			    Graph.Add(Clear);
			    FRenderView View{
			        Multiply(Perspective(1, 4.f / 3, .1f, 10), LookAt({0, 0, 3}, {0, 0, 0})), {0, 0, 3}, 320, 240};
			    View.CullingMode = InMode;
			    if (InConfigure)
			    {
				    InConfigure(View);
			    }
			    HYP_CHECK(Session->BuildViews(Graph, std::span(&View, 1), MaterialFrame, 1, false, true) ==
			              InExpectedItems);
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
		                                              Result = InFixture.Session->GetScene().Collect(View).Items;
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
		    View.ClearColor = FVec4{};
		    std::array<FRenderGraph, 2> Graphs;
		    InFixture.Session->BuildViews(Graphs[0], std::span(&View, 1), First, 1, false, true);
		    View.ViewProjection = Multiply(Translation({1.2f, 0, 0}), View.ViewProjection);
		    InFixture.Session->BuildViews(Graphs[1], std::span(&View, 1), Second, 1, false, true);
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
				    View.ClearColor = FVec4{};
				    Views.push_back(View);
			    }
			    FRenderGraph Graph;
			    InFixture.Session->BuildViews(Graph, Views, Frame, 1, false, true);
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
