#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/Model.h"
#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

using namespace Hyperion;
void RunColorTargetTests(IRHIDevice& InDevice, IRHISwapchain& InSwapchain);
void RunFullscreenTests(FTaskSystem& InTasks, IRHIDevice& InDevice, IRHISwapchain& InSwapchain);

namespace
{
void Rejects(const std::function<void()>& InAction)
{
	try
	{
		InAction();
	}
	catch (const std::invalid_argument&)
	{
		return;
	}
	throw std::runtime_error("Invalid deferred configuration accepted");
}

std::shared_ptr<const FModelAsset> Quad(FModelMaterial InMaterial, bool bInNormalMap = false)
{
	FModelAsset Asset;
	FModelPrimitive Primitive;
	Primitive.Positions = {-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0};
	Primitive.Normals = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
	Primitive.Tangents = {1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1};
	Primitive.TexCoords0 = {0, 1, 1, 1, 1, 0, 0, 0};
	Primitive.TexCoords1 = Primitive.TexCoords0;
	Primitive.Indices = {0, 1, 2, 0, 2, 3};
	Primitive.Material = 0;
	Asset.Primitives.push_back(Primitive);
	if (InMaterial.AlphaMode == EAlphaMode::Mask)
	{
		FModelImage Image{"Coverage", 2, 1};
		Image.Rgba = {255, 255, 255, 255, 255, 255, 255, 0};
		Asset.Images.push_back(Image);
		InMaterial.BaseColorTexture = {0, -1, 1};
	}
	if (bInNormalMap)
	{
		FModelImage Image{"Normal", 1, 1};
		Image.Rgba = {170, 140, 245, 255};
		InMaterial.NormalTexture = {static_cast<int>(Asset.Images.size()), -1, 0};
		Asset.Images.push_back(Image);
	}
	Asset.Materials.push_back(InMaterial);
	FModelNode Node;
	Node.Primitives = {0};
	Asset.Nodes.push_back(Node);
	Asset.Roots = {0};
	return std::make_shared<const FModelAsset>(std::move(Asset));
}

void Await(const FModel& InModel)
{
	const auto End = std::chrono::steady_clock::now() + std::chrono::seconds(20);
	while (!InModel.IsReady() && InModel.GetError().empty() && std::chrono::steady_clock::now() < End)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if (!InModel.GetError().empty())
	{
		throw std::runtime_error(InModel.GetError());
	}
	HYP_CHECK(InModel.IsReady());
}

struct FFixture
{
	FTaskSystem Tasks{2, 2};
	FWindow Window{"Deferred regression", {384, 288}, true};
	FShaderCompiler Compiler{std::filesystem::path(HYP_SOURCE_DIR) / "shaders", "deferred-render-cache"};
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::unique_ptr<FRenderSession> Session;
	std::unique_ptr<FSceneRenderPipeline> Pipeline;
	FRenderView View;
	FCascadedShadowSettings Shadows;
	FForwardPipelineStatistics Statistics;
	FDeviceStats DeviceStats;
	FScenePipelineSettings Settings;
	FVec4 Clear{.025f, .035f, .065f, 1};

	FFixture()
	{
		const auto Surface = Window.Surface();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          FRHIBackendRegistry Registry;
			                          RegisterD3D12RHIBackend(Registry);
			                          Device = Registry.CreateDevice(ERHIBackend::D3D12);
			                          Swapchain = Device->CreateSwapchain({Surface, {384, 288}, ERHIDepthFormat::D32});
			                          Swapchain->SetGpuTimingEnabled(true);
			                          RunColorTargetTests(*Device, *Swapchain);
		                          }));
		Session = std::make_unique<FRenderSession>(Tasks, *Device, Compiler);
		Pipeline = std::make_unique<FSceneRenderPipeline>(*Session, Device->GetCapabilities());
		View.Eye = {0, 0, 5};
		View.Width = 384;
		View.Height = 288;
		View.Camera = FRenderCamera{{0, 0, -1}, {0, 1, 0}, 1, .1f, 40};
		View.ViewProjection = Multiply(Perspective(1, 4.f / 3, .1f, 40), LookAt(View.Eye, {}));
		Shadows.Resolution = 1024;
		Shadows.Distance = 18;
		Shadows.bEnabled = false;
	}

	~FFixture()
	{
		Pipeline.reset();
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

	FImage Frame(ESceneRenderPipeline InPipeline = ESceneRenderPipeline::Deferred, bool bInCapture = true)
	{
		Window.Poll();
		Tasks.Wait(Session->GetScene().Flush());
		Settings.Pipeline = InPipeline;
		const auto Frame = Session->FreezeFrame();
		FImage Result;
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          Pipeline->Configure(Settings);
			                          FRenderGraph Graph;
			                          Pipeline->Build(Graph, View, Frame, Shadows, Clear, {}, true);
			                          const auto Prepared = Pipeline->GetFrame();
			                          Result = ExecuteGraph(std::move(Graph), Tasks, *Swapchain,
			                                                {View.Width, View.Height}, false, bInCapture);
			                          Statistics = Prepared.Statistics();
			                          for (const auto& Row : Statistics.Views)
			                          {
				                          HYP_CHECK(Row.Visibility.Batches.FailedItems == 0);
			                          }
			                          Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
			                                                    [&]
			                                                    {
				                                                    Device->CollectCompletedResources();
				                                                    DeviceStats = Device->Statistics();
				                                                    HYP_CHECK(DeviceStats.ValidationErrors == 0);
			                                                    }));
		                          }));
		return Result;
	}
};

float Pixel(const FImage& InImage, unsigned InX, unsigned InY, unsigned InChannel = 0)
{
	return InImage.Rgba.at((std::size_t(InY) * InImage.Width + InX) * 4 + InChannel);
}

void Similar(const FImage& InA, const FImage& InB, float InTolerance = .025f)
{
	HYP_CHECK(InA.Rgba.size() == InB.Rgba.size());
	double Mean{};
	float Maximum{};
	for (std::size_t Index = 0; Index < InA.Rgba.size(); ++Index)
	{
		const float Delta = std::abs(InA.Rgba[Index] - InB.Rgba[Index]);
		Mean += Delta;
		Maximum = std::max(Maximum, Delta);
	}
	std::cout << "image mean=" << Mean / InA.Rgba.size() << " max=" << Maximum << '\n';
	HYP_CHECK(Mean / InA.Rgba.size() < .002);
	HYP_CHECK(Maximum < InTolerance);
}

void CheckConfiguration(FFixture& InFixture)
{
	const auto Caps = InFixture.Device->GetCapabilities();
	FGBufferLayout{}.Validate(Caps);
	HYP_CHECK(FGBufferLayout{}.BytesPerPixel() == 24);
	HYP_CHECK(FGBufferLayout::HighPrecision().BytesPerPixel() == 32);
	auto Invalid = FGBufferLayout{};
	Invalid.Formats[3] = EMaterialColorFormat::Rgba8Unorm;
	Rejects(
	    [&]
	    {
		    Invalid.Validate(Caps);
	    });
	auto LowCaps = Caps;
	LowCaps.MaxColorTargets = 3;
	Rejects(
	    [&]
	    {
		    FGBufferLayout{}.Validate(LowCaps);
	    });
	auto Singular = FMat4{};
	Rejects(
	    [&]
	    {
		    Inverse(Singular);
	    });
	const auto IdentityCheck = Multiply(Inverse(InFixture.View.ViewProjection), InFixture.View.ViewProjection);
	for (std::size_t Index = 0; Index < 16; ++Index)
	{
		HYP_CHECK(std::abs(IdentityCheck.Values[Index] - Identity().Values[Index]) < .0001f);
	}
}

void CheckHdrAndRoutes(FFixture& InFixture)
{
	FModelMaterial Material;
	Material.BaseColor = {0, 0, 0, 1};
	Material.Metallic = 0;
	Material.Emissive = {4, 2, 1};
	FModel Surface(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material));
	Await(Surface);
	const auto Deferred = InFixture.Frame();
	const auto Forward = InFixture.Frame(ESceneRenderPipeline::Forward);
	Similar(Deferred, Forward, .008f);
	// 4.0 HDR -> Reinhard .8 -> sRGB .906; catches premature clamping and double encoding.
	HYP_CHECK(std::abs(Pixel(Deferred, 192, 144) - .9063f) < .008f);
	HYP_CHECK(std::abs(Pixel(Deferred, 0, 0) - InFixture.Clear.X) < .004f);
	Material.bUnlit = true;
	Material.BaseColor = {.2f, .4f, .8f, .5f};
	Material.AlphaMode = EAlphaMode::Blend;
	FModel Transparent(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material));
	Transparent.SetTransform(Translation({0, 0, 1}));
	Await(Transparent);
	const auto Blended = InFixture.Frame();
	Similar(Blended, InFixture.Frame(ESceneRenderPipeline::Forward), .008f);
	HYP_CHECK(InFixture.Statistics.MainView().VisibleItems == 2);
	// Blend in HDR: (4*.5 + .2*.5) -> tone -> sRGB, rather than blending tonemapped values.
	HYP_CHECK(std::abs(Pixel(Blended, 192, 144) - .842f) < .012f);
	Transparent.Remove();
	Surface.Remove();
	Material.AlphaMode = EAlphaMode::Opaque;
	FModel Unlit(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material));
	Await(Unlit);
	Similar(InFixture.Frame(), InFixture.Frame(ESceneRenderPipeline::Forward), .008f);
	HYP_CHECK(InFixture.Statistics.MainView().VisibleItems == 1);
	Unlit.Remove();
}

void CheckCoverageAndInstances(FFixture& InFixture)
{
	FModelMaterial Material;
	Material.BaseColor = {.4f, .2f, .1f, 1};
	Material.Metallic = .6f;
	Material.Roughness = .25f;
	Material.bDoubleSided = true;
	Material.AlphaMode = EAlphaMode::Mask;
	const auto Asset = Quad(Material, true);
	FModel A(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Asset);
	FModel B(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Asset);
	A.SetTransform(Multiply(Translation({-.9f, 0, 0}), Scale({.7f, .7f, 1})));
	B.SetTransform(Multiply(Translation({.9f, 0, 0}), Scale({.7f, .7f, 1})));
	Await(A);
	Await(B);
	const auto Deferred = InFixture.Frame();
	HYP_CHECK(InFixture.Statistics.MainView().VisibleItems == 2);
	HYP_CHECK(InFixture.Statistics.MainView().Batches.InstancedItems == 2);
	Similar(Deferred, InFixture.Frame(ESceneRenderPipeline::Forward));
	InFixture.View.bInstanceBatching = false;
	Similar(Deferred, InFixture.Frame(), .008f);
	InFixture.View.bInstanceBatching = true;
	InFixture.Settings.GBuffer = FGBufferLayout::HighPrecision();
	Similar(Deferred, InFixture.Frame());
	InFixture.Settings.GBuffer.Formats[1] = EMaterialColorFormat::Rgba32Float;
	Similar(Deferred, InFixture.Frame());
	InFixture.Settings.GBuffer = {};
	InFixture.View.Viewport = FViewport{32, 24, 320, 240};
	Similar(InFixture.Frame(), InFixture.Frame(ESceneRenderPipeline::Forward));
	InFixture.View.Viewport.reset();
	B.SetTransform(Multiply(Translation({.9f, 0, 0}), Scale({-.7f, .7f, 1})));
	Similar(InFixture.Frame(), InFixture.Frame(ESceneRenderPipeline::Forward));
	A.Remove();
	B.Remove();
}

void CheckShadowContinuity(FFixture& InFixture)
{
	FModelMaterial Material;
	Material.BaseColor = {.6f, .6f, .6f, 1};
	Material.Metallic = 0;
	Material.Roughness = 1;
	FModel Receiver(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material, true));
	Receiver.SetTransform(Scale({4, 4, 1}));
	Material.AlphaMode = EAlphaMode::Mask;
	FModel Caster(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material));
	Caster.SetTransform(Multiply(Translation({0, 0, 1}), Scale({.5f, .5f, 1})));
	Await(Receiver);
	Await(Caster);
	InFixture.Session->SetSceneParameters(
	    {{"Engine.Scene.MainDirectionalLightDirection", FMaterialValue::Float(Normalize(FVec3{1, 0, 1}))},
	     {"Engine.Scene.MainDirectionalLightColor", FMaterialValue::Float(FVec3{3, 3, 3})},
	     {"Engine.Scene.AmbientColor", FMaterialValue::Float(FVec3{.2f, .2f, .2f})}});
	const auto Unshadowed = InFixture.Frame();
	InFixture.Shadows.bEnabled = true;
	const auto Shadowed = InFixture.Frame();
	float MaximumShadow{};
	for (std::size_t Index = 0; Index < Shadowed.Rgba.size(); Index += 4)
	{
		MaximumShadow = std::max(MaximumShadow, Unshadowed.Rgba[Index] - Shadowed.Rgba[Index]);
	}
	HYP_CHECK(MaximumShadow > .1f);
	Similar(Shadowed, InFixture.Frame(ESceneRenderPipeline::Forward), .065f);
	InFixture.View.Eye.X = .015f;
	InFixture.View.ViewProjection =
	    Multiply(Perspective(1, 4.f / 3, .1f, 40), LookAt(InFixture.View.Eye, {.015f, 0, 0}));
	Similar(InFixture.Frame(), InFixture.Frame(ESceneRenderPipeline::Forward), .065f);
	Receiver.Remove();
	Caster.Remove();
	InFixture.Shadows.bEnabled = false;
}

void CheckQueuedGenerations(FFixture& InFixture)
{
	FModelMaterial Material;
	Material.BaseColor = {0, 0, 0, 1};
	Material.Emissive = {.5f, .25f, .1f};
	FModel Model(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material));
	Await(Model);
	const auto Reference = InFixture.Frame();
	const auto FirstFrame = InFixture.Session->FreezeFrame();
	const auto SecondFrame = InFixture.Session->FreezeFrame();
	FImage FirstImage;
	FImage SecondImage;
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch(
	    {EDomain::Render},
	    [&]
	    {
		    FRenderGraph First;
		    InFixture.Pipeline->Build(First, InFixture.View, FirstFrame, InFixture.Shadows, InFixture.Clear, {}, true);
		    const auto FirstPrepared = InFixture.Pipeline->GetFrame();
		    auto Changed = InFixture.Settings;
		    Changed.Pipeline = ESceneRenderPipeline::Forward;
		    Changed.GBuffer = FGBufferLayout::HighPrecision();
		    Changed.Exposure = 2;
		    InFixture.Pipeline->Configure(Changed);
		    FRenderGraph Second;
		    InFixture.Pipeline->Build(Second, InFixture.View, SecondFrame, InFixture.Shadows, InFixture.Clear, {},
		                              true);
		    const auto SecondPrepared = InFixture.Pipeline->GetFrame();
		    FirstImage = ExecuteGraph(std::move(First), InFixture.Tasks, *InFixture.Swapchain, {384, 288}, false, true);
		    SecondImage =
		        ExecuteGraph(std::move(Second), InFixture.Tasks, *InFixture.Swapchain, {384, 288}, false, true);
		    HYP_CHECK(FirstPrepared.Statistics().FullscreenDraws == 2);
		    HYP_CHECK(SecondPrepared.Statistics().FullscreenDraws == 1);
	    }));
	Similar(Reference, FirstImage, .008f);
	HYP_CHECK(Pixel(SecondImage, 192, 144) > Pixel(FirstImage, 192, 144) + .05f);
	Model.Remove();
}

void CheckFailedFrame(FFixture& InFixture, bool bInAfterSubmit)
{
	const auto Frame = InFixture.Session->FreezeFrame();
	bool bFailed = false;
	bool bNotified = false;
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch(
	    {EDomain::Render},
	    [&]
	    {
		    FRenderGraph Graph;
		    InFixture.Pipeline->Build(Graph, InFixture.View, Frame, InFixture.Shadows, InFixture.Clear, {}, true);
		    FRenderGraphCallbacks Callbacks;
		    const auto Fail = []
		    {
			    throw std::runtime_error("Injected deferred frame failure");
		    };
		    if (bInAfterSubmit)
		    {
			    Callbacks.AfterSubmit = Fail;
		    }
		    else
		    {
			    Callbacks.BeforePrepare = Fail;
		    }
		    Callbacks.OnFailure = [&]
		    {
			    bNotified = true;
		    };
		    try
		    {
			    ExecuteGraph(std::move(Graph), InFixture.Tasks, *InFixture.Swapchain,
			                 {InFixture.View.Width, InFixture.View.Height}, false, false, Callbacks);
		    }
		    catch (const std::runtime_error& Error)
		    {
			    HYP_CHECK(std::string(Error.what()) == "Injected deferred frame failure");
			    bFailed = true;
		    }
	    }));
	HYP_CHECK(bFailed && bNotified);
	InFixture.Frame();
}

void CheckReplacementAndRecovery(FFixture& InFixture)
{
	FModelMaterial Material;
	Material.BaseColor = {.5f, .5f, .5f, 1};
	FModel Model(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material));
	Await(Model);
	InFixture.Shadows.bEnabled = true;
	InFixture.Frame();
	for (unsigned Index = 0; Index < 8; ++Index)
	{
		InFixture.View.Width = Index % 2 ? 384 : 320;
		InFixture.View.Height = Index % 2 ? 288 : 240;
		InFixture.Settings.GBuffer = Index % 2 ? FGBufferLayout{} : FGBufferLayout::HighPrecision();
		InFixture.Shadows.Resolution = Index % 2 ? 1024 : 2048;
		InFixture.Frame(Index % 3 ? ESceneRenderPipeline::Deferred : ESceneRenderPipeline::Forward, false);
	}
	InFixture.Settings.GBuffer = {};
	InFixture.View.Width = 384;
	InFixture.View.Height = 288;
	InFixture.Shadows.Resolution = 1024;
	for (unsigned Index = 0; Index < 6; ++Index)
	{
		InFixture.Frame();
	}
	CheckFailedFrame(InFixture, false);
	CheckFailedFrame(InFixture, true);
	const auto Before = InFixture.DeviceStats;
	for (unsigned Index = 0; Index < 10; ++Index)
	{
		InFixture.Frame();
	}
	HYP_CHECK(InFixture.DeviceStats.PipelinesCreated == Before.PipelinesCreated);
	HYP_CHECK(InFixture.DeviceStats.DescriptorAllocations == Before.DescriptorAllocations);
	HYP_CHECK(InFixture.DeviceStats.GpuAllocationBytes <= Before.GpuAllocationBytes + 1024 * 1024);
	InFixture.Settings.DebugMode = 2;
	InFixture.Shadows.DebugMode = 2;
	InFixture.Frame();
	InFixture.Settings.DebugMode = 0;
	InFixture.Shadows.DebugMode = 0;
	Model.Remove();
	InFixture.Shadows.bEnabled = false;
}
} // namespace

int main()
{
	try
	{
		FFixture Fixture;
		CheckConfiguration(Fixture);
		CheckHdrAndRoutes(Fixture);
		CheckCoverageAndInstances(Fixture);
		CheckShadowContinuity(Fixture);
		Fixture.View.Viewport = FViewport{32, 24, 320, 240, .2f, .8f};
		CheckShadowContinuity(Fixture);
		Fixture.View.Viewport->MaxDepth = Fixture.View.Viewport->MinDepth;
		Rejects(
		    [&]
		    {
			    Fixture.Frame();
		    });
		Fixture.View.Viewport.reset();
		RunFullscreenTests(Fixture.Tasks, *Fixture.Device, *Fixture.Swapchain);
		CheckQueuedGenerations(Fixture);
		CheckReplacementAndRecovery(Fixture);
		std::cout << "Deferred rendering tests passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
