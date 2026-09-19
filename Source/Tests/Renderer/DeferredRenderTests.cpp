#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/Model.h"
#include "Hyperion/Renderer/SceneBridge.h"
#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include "Support/ModelAssetSupport.h"
#include "Support/ShaderSourceSupport.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

using namespace Hyperion;
void RunColorTargetTests(IRHIDevice& InDevice, IRHISwapchain& InSwapchain);
void RunClipSpaceTests(FTaskSystem& InTasks, IRHIDevice& InDevice, IRHISwapchain& InSwapchain);
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

std::shared_ptr<const FModelSource> Quad(FModelMaterial InMaterial, bool bInNormalMap = false)
{
	FModelSource Asset;
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
	return std::make_shared<const FModelSource>(std::move(Asset));
}

void Await(const FSourceModel& InModel)
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
	FShaderCompiler Compiler{TestShaderRoot(), "deferred-render-cache"};
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::unique_ptr<FRenderSession> Session;
	std::unique_ptr<FSceneRenderPipeline> Pipeline;
	FRenderView View;
	FCascadedShadowSettings Shadows;
	FForwardPipelineStatistics Statistics;
	FDeviceStats DeviceStats;
	FScenePipelineSettings Settings;
	FSceneRenderBridge* LogicalBridge{};
	FVec4 Clear{.025f, .035f, .065f, 1};

	explicit FFixture(EDepthConvention InConvention = EDepthConvention::Standard)
	{
		View.DepthConvention = InConvention;
		const auto Surface = Window.Surface();
		Tasks.Wait(
		    Tasks.Dispatch({EDomain::Rhi, 0},
		                   [&]
		                   {
			                   FRHIBackendRegistry Registry;
			                   RegisterD3D12RHIBackend(Registry);
			                   Device = Registry.CreateDevice(ERHIBackend::D3D12);
			                   Swapchain = Device->CreateSwapchain(
			                       {Surface, {384, 288}, ERHIDepthFormat::D32, GetDepthClearValue(InConvention)});
			                   Swapchain->SetGpuTimingEnabled(true);
			                   RunColorTargetTests(*Device, *Swapchain);
		                   }));
		Session = std::make_unique<FRenderSession>(Tasks, *Device, Compiler);
		Pipeline = std::make_unique<FSceneRenderPipeline>(*Session, Device->GetCapabilities());
		View.Eye = {0, 0, 5};
		View.Width = 384;
		View.Height = 288;
		View.Camera = FRenderCamera{{0, 0, -1}, {0, 1, 0}, 1, .1f, 40};
		View.ViewProjection = Multiply(Perspective(1, 4.f / 3, .1f, 40, View.DepthConvention), LookAt(View.Eye, {}));
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
		const auto Frame = LogicalBridge ? std::shared_ptr<const FMaterialFrameContext>{} : Session->FreezeFrame();
		const auto Seed = LogicalBridge ? Session->FreezeSceneFrame(LogicalBridge->GetToken()) : nullptr;
		FImage Result;
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          Pipeline->Configure(Settings);
			                          FRenderGraph Graph;
			                          if (Seed)
			                          {
				                          FSceneViewRequest Request;
				                          Request.Width = View.Width;
				                          Request.Height = View.Height;
				                          Request.Viewport = View.Viewport;
				                          Request.DepthConvention = View.DepthConvention;
				                          Request.bInstanceBatching = View.bInstanceBatching;
				                          Request.CullingMode = View.CullingMode;
				                          Pipeline->Build(Graph, Request, Seed, Shadows, Clear, {}, true);
			                          }
			                          else
			                          {
				                          Pipeline->Build(Graph, View, Frame, Shadows, Clear, {}, true);
			                          }
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

class FStageProbe final : public IRenderFeature
{
public:
	explicit FStageProbe(std::vector<ERenderFeatureStage>& InStages) : Stages(InStages)
	{
	}

	void Build(ERenderFeatureStage InStage, FRenderFeatureContext& InContext) override
	{
		HYP_CHECK(InContext.Resources.Depth.Texture && InContext.Resources.Depth.Lifetime);
		HYP_CHECK(InContext.Resources.Depth.Texture->GetDepthTarget()->Width == InContext.View.Width);
		Stages.push_back(InStage);
	}

private:
	std::vector<ERenderFeatureStage>& Stages;
};

void CheckFeatureSelection(FFixture& InFixture)
{
	FRenderFeatureRegistry Features;
	std::vector<ERenderFeatureStage> Stages;
	Features.Add("probe",
	             [&]
	             {
		             return std::make_unique<FStageProbe>(Stages);
	             });
	InFixture.Pipeline = std::make_unique<FSceneRenderPipeline>(*InFixture.Session, InFixture.Device->GetCapabilities(),
	                                                            FScenePipelineSettings{}, Features.Create());
	InFixture.Settings.ContactShadows.bEnabled = true;
	InFixture.Frame();
	HYP_CHECK(!InFixture.Statistics.bContactShadows && InFixture.Statistics.HierarchicalDepth.Bytes == 0);
	HYP_CHECK((Stages == std::vector<ERenderFeatureStage>{
	                         ERenderFeatureStage::AfterOpaque, ERenderFeatureStage::BeforeLighting,
	                         ERenderFeatureStage::BeforeTonemap, ERenderFeatureStage::AfterTonemap}));
	bool bRejected{};
	try
	{
		Features.Add("late", MakeContactShadowFeature);
	}
	catch (const std::logic_error&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
	Features.Remove("probe");
	HYP_CHECK(Features.Create().empty());
	InFixture.Settings.ContactShadows = {};
	InFixture.Pipeline =
	    std::make_unique<FSceneRenderPipeline>(*InFixture.Session, InFixture.Device->GetCapabilities());
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
	FSourceModel Surface(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material));
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
	FSourceModel Transparent(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material));
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
	FSourceModel Unlit(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material));
	Await(Unlit);
	Similar(InFixture.Frame(), InFixture.Frame(ESceneRenderPipeline::Forward), .008f);
	HYP_CHECK(InFixture.Statistics.MainView().VisibleItems == 1);
	Unlit.Remove();
}

void CheckDepthOrdering(FFixture& InFixture)
{
	FModelMaterial Material;
	Material.bUnlit = true;
	Material.BaseColor = {.8f, 0, 0, 1};
	// Create the nearer surface first so wrong depth state lets the later far draw overwrite it.
	FSourceModel Near(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material));
	Near.SetTransform(Translation({0, 0, 1}));
	Material.BaseColor = {0, 0, .8f, 1};
	FSourceModel Far(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material));
	Await(Near);
	Await(Far);
	const auto Opaque = InFixture.Frame();
	const auto Center = (144 * 384 + 192) * 4;
	HYP_CHECK(Opaque.Rgba[Center] > .6f && Opaque.Rgba[Center + 2] < .01f);
	Similar(Opaque, InFixture.Frame(ESceneRenderPipeline::Forward), .008f);
	Near.Remove();
	Far.Remove();
	Material.AlphaMode = EAlphaMode::Blend;
	Material.BaseColor = {.8f, 0, 0, .5f};
	FSourceModel NearBlend(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material));
	NearBlend.SetTransform(Translation({0, 0, 1}));
	Material.BaseColor = {0, 0, .8f, .5f};
	FSourceModel FarBlend(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material));
	Await(NearBlend);
	Await(FarBlend);
	const auto SavedClear = InFixture.Clear;
	InFixture.Clear = {0, 0, 0, 1};
	const auto Blended = InFixture.Frame();
	// Far blue then near red: HDR (.4, 0, .2), followed by Reinhard and sRGB.
	HYP_CHECK(std::abs(Blended.Rgba[Center] - .571f) < .008f);
	HYP_CHECK(std::abs(Blended.Rgba[Center + 2] - .445f) < .008f);
	Similar(Blended, InFixture.Frame(ESceneRenderPipeline::Forward), .008f);
	InFixture.Clear = SavedClear;
	NearBlend.Remove();
	FarBlend.Remove();
}

void CheckViewDepthCacheIsolation(FFixture& InFixture)
{
	FModelMaterial Material;
	Material.bUnlit = true;
	Material.BaseColor = {.8f, .2f, .1f, 1};
	const auto Asset = Quad(Material);
	FSourceModel A(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Asset);
	FSourceModel B(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Asset);
	A.SetTransform(Translation({-.5f, 0, 1}));
	B.SetTransform(Translation({.5f, 0, 0}));
	Await(A);
	Await(B);
	const auto Reference = InFixture.Frame();
	HYP_CHECK(InFixture.Statistics.MainView().Batches.InstancedItems == 2);
	const auto Initial = InFixture.View.DepthConvention;
	const auto Other = Initial == EDepthConvention::Reversed ? EDepthConvention::Standard : EDepthConvention::Reversed;
	const auto Surface = InFixture.Window.Surface();

	// Reuse the session/material/batch caches across explicitly different views. A new
	// swapchain keeps optimized clear metadata consistent; Viewer does not expose this switch.
	for (const auto Convention : {Other, Initial, Other, Initial})
	{
		InFixture.Tasks.Wait(InFixture.Tasks.Dispatch(
		    {EDomain::Rhi, 0},
		    [&]
		    {
			    InFixture.Device->WaitIdle();
			    InFixture.Swapchain.reset();
			    InFixture.Swapchain = InFixture.Device->CreateSwapchain(
			        {Surface, {384, 288}, ERHIDepthFormat::D32, GetDepthClearValue(Convention)});
			    InFixture.Swapchain->SetGpuTimingEnabled(true);
		    }));
		InFixture.View.DepthConvention = Convention;
		InFixture.View.ViewProjection =
		    Multiply(Perspective(1, 4.f / 3, .1f, 40, Convention), LookAt(InFixture.View.Eye, {}));
		Similar(Reference, InFixture.Frame(), .008f);
		HYP_CHECK(InFixture.Statistics.MainView().Batches.InstancedItems == 2);
		// Replacing scene targets can retire fullscreen PSOs; stationary frames must reuse them.
		const auto Pipelines = InFixture.DeviceStats.PipelinesCreated;
		Similar(Reference, InFixture.Frame(), .008f);
		HYP_CHECK(InFixture.DeviceStats.PipelinesCreated == Pipelines);
	}
	A.Remove();
	B.Remove();
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
	FSourceModel A(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Asset);
	FSourceModel B(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Asset);
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
	FSourceModel Receiver(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material, true));
	Receiver.SetTransform(Scale({4, 4, 1}));
	Material.AlphaMode = EAlphaMode::Mask;
	FSourceModel Caster(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material));
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
	InFixture.View.ViewProjection = Multiply(Perspective(1, 4.f / 3, .1f, 40, InFixture.View.DepthConvention),
	                                         LookAt(InFixture.View.Eye, {.015f, 0, 0}));
	Similar(InFixture.Frame(), InFixture.Frame(ESceneRenderPipeline::Forward), .065f);
	Receiver.Remove();
	Caster.Remove();
	InFixture.Shadows.bEnabled = false;
}

void AwaitScene(FSceneRenderBridge& InBridge, std::span<const FSceneHandle> InHandles)
{
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
	while (true)
	{
		InBridge.Flush();
		bool bReady = true;
		for (const auto Handle : InHandles)
		{
			HYP_CHECK(InBridge.GetError(Handle).empty());
			bReady &= InBridge.IsReady(Handle);
		}
		if (bReady)
		{
			return;
		}
		HYP_CHECK(std::chrono::steady_clock::now() < Deadline);
		std::this_thread::yield();
	}
}

float HdrPixel(const FImage& InImage, unsigned InX = 192, unsigned InY = 144)
{
	const float Srgb = Pixel(InImage, InX, InY);
	const float Linear = Srgb <= .04045f ? Srgb / 12.92f : std::pow((Srgb + .055f) / 1.055f, 2.4f);
	return Linear / std::max(1.f - Linear, 1e-6f);
}

void CheckLocalVolumeCoverage(FFixture& InFixture, FScene& InScene, FSceneHandle InCamera, FSceneRenderBridge& InBridge,
                              float InReference)
{
	for (const float Eye : {10.f, 6.01f, 6.f, 5.99f, 5.f, 2.f, 1.f, .25f})
	{
		InScene.SetCameraView(InCamera, SceneCameraTransform({0, 0, Eye}, {}), {1, .01f, Eye + .1f, Eye});
		InBridge.Flush();
		const auto Image = InFixture.Frame();
		const float Actual = HdrPixel(Image);
		std::cout << "local volume eye=" << Eye << " hdr=" << Actual << " expected=" << InReference << '\n';
		HYP_CHECK(std::abs(Actual - InReference) < .012f);
		HYP_CHECK(InFixture.Statistics.LocalLights.Draws == (InFixture.Settings.bClusteredLighting ? 0 : 1));
	}
	InScene.SetCameraView(InCamera, SceneCameraTransform({0, 0, 5}, {}), {1, .1f, 40, 5});
	InBridge.Flush();
	InFixture.View.Viewport = FViewport{32, 24, 300, 180, .2f, .85f};
	HYP_CHECK(std::abs(HdrPixel(InFixture.Frame(), 182, 114) - InReference) < .012f);
	InFixture.View.Viewport.reset();
}

void CheckLocalLightCulling(FFixture& InFixture, FScene& InScene, FSceneHandle InPoint, FSceneRenderBridge& InBridge)
{
	InScene.SetWorldTransform(InPoint, Translation({4, 0, 2}));
	InScene.SetPointLight(InPoint, {{1, 1, 1}, 20, 8});
	InBridge.Flush();
	InFixture.View.CullingMode = ESceneCullingMode::None;
	const auto Reference = InFixture.Frame();
	HYP_CHECK(HdrPixel(Reference) > .01f);
	for (const auto Mode : {ESceneCullingMode::Linear, ESceneCullingMode::Bvh})
	{
		InFixture.View.CullingMode = Mode;
		HYP_CHECK(InFixture.Frame().Rgba == Reference.Rgba);
	}
	InScene.SetWorldTransform(InPoint, Translation({100, 0, 2}));
	InBridge.Flush();
	InFixture.Frame();
	HYP_CHECK(InFixture.Statistics.LocalLights.Draws == 0);
	InScene.SetWorldTransform(InPoint, Translation({0, 0, 2}));
	InScene.SetPointLight(InPoint, {{1, 1, 1}, 5, 4});
	InBridge.Flush();
}

void CheckLocalLightLifetime(FFixture& InFixture, FScene& InScene, FSceneHandle InPoint, FSceneRenderBridge& InBridge,
                             const FImage& InReference)
{
	auto& F = InFixture;
	const auto Seed = F.Session->FreezeSceneFrame(InBridge.GetToken());
	FRenderGraph Pending;
	F.Tasks.Wait(F.Tasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              FSceneViewRequest Request;
		                              Request.Width = F.View.Width;
		                              Request.Height = F.View.Height;
		                              Request.DepthConvention = F.View.DepthConvention;
		                              F.Pipeline->Build(Pending, Request, Seed, F.Shadows, F.Clear, {}, true);
	                              }));
	InScene.SetEnabled(InPoint, false);
	InBridge.Flush();
	F.Settings.GBuffer = FGBufferLayout::HighPrecision();
	const auto NewFrame = F.Frame(ESceneRenderPipeline::Forward);
	HYP_CHECK(HdrPixel(NewFrame) < .03f);
	FImage OldFrame;
	F.Tasks.Wait(F.Tasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              OldFrame = ExecuteGraph(std::move(Pending), F.Tasks, *F.Swapchain, {384, 288},
		                                                      false, true);
	                              }));
	Similar(InReference, OldFrame, .008f);
	InScene.SetEnabled(InPoint, true);
	InBridge.Flush();
	F.Settings.GBuffer = {};
	for (unsigned Index = 0; Index < 6; ++Index)
	{
		F.View.Width = Index % 2 ? 384 : 320;
		F.View.Height = Index % 2 ? 288 : 240;
		const auto Resized = F.Frame();
		HYP_CHECK(std::abs(HdrPixel(Resized, F.View.Width / 2, F.View.Height / 2) - HdrPixel(InReference)) < .012f);
		HYP_CHECK(F.Statistics.LocalLights.Draws == (F.Settings.bClusteredLighting ? 0 : 1));
	}
	for (unsigned Index = 0; Index < 10; ++Index)
	{
		F.Frame();
	}
	const auto Before = F.DeviceStats;
	const auto LiveBefore = F.Session->GetResources().Statistics().Materials.LiveObjects;
	for (unsigned Index = 0; Index < 24; ++Index)
	{
		InScene.SetWorldTransform(InPoint, Translation({.1f * float(Index % 3), 0, 2}));
		InBridge.Flush();
		F.Frame();
	}
	HYP_CHECK(F.DeviceStats.PipelinesCreated == Before.PipelinesCreated);
	// DescriptorAllocations is cumulative; changing immutable read buffers legitimately creates new sets.
	if (!F.Settings.bClusteredLighting)
	{
		HYP_CHECK(F.DeviceStats.DescriptorAllocations == Before.DescriptorAllocations);
	}
	HYP_CHECK(F.Session->GetResources().Statistics().Materials.LiveObjects <= LiveBefore + 32);
	HYP_CHECK(F.DeviceStats.GpuAllocationBytes <= Before.GpuAllocationBytes + 1024 * 1024);
	InScene.SetWorldTransform(InPoint, Translation({0, 0, 2}));
	InBridge.Flush();
}

void CheckLocalLightCenter(FFixture& InFixture, FScene& InScene, FSceneHandle InPoint, FSceneRenderBridge& InBridge)
{
	InScene.SetWorldTransform(InPoint, Translation({0, 0, 0}));
	InBridge.Flush();
	const auto AtCenter = InFixture.Frame();
	HYP_CHECK(std::isfinite(HdrPixel(AtCenter)) && HdrPixel(AtCenter) >= .018f);
	InScene.SetWorldTransform(InPoint, Translation({0, 0, 2}));
	InBridge.Flush();
}

void CheckStationaryLocalRetirement(FFixture& InFixture, FScene& InScene, FSceneRenderBridge& InBridge)
{
	std::vector<FSceneHandle> Added;
	for (unsigned Index = 0; Index < 12; ++Index)
	{
		auto Node = MakeScenePointLightNode("retirement-" + std::to_string(Index));
		Node.Local() = Translation({0, 0, 2});
		Node.PointLight() = FScenePointLight{{1, 1, 1}, float(Index + 1), 4};
		Added.push_back(InScene.AddNode(Node));
	}
	InBridge.Flush();
	for (unsigned Index = 0; Index < 16; ++Index)
	{
		InFixture.Frame(ESceneRenderPipeline::Deferred, false);
	}
	const auto Before = InFixture.DeviceStats;
	for (unsigned Index = 0; Index < 96; ++Index)
	{
		InFixture.Frame(ESceneRenderPipeline::Deferred, false);
		HYP_CHECK(InFixture.Statistics.LocalLights.Draws == (InFixture.Settings.bClusteredLighting ? 0 : 13));
		HYP_CHECK(InFixture.DeviceStats.GpuAllocationBytes <= Before.GpuAllocationBytes + 4 * 65536);
	}
	HYP_CHECK(InFixture.DeviceStats.PipelinesCreated == Before.PipelinesCreated);
	HYP_CHECK(InFixture.DeviceStats.DescriptorAllocations == Before.DescriptorAllocations);
	if (InFixture.Settings.bClusteredLighting)
	{
		const auto Camera = *InScene.GetSettings().DefaultCamera;
		for (unsigned Index = 1; Index <= 12; ++Index)
		{
			const float X = float(Index) * .00001f;
			InScene.SetWorldTransform(Camera, SceneCameraTransform({X, 0, 5}, {X, 0, 0}));
			InBridge.Flush();
			InFixture.Frame(ESceneRenderPipeline::Deferred, false);
		}
		HYP_CHECK(InFixture.DeviceStats.DescriptorAllocations == Before.DescriptorAllocations);
		InScene.SetWorldTransform(Camera, SceneCameraTransform({0, 0, 5}, {}));
	}
	for (const auto Handle : Added)
	{
		InScene.RemoveSubtree(Handle);
	}
	InBridge.Flush();
}

void CheckSpotAttenuation(FFixture& InFixture, FScene& InScene, FSceneHandle InPoint, FSceneHandle InSpot,
                          FSceneRenderBridge& InBridge)
{
	InScene.SetEnabled(InPoint, true);
	InScene.SetEnabled(InSpot, false);
	InBridge.Flush();
	const auto Point = InFixture.Frame();
	InScene.SetEnabled(InPoint, false);
	InScene.SetEnabled(InSpot, true);
	InBridge.Flush();
	const auto Spot = InFixture.Frame();
	// Center is fully inside the inner cone; a lateral receiver is in the penumbra; far lateral is outside.
	HYP_CHECK(std::abs(HdrPixel(Point) - HdrPixel(Spot)) < .01f);
	HYP_CHECK(HdrPixel(Spot, 225, 144) < HdrPixel(Point, 225, 144) - .005f);
	HYP_CHECK(HdrPixel(Spot, 225, 144) > .021f);
	HYP_CHECK(std::abs(HdrPixel(Spot, 300, 144) - .02f) < .002f);
}

void CheckLocalTransparentRoute(FFixture& InFixture, FScene& InScene, FSceneHandle InSurface, FSceneHandle InPoint,
                                FSceneRenderBridge& InBridge)
{
	FModelMaterial Material;
	Material.BaseColor = {.4f, .3f, .2f, .5f};
	Material.Metallic = 0;
	Material.Roughness = .5f;
	Material.Emissive = {.05f, .05f, .05f};
	Material.AlphaMode = EAlphaMode::Blend;
	InScene.SetModelVisible(InSurface, false);
	const auto Transparent =
	    InScene.Add({"transparent receiver", PrepareSourceModel(Quad(Material)), Scale({20, 20, 1})});
	InScene.SetEnabled(InPoint, true);
	AwaitScene(InBridge, std::array{Transparent});
	const auto WithLight = InFixture.Frame();
	if (InFixture.Settings.bClusteredLighting)
	{
		Similar(WithLight, InFixture.Frame(ESceneRenderPipeline::Forward), .008f);
	}
	InScene.SetEnabled(InPoint, false);
	InBridge.Flush();
	const auto WithoutLight = InFixture.Frame();
	if (InFixture.Settings.bClusteredLighting)
	{
		HYP_CHECK(HdrPixel(WithLight) > HdrPixel(WithoutLight) + .01f);
	}
	else
	{
		HYP_CHECK(WithoutLight.Rgba == WithLight.Rgba);
	}
	InScene.RemoveSubtree(Transparent);
	InScene.SetModelVisible(InSurface, true);
	InBridge.Flush();
}

void CheckClusteredDirectionalAndDense(FFixture& InFixture, FScene& InScene, FSceneRenderBridge& InBridge)
{
	if (!InFixture.Settings.bClusteredLighting)
	{
		return;
	}
	const auto Original = InScene.GetSettings();
	auto SunNode = MakeSceneDirectionalLightNode("cluster sun");
	SunNode.DirectionalLight() = FSceneDirectionalLight{{1, 1, 1}, .5f, false};
	const auto Sun = InScene.AddNode(SunNode);
	auto Settings = Original;
	Settings.MainDirectionalLight = Sun;
	InScene.SetSettings(Settings);
	InBridge.Flush();
	const auto Fused = InFixture.Frame();
	HYP_CHECK(InFixture.Statistics.LocalLights.Draws == 0);
	Similar(Fused, InFixture.Frame(ESceneRenderPipeline::Forward), .008f);
	InFixture.Settings.bClusteredLighting = false;
	Similar(Fused, InFixture.Frame(), .008f);
	InFixture.Settings.bClusteredLighting = true;
	InScene.SetSettings(Original);
	InScene.RemoveSubtree(Sun);
	std::vector<FSceneHandle> Added;
	for (unsigned Index = 0; Index < 80; ++Index)
	{
		auto Node = MakeScenePointLightNode("dense-" + std::to_string(Index));
		Node.Local() = Translation({0, 0, 2});
		Node.PointLight() = FScenePointLight{{1, 1, 1}, .02f, 4};
		Added.push_back(InScene.AddNode(Node));
	}
	InBridge.Flush();
	const auto Dense = InFixture.Frame();
	HYP_CHECK(InFixture.Statistics.LocalLights.ClusterMaximum == 81);
	Similar(Dense, InFixture.Frame(ESceneRenderPipeline::Forward), .008f);
	InFixture.Settings.bClusteredLighting = false;
	// Eighty half-float additive roundings can differ from one float accumulation and final half conversion.
	Similar(Dense, InFixture.Frame(), .016f);
	InFixture.Settings.bClusteredLighting = true;
	for (const auto Handle : Added)
	{
		InScene.RemoveSubtree(Handle);
	}
	InBridge.Flush();
}

void CheckOwnedLocalLights(FFixture& InFixture)
{
	auto& F = InFixture;
	FScene Scene;
	const auto Camera = Scene.AddNode(MakeSceneCameraNode("camera", {0, 0, 5}, {}, {1, .1f, 40, 5}));
	Scene.SetSettings({Camera, {}, {}});
	FModelMaterial Material;
	Material.BaseColor = {.5f, .5f, .5f, 1};
	Material.Metallic = 0;
	Material.Roughness = 1;
	Material.Emissive = {.02f, .02f, .02f};
	const auto Surface = Scene.Add({"receiver", PrepareSourceModel(Quad(Material)), Scale({20, 20, 1})});
	auto Node = MakeScenePointLightNode("point");
	Node.Local() = Translation({0, 0, 2});
	Node.PointLight() = FScenePointLight{{1, 1, 1}, 5, 4};
	const auto Point = Scene.AddNode(Node);
	FSceneRenderBridge Bridge(Scene, *F.Session, F.Tasks);
	F.LogicalBridge = &Bridge;
	AwaitScene(Bridge, std::array{Surface});
	const auto Lit = F.Frame();
	const float One = HdrPixel(Lit);
	HYP_CHECK(One > .1f && F.Statistics.LocalLights.Draws == (F.Settings.bClusteredLighting ? 0 : 1));
	if (F.Settings.bClusteredLighting)
	{
		HYP_CHECK(F.Statistics.LocalLights.ClusterReferences > 0);
		Similar(Lit, F.Frame(ESceneRenderPipeline::Forward), .008f);
		F.Settings.bClusteredLighting = false;
		Similar(Lit, F.Frame(), .008f);
		F.Settings.bClusteredLighting = true;
		Similar(Lit, F.Frame(), .008f);
	}
	// Normal incidence, roughness=1: diffuse .48/pi plus specular .01/pi, attenuated at d=2/R=4.
	const float Expected = .02f + (.49f / 3.14159265f) * (5.f / 4) * std::pow(1.f - 1.f / 16, 2.f);
	HYP_CHECK(std::abs(One - Expected) < .01f);
	CheckLocalVolumeCoverage(F, Scene, Camera, Bridge, One);
	CheckLocalLightLifetime(F, Scene, Point, Bridge, Lit);
	CheckLocalLightCenter(F, Scene, Point, Bridge);
	CheckStationaryLocalRetirement(F, Scene, Bridge);
	CheckClusteredDirectionalAndDense(F, Scene, Bridge);
	Scene.SetEnabled(Point, false);
	Bridge.Flush();
	const auto Dark = F.Frame();
	const float Base = HdrPixel(Dark);
	HYP_CHECK(std::abs(Base - .02f) < .002f && F.Statistics.LocalLights.Draws == 0);
	Scene.SetEnabled(Point, true);
	Node.Id = "second";
	const auto Second = Scene.AddNode(Node);
	Bridge.Flush();
	const auto Two = F.Frame();
	HYP_CHECK(std::abs(HdrPixel(Two) - (2 * One - Base)) < .012f);
	HYP_CHECK(F.Statistics.LocalLights.Draws == (F.Settings.bClusteredLighting ? 0 : 2));
	Similar(F.Settings.bClusteredLighting ? Two : Dark, F.Frame(ESceneRenderPipeline::Forward), .008f);
	HYP_CHECK(F.Statistics.LocalLights.bActive == F.Settings.bClusteredLighting && F.Statistics.LocalLights.Draws == 0);
	Scene.RemoveSubtree(Second);
	CheckLocalLightCulling(F, Scene, Point, Bridge);
	Scene.SetEnabled(Point, false);
	auto Cone = MakeSceneSpotLightNode("spot");
	Cone.Local() = Translation({0, 0, 2});
	Cone.SpotLight() = FSceneSpotLight{{1, 1, 1}, 5, 4, .2f, .65f};
	const auto Spot = Scene.AddNode(Cone);
	Bridge.Flush();
	HYP_CHECK(std::abs(HdrPixel(F.Frame()) - One) < .01f);
	CheckSpotAttenuation(F, Scene, Point, Spot, Bridge);
	// Cross the cone apex from outside to inside while its exit lies beyond the camera far plane.
	Scene.SetCameraView(Camera, SceneCameraTransform({0, 0, 1}, {}), {1, .01f, 1.1f, 1});
	Bridge.Flush();
	HYP_CHECK(std::abs(HdrPixel(F.Frame()) - One) < .012f);
	Scene.SetWorldTransform(Spot, SceneCameraTransform({0, 0, 2}, {0, 0, 3}));
	Bridge.Flush();
	HYP_CHECK(std::abs(HdrPixel(F.Frame()) - Base) < .002f);
	Scene.SetWorldTransform(Spot, Translation({0, 0, 2}));
	Scene.SetSpotLight(Spot, {{1, 1, 1}, 5, 1, .2f, .65f});
	Bridge.Flush();
	HYP_CHECK(std::abs(HdrPixel(F.Frame()) - Base) < .002f);
	CheckLocalTransparentRoute(F, Scene, Surface, Point, Bridge);
	Bridge.Close();
	F.LogicalBridge = nullptr;
	std::cout << "Local light GPU: analytic BRDF, additivity, camera/clip crossing, cone, culling and Forward "
	             "exclusion passed\n";
}

void CheckClusteredMaterialCapacity(FFixture& InFixture)
{
	auto& F = InFixture;
	FScene Scene;
	AddDefaultSceneContent(Scene, {0, 0, 5}, {}, {1, .1f, 40, 5});
	auto Point = MakeScenePointLightNode("capacity point");
	Point.Local() = Translation({0, 0, 2});
	Point.PointLight() = FScenePointLight{{1, 1, 1}, 5, 4};
	Scene.AddNode(Point);
	std::vector<FSceneHandle> Models;
	for (unsigned Index = 0; Index < 32; ++Index)
	{
		FModelMaterial Material;
		Material.BaseColor = {.5f, .5f, .5f, 1};
		// Independent texture assets require distinct resource sets even when their texels match.
		Models.push_back(Scene.Add({"material " + std::to_string(Index), PrepareSourceModel(Quad(Material, true))}));
	}
	FSceneRenderBridge Bridge(Scene, *F.Session, F.Tasks);
	F.LogicalBridge = &Bridge;
	AwaitScene(Bridge, Models);
	F.Shadows.bEnabled = true;
	for (unsigned Index = 0; Index < 8; ++Index)
	{
		F.Settings.bClusteredLighting = Index % 2 == 0;
		F.Frame(ESceneRenderPipeline::Forward, false);
		HYP_CHECK(F.Statistics.MainView().VisibleItems == Models.size());
		HYP_CHECK(F.Statistics.MainView().Draws == Models.size());
	}
	F.Settings.bClusteredLighting = true;
	F.Shadows.bEnabled = false;
	Bridge.Close();
	F.LogicalBridge = nullptr;
}

void CheckOwnedOffscreenShadow(FFixture& InFixture)
{
	auto& F = InFixture;
	FScene Scene;
	AddDefaultSceneContent(Scene, {0, 0, 5}, {}, {1, .1f, 40, 5});
	const auto Camera = *Scene.GetSettings().DefaultCamera;
	const auto Sun = *Scene.GetSettings().MainDirectionalLight;
	Scene.SetWorldTransform(Sun, SceneCameraTransform({}, {-3, 0, -1}));
	Scene.SetDirectionalLight(Sun, {{1, 1, 1}, 3, true});
	FSceneNode Parent;
	Parent.Id = "caster-parent";
	Parent.Local() = Translation({3.5f, 0, 1});
	const auto Rig = Scene.AddNode(Parent);
	FModelMaterial Material;
	Material.BaseColor = {.6f, .6f, .6f, 1};
	Material.Metallic = 0;
	Material.Roughness = 1;
	const auto Data = PrepareSourceModel(Quad(Material));
	FSceneModel Receiver{"receiver", Data, Scale({4, 4, 1})};
	const auto Ground = Scene.Add(Receiver);
	FSceneNode Caster;
	Caster.Id = "offscreen-caster";
	Caster.Parent() = "caster-parent";
	Caster.Local() = Scale({.4f, .4f, 1});
	Caster.Model() = FSceneModelComponent{};
	Caster.Model()->Data = Data;
	const auto Occluder = Scene.AddNode(Caster);
	FSceneRenderBridge Bridge(Scene, *F.Session, F.Tasks);
	F.LogicalBridge = &Bridge;
	const std::array Handles{Ground, Occluder};
	AwaitScene(Bridge, Handles);
	F.Shadows.bEnabled = false;
	const auto Unshadowed = F.Frame();
	HYP_CHECK(F.Statistics.MainView().VisibleItems == 1);
	F.Shadows.bEnabled = true;
	const auto Shadowed = F.Frame();
	HYP_CHECK(F.Statistics.MainView().VisibleItems == 1 && F.Statistics.bShadows);
	std::size_t ShadowItems{};
	for (const auto& View : F.Statistics.Views)
	{
		if (View.Usage == "ShadowDepth")
		{
			ShadowItems += View.Visibility.VisibleItems;
		}
	}
	HYP_CHECK(ShadowItems >= 2);
	float MaximumShadow{};
	for (std::size_t Index = 0; Index < Shadowed.Rgba.size(); Index += 4)
	{
		MaximumShadow = std::max(MaximumShadow, Unshadowed.Rgba[Index] - Shadowed.Rgba[Index]);
	}
	std::cout << "Owned offscreen shadow contrast=" << MaximumShadow << " shadow items=" << ShadowItems << "\n";
	HYP_CHECK(MaximumShadow > .05f);
	Similar(Shadowed, F.Frame(ESceneRenderPipeline::Forward), .065f);
	Scene.SetLocalTransform(Rig, Translation({3.5f, .6f, 1}));
	Bridge.Flush();
	HYP_CHECK(F.Frame().Rgba != Shadowed.Rgba);
	auto Lens = *Scene.FindCamera(Camera);
	Lens.VerticalRadians = .9f;
	Scene.SetCamera(Camera, Lens);
	Scene.SetWorldTransform(Sun, SceneCameraTransform({}, {0, -1, -.001f}, {0, 0, 1}));
	Bridge.Flush();
	Similar(F.Frame(), F.Frame(ESceneRenderPipeline::Forward), .065f);
	Scene.SetDirectionalLight(Sun, {{1, 1, 1}, 3, false});
	Bridge.Flush();
	F.Frame();
	HYP_CHECK(!F.Statistics.bShadows && !F.Pipeline->Shadows().IsEnabled());
	Scene.RemoveSubtree(Sun);
	Bridge.Flush();
	F.Frame();
	HYP_CHECK(!F.Statistics.bShadows && !F.Pipeline->Shadows().IsEnabled());
	Bridge.Close();
	F.LogicalBridge = nullptr;
	F.Shadows.bEnabled = false;
}

void CheckOwnedCameraLightRoutes(FFixture& InFixture)
{
	auto& F = InFixture;
	FScene Scene;
	FSceneNode Rig;
	Rig.Id = "rig";
	Rig.Local() = Translation({.2f, 0, 0});
	const auto Parent = Scene.AddNode(Rig);
	auto CameraNode = MakeSceneCameraNode("camera", {-.2f, 0, 5}, {-.2f, 0, 0}, {1, .1f, 40, 5});
	CameraNode.Parent() = "rig";
	const auto Camera = Scene.AddNode(CameraNode);
	auto SunNode = MakeSceneDirectionalLightNode("sun");
	SunNode.Parent() = "rig";
	SunNode.Local() = SceneCameraTransform({}, {-1, 0, -1});
	SunNode.DirectionalLight() = FSceneDirectionalLight{{1, 1, 1}, 3, true};
	const auto Sun = Scene.AddNode(SunNode);
	auto Environment = MakeSceneEnvironmentLightNode("environment");
	Environment.EnvironmentLight() = FSceneEnvironmentLight{{1, 1, 1}, .2f};
	const auto Ambient = Scene.AddNode(Environment);
	Scene.SetSettings({Camera, Sun, Ambient});
	FSceneRenderBridge Bridge(Scene, *F.Session, F.Tasks);
	F.LogicalBridge = &Bridge;
	FModelMaterial Material;
	Material.BaseColor = {.4f, .2f, .1f, 1};
	Material.Metallic = .6f;
	Material.Roughness = .25f;
	Material.bDoubleSided = true;
	Material.AlphaMode = EAlphaMode::Mask;
	const auto Data = PrepareSourceModel(Quad(Material, true));
	FSceneModel Left{"left", Data};
	Left.World = Multiply(Translation({-.9f, 0, 0}), Scale({.7f, .7f, 1}));
	const auto A = Scene.Add(Left);
	Scene.Reparent(A, Parent, ESceneReparentMode::KeepWorld);
	Left.World = Multiply(Translation({.9f, 0, 0}), Scale({.7f, .7f, 1}));
	const auto B = Scene.Add(Left);
	Material.bUnlit = true;
	Material.AlphaMode = EAlphaMode::Blend;
	Material.BaseColor = {.1f, .2f, .8f, .4f};
	FSceneModel Transparent{"transparent", PrepareSourceModel(Quad(Material))};
	Transparent.World = Multiply(Translation({0, -.5f, 1}), Scale({.4f, .4f, 1}));
	const auto C = Scene.Add(Transparent);
	const std::array Handles{A, B, C};
	AwaitScene(Bridge, Handles);
	for (const bool bShadows : {false, true})
	{
		F.Shadows.bEnabled = bShadows;
		F.View.bInstanceBatching = true;
		const auto Reference = F.Frame();
		HYP_CHECK(F.Statistics.SceneToken == Bridge.GetToken());
		HYP_CHECK(F.Statistics.MainView().VisibleItems == 3);
		HYP_CHECK(F.Statistics.MainView().Batches.InstancedItems == 2);
		HYP_CHECK(F.Statistics.bShadows == bShadows);
		Similar(Reference, F.Frame(ESceneRenderPipeline::Forward), .065f);
		F.View.bInstanceBatching = false;
		Similar(Reference, F.Frame(), .008f);
		HYP_CHECK(F.Statistics.MainView().Batches.InstancedItems == 0);
		F.View.bInstanceBatching = true;
	}
	Scene.SetWorldTransform(B, Multiply(Translation({.9f, 0, 0}), Scale({-.7f, .7f, 1})));
	Bridge.Flush();
	Similar(F.Frame(), F.Frame(ESceneRenderPipeline::Forward), .065f);
	F.View.Viewport = FViewport{32, 24, 300, 180};
	auto Lens = *Scene.FindCamera(Camera);
	Lens.VerticalRadians = .8f;
	Scene.SetCamera(Camera, Lens);
	Scene.SetLocalTransform(Parent, Translation({.4f, 0, 0}));
	Bridge.Flush();
	Similar(F.Frame(), F.Frame(ESceneRenderPipeline::Forward), .065f);
	const auto HierarchyReference = F.Frame();
	HYP_CHECK(F.Statistics.MainView().Groups == 3 && F.Statistics.MainView().UnboundedGroups == 0);
	for (const auto Mode : {ESceneCullingMode::None, ESceneCullingMode::Linear, ESceneCullingMode::Bvh})
	{
		F.View.CullingMode = Mode;
		HYP_CHECK(HierarchyReference.Rgba == F.Frame().Rgba);
		HYP_CHECK(F.Statistics.MainView().VisibleItems == 3);
	}
	F.View.Viewport.reset();
	Scene.SetEnabled(Sun, false);
	Bridge.Flush();
	const auto WithoutSun = F.Frame();
	HYP_CHECK(!F.Statistics.bShadows && !F.Pipeline->Shadows().IsEnabled());
	Similar(WithoutSun, F.Frame(ESceneRenderPipeline::Forward), .025f);
	Scene.SetEnabled(Camera, false);
	Bridge.Flush();
	const auto Clear = F.Frame();
	HYP_CHECK(F.Statistics.CameraStatus == ESceneCameraStatus::NoActiveCamera);
	HYP_CHECK(std::abs(Pixel(Clear, 192, 144) - F.Clear.X) < .004f);
	Scene.SetEnabled(Camera, true);
	Bridge.Flush();
	HYP_CHECK(F.Frame().Rgba != Clear.Rgba);
	Bridge.Close();
	F.LogicalBridge = nullptr;
	F.Shadows.bEnabled = false;
	F.View.bInstanceBatching = true;
	std::cout << "Scene-owned inherited camera/light: Forward/Deferred, shadow, instance, mask, blend, mirror and "
	             "clear parity passed\n";
}

void CheckQueuedGenerations(FFixture& InFixture)
{
	FModelMaterial Material;
	Material.BaseColor = {0, 0, 0, 1};
	Material.Emissive = {.5f, .25f, .1f};
	FSourceModel Model(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material));
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
	FSourceModel Model(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material));
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

void CheckContactRoutes(FFixture& InFixture)
{
	FModelMaterial Material;
	Material.BaseColor = {.5f, .5f, .5f, 1};
	FSourceModel Model(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Quad(Material));
	Await(Model);
	InFixture.Shadows.bEnabled = false;
	for (unsigned Index = 0; Index < 4; ++Index)
	{
		InFixture.Frame();
	}
	for (const bool bClustered : {false, true})
	{
		InFixture.Settings.bClusteredLighting = bClustered;
		for (const bool bShadows : {false, true})
		{
			InFixture.Shadows.bEnabled = bShadows;
			InFixture.Settings.ContactShadows.bEnabled = false;
			const auto Baseline = InFixture.Frame();
			HYP_CHECK(InFixture.Statistics.HierarchicalDepth.Dispatches == 0);
			InFixture.Settings.ContactShadows.bEnabled = true;
			InFixture.Frame();
			const auto Contact = InFixture.Frame();
			HYP_CHECK(InFixture.Statistics.bContactShadows && InFixture.Statistics.HierarchicalDepth.Consumers == 1);
			HYP_CHECK(InFixture.Statistics.HierarchicalDepth.Dispatches == 9);
			HYP_CHECK(Baseline.Rgba == Contact.Rgba);
			const auto Before = InFixture.DeviceStats;
			InFixture.Frame();
			HYP_CHECK(InFixture.DeviceStats.PipelinesCreated == Before.PipelinesCreated);
			HYP_CHECK(InFixture.DeviceStats.DescriptorAllocations == Before.DescriptorAllocations);
		}
	}
	InFixture.Frame(ESceneRenderPipeline::Forward);
	HYP_CHECK(!InFixture.Statistics.bContactShadows && InFixture.Statistics.HierarchicalDepth.Dispatches == 0);
	InFixture.Settings.ContactShadows.bEnabled = false;
	InFixture.Settings.ContactShadows.DebugMode = 2;
	InFixture.Frame();
	HYP_CHECK(!InFixture.Statistics.bContactShadows && InFixture.Statistics.HierarchicalDepth.Consumers == 1);
	InFixture.Settings.ContactShadows.bEnabled = true;
	InFixture.Frame();
	HYP_CHECK(InFixture.Statistics.HierarchicalDepth.Consumers == 2 &&
	          InFixture.Statistics.HierarchicalDepth.Products == 1);
	InFixture.Settings.ContactShadows.DebugMode = 0;
	InFixture.View.Viewport = FViewport{32, 24, 320, 240, .2f, .8f};
	InFixture.Frame();
	InFixture.View.Viewport.reset();
	InFixture.View.Width = 320;
	InFixture.View.Height = 240;
	InFixture.Frame();
	HYP_CHECK(InFixture.Statistics.HierarchicalDepth.Bytes > 320U * 240U * 4U);
	InFixture.View.Width = 384;
	InFixture.View.Height = 288;
	InFixture.Settings.ContactShadows = {};
	InFixture.Shadows.bEnabled = false;
	InFixture.Frame();
	HYP_CHECK(InFixture.Statistics.HierarchicalDepth.Bytes == 0 && !InFixture.Statistics.bContactShadows);
	Model.Remove();
}
} // namespace

int main()
{
	try
	{
		for (const auto Convention : {EDepthConvention::Standard, EDepthConvention::Reversed})
		{
			FFixture Fixture(Convention);
			CheckFeatureSelection(Fixture);
			CheckConfiguration(Fixture);
			CheckContactRoutes(Fixture);
			CheckHdrAndRoutes(Fixture);
			CheckDepthOrdering(Fixture);
			CheckViewDepthCacheIsolation(Fixture);
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
			RunClipSpaceTests(Fixture.Tasks, *Fixture.Device, *Fixture.Swapchain);
			RunFullscreenTests(Fixture.Tasks, *Fixture.Device, *Fixture.Swapchain);
			CheckQueuedGenerations(Fixture);
			CheckReplacementAndRecovery(Fixture);
			CheckOwnedCameraLightRoutes(Fixture);
			CheckOwnedLocalLights(Fixture);
			Fixture.Settings.bClusteredLighting = false;
			CheckOwnedLocalLights(Fixture);
			Fixture.Settings.bClusteredLighting = true;
			CheckOwnedOffscreenShadow(Fixture);
			CheckClusteredMaterialCapacity(Fixture);
		}
		std::cout << "Deferred rendering tests passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
