#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/ForwardRenderPipeline.h"
#include "Hyperion/Renderer/Model.h"
#include "Support/ModelAssetSupport.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

using namespace Hyperion;

namespace
{
std::shared_ptr<const FModelSource> Plane(EAlphaMode InAlpha = EAlphaMode::Opaque)
{
	FModelSource Asset;
	FModelPrimitive Primitive;
	Primitive.Positions = {-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0};
	Primitive.Normals = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
	Primitive.TexCoords0 = {0, 0, 0, 0, 0, 0, 0, 0};
	Primitive.TexCoords1 = {0, 1, 1, 1, 1, 0, 0, 0};
	Primitive.Colors = {1, 1, 1, .75f, 1, 1, 1, .75f, 1, 1, 1, .75f, 1, 1, 1, .75f};
	Primitive.Indices = {0, 1, 2, 0, 2, 3};
	Primitive.Material = 0;
	Asset.Primitives.push_back(Primitive);
	FModelMaterial Material;
	Material.BaseColor = {.8f, .8f, .8f, .8f};
	Material.Metallic = 0;
	Material.Roughness = 1;
	Material.AlphaMode = InAlpha;
	if (InAlpha == EAlphaMode::Mask)
	{
		FModelImage Image{"Alpha halves", 8, 8};
		for (unsigned Y = 0; Y < 8; ++Y)
		{
			for (unsigned X = 0; X < 8; ++X)
			{
				Image.Rgba.insert(Image.Rgba.end(), {255, 255, 255, static_cast<std::uint8_t>(X < 4 ? 255 : 0)});
			}
		}
		Asset.Images.push_back(std::move(Image));
		Material.BaseColorTexture = {0, -1, 1}; // UV1 must be shared by forward and shadow permutations.
	}
	Asset.Materials.push_back(Material);
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

struct FShadowFixture
{
	FTaskSystem Tasks{2, 2};
	FWindow Window{"Cascaded shadow regression", {384, 288}, true};
	FShaderCompiler Compiler{std::filesystem::path(HYP_SOURCE_DIR) / "shaders", "shadow-render-cache"};
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::unique_ptr<FRenderSession> Session;
	std::unique_ptr<FForwardRenderPipeline> Pipeline;
	FRenderView View;
	FCascadedShadowSettings Settings;
	FForwardPipelineStatistics Statistics;
	FDeviceStats DeviceStats;

	explicit FShadowFixture(ERHIDepthFormat InDepth = ERHIDepthFormat::D32,
	                        EDepthConvention InConvention = EDepthConvention::Standard)
	{
		View.DepthConvention = InConvention;
		const auto Surface = Window.Surface();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          FRHIBackendRegistry Registry;
			                          RegisterD3D12RHIBackend(Registry);
			                          Device = Registry.CreateDevice(ERHIBackend::D3D12);
			                          Swapchain = Device->CreateSwapchain(
			                              {Surface, {384, 288}, InDepth, GetDepthClearValue(InConvention)});
			                          Swapchain->SetGpuTimingEnabled(true);
		                          }));
		Session = std::make_unique<FRenderSession>(Tasks, *Device, Compiler, InDepth, GetStandardMaterialSemantics());
		Pipeline = std::make_unique<FForwardRenderPipeline>(*Session);
		View.Eye = {0, 0, 10};
		View.Width = 384;
		View.Height = 288;
		View.Camera = FRenderCamera{{0, 0, -1}, {0, 1, 0}, 1, .1f, 40};
		View.ViewProjection =
		    Multiply(Perspective(1, 4.f / 3, .1f, 40, View.DepthConvention), LookAt(View.Eye, {0, 0, 0}));
		Settings.Resolution = 1024;
		Settings.Distance = 18;
	}

	~FShadowFixture()
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

	FImage Frame(bool bInEnabled, FVec3 InLight = {1, 0, 1}, bool bInRawLight = true)
	{
		Window.Poll();
		Tasks.Wait(Session->GetScene().Flush());
		Settings.bEnabled = bInEnabled;
		FMaterialParameterValues Values{
		    {"Engine.Scene.MainDirectionalLightColor", FMaterialValue::Float(FVec3{3, 3, 3})},
		    {"Engine.Scene.AmbientColor", FMaterialValue::Float(FVec3{.2f, .2f, .2f})}};
		if (bInRawLight)
		{
			Values.push_back({"Engine.Scene.MainDirectionalLightDirection", FMaterialValue::Float(Normalize(InLight))});
		}
		Session->SetSceneParameters(std::move(Values));
		const auto Frame = Session->FreezeFrame();
		FImage Image;
		Tasks.Wait(Tasks.Dispatch(
		    {EDomain::Render},
		    [&]
		    {
			    FRenderGraph Graph;
			    Pipeline->Build(Graph, View, Frame, Settings, {0, 0, 0, 1});
			    std::vector<FPassCommands> Plan;
			    Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
			                              [&]
			                              {
				                              Plan = Graph.Compile();
			                              }));
			    HYP_CHECK(Plan.size() <= 8);
			    for (const auto& Pass : Plan)
			    {
				    if (Pass.HasDepth())
				    {
					    HYP_CHECK(Pass.DepthStencil->ClearDepth == GetDepthClearValue(View.DepthConvention));
				    }
			    }
			    if (Pipeline->Statistics().bShadows)
			    {
				    for (unsigned Index = 0; Index < 4; ++Index)
				    {
					    HYP_CHECK(!Plan[Index].HasColor() &&
					              Plan[Index].DepthStencil->Depth->Load == EAttachmentLoad::Clear &&
					              Plan[Index].GetDepthTexture());
					    HYP_CHECK(Plan[Index].Draws.empty() || Plan[Index].Draws[0].Pipeline);
				    }
				    HYP_CHECK(Plan[4].HasColor() && Plan[4].SampledTextures.size() == 4);
			    }
			    Statistics = Pipeline->Statistics();
			    for (const auto& Stats : Statistics.Views)
			    {
				    HYP_CHECK(Stats.Visibility.Batches.FailedItems == 0);
			    }
			    Image = ExecuteGraph(Graph, Tasks, *Swapchain, {384, 288}, false, true);
			    Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
			                              [&]
			                              {
				                              Device->CollectCompletedResources();
				                              DeviceStats = Device->Statistics();
				                              HYP_CHECK(DeviceStats.ValidationErrors == 0);
			                              }));
		    }));
		return Image;
	}

	float Pixel(const FImage& InImage, FVec3 InWorld) const
	{
		const auto Clip = Transform(View.ViewProjection, {InWorld.X, InWorld.Y, InWorld.Z, 1});
		const auto X = static_cast<unsigned>((Clip.X / Clip.W * .5f + .5f) * InImage.Width);
		const auto Y = static_cast<unsigned>((.5f - Clip.Y / Clip.W * .5f) * InImage.Height);
		return InImage.Rgba.at((Y * InImage.Width + X) * 4);
	}
};

void CheckOffscreenAndRemoval(FShadowFixture& InFixture)
{
	FSourceModel Caster(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Plane());
	Caster.SetTransform(Multiply(Translation({8, 0, 3}), Scale({.75f, .75f, 1})));
	Await(Caster);
	const auto Lit = InFixture.Frame(false, {8, 0, 3});
	const auto Shadowed = InFixture.Frame(true, {8, 0, 3});
	HYP_CHECK(InFixture.Statistics.Views.back().Visibility.VisibleItems == 1);
	HYP_CHECK(InFixture.Pixel(Lit, {}) - InFixture.Pixel(Shadowed, {}) > .12f);
	bool bFoundCaster{};
	for (const auto& View : InFixture.Statistics.Views)
	{
		bFoundCaster |= View.Usage == "ShadowDepth" && View.Visibility.VisibleItems > 1;
	}
	HYP_CHECK(bFoundCaster);
	Caster.Remove();
	const auto Removed = InFixture.Frame(true, {8, 0, 3});
	HYP_CHECK(std::abs(InFixture.Pixel(Lit, {}) - InFixture.Pixel(Removed, {})) < .012f);
}

void CheckMaskAndMirroring(FShadowFixture& InFixture)
{
	const auto Asset = Plane(EAlphaMode::Mask);
	FSourceModel Caster(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Asset);
	Caster.SetTransform(Translation({0, 0, 2}));
	Await(Caster);
	const auto Program = Caster.GetResource()->GetMaterial(0)->GetCompiled();
	HYP_CHECK(Program->FindInstancePass("ShadowDepth"));
	const auto Lit = InFixture.Frame(false);
	const auto Shadowed = InFixture.Frame(true);
	const FVec3 Left{-2.4f, .3f, 0};
	const FVec3 Right{-1.6f, .3f, 0};
	HYP_CHECK(InFixture.Pixel(Lit, Left) - InFixture.Pixel(Shadowed, Left) > .15f);
	HYP_CHECK(std::abs(InFixture.Pixel(Lit, Right) - InFixture.Pixel(Shadowed, Right)) < .025f);
	Caster.SetTransform(Multiply(Translation({0, 0, 2}), Scale({-1, 1, 1})));
	const auto Mirrored = InFixture.Frame(true);
	HYP_CHECK(InFixture.Pixel(Lit, Right) - InFixture.Pixel(Mirrored, Right) > .15f);
	HYP_CHECK(std::abs(InFixture.Pixel(Lit, Left) - InFixture.Pixel(Mirrored, Left)) < .025f);
	FMaterialOverride Override;
	Override.BaseColor = FVec4{.8f, .8f, .8f, .4f};
	Caster.SetMaterial(Override);
	const auto Clipped = InFixture.Frame(true);
	HYP_CHECK(std::abs(InFixture.Pixel(Lit, Right) - InFixture.Pixel(Clipped, Right)) < .025f);
	Caster.Remove();
}

void CheckBlendAndBatching(FShadowFixture& InFixture)
{
	FSourceModel Blend(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Plane(EAlphaMode::Blend));
	Blend.SetTransform(Translation({0, 0, 2}));
	Await(Blend);
	const auto Lit = InFixture.Frame(false);
	const auto Enabled = InFixture.Frame(true);
	HYP_CHECK(std::abs(InFixture.Pixel(Lit, {-2, 0, 0}) - InFixture.Pixel(Enabled, {-2, 0, 0})) < .025f);
	Blend.Remove();
	const auto Asset = Plane();
	FSourceModel A(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Asset);
	FSourceModel B(InFixture.Session->GetScene(), InFixture.Session->GetResources(), Asset);
	A.SetTransform(Translation({0, -1, 2}));
	B.SetTransform(Translation({0, 1, 2}));
	Await(A);
	Await(B);
	HYP_CHECK(A.GetResource()->GetMaterial(0)->GetCompiled()->FindInstancePass("ShadowDepth"));
	const auto Batched = InFixture.Frame(true);
	std::size_t Instanced{};
	for (const auto& View : InFixture.Statistics.Views)
	{
		if (View.Usage == "ShadowDepth")
		{
			Instanced += View.Visibility.Batches.InstancedItems;
		}
	}
	HYP_CHECK(Instanced > 0);
	InFixture.View.bInstanceBatching = false;
	const auto Ordinary = InFixture.Frame(true);
	HYP_CHECK(Ordinary.Rgba == Batched.Rgba);
	InFixture.View.bInstanceBatching = true;
	InFixture.Frame(true);
	const auto Before = InFixture.DeviceStats;
	for (unsigned Index = 0; Index < 20; ++Index)
	{
		InFixture.View.Eye.X = float(Index) * .002f;
		InFixture.View.ViewProjection = Multiply(Perspective(1, 4.f / 3, .1f, 40, InFixture.View.DepthConvention),
		                                         LookAt(InFixture.View.Eye, {InFixture.View.Eye.X, 0, 0}));
		InFixture.Frame(true, {1, .1f * std::sin(float(Index) * .1f), 1});
	}
	HYP_CHECK(InFixture.DeviceStats.PipelinesCreated == Before.PipelinesCreated);
	HYP_CHECK(InFixture.DeviceStats.DescriptorAllocations == Before.DescriptorAllocations);
	HYP_CHECK(InFixture.DeviceStats.GpuAllocationBytes <= Before.GpuAllocationBytes + 8 * 1024 * 1024);
	HYP_CHECK(InFixture.DeviceStats.GpuTiming.Passes.size() == 6);
	HYP_CHECK(InFixture.DeviceStats.GpuTiming.Passes.front().Name == "Shadow cascade 0/0");
	A.Remove();
	B.Remove();
}

void CheckResolutionAndPreview(FShadowFixture& InFixture)
{
	InFixture.Frame(true);
	const auto Small = InFixture.DeviceStats.GpuAllocationBytes;
	for (unsigned Iteration = 0; Iteration < 3; ++Iteration)
	{
		InFixture.Settings.Resolution = 2048;
		InFixture.Frame(true);
		HYP_CHECK(InFixture.Statistics.ShadowTextureBytes == 64 * 1024 * 1024);
		InFixture.Settings.Resolution = 1024;
		for (unsigned Frame = 0; Frame < 4; ++Frame)
		{
			InFixture.Frame(true);
		}
		HYP_CHECK(InFixture.Statistics.ShadowTextureBytes == 16 * 1024 * 1024);
		HYP_CHECK(InFixture.DeviceStats.GpuAllocationBytes <= Small + 8 * 1024 * 1024);
	}
	InFixture.Settings.DebugMode = 2;
	const auto Preview = InFixture.Frame(true);
	// The pane displays raw depth, including the convention's far clear value.
	HYP_CHECK(std::abs(Preview.Rgba.at((280 * Preview.Width + 380) * 4) -
	                   GetDepthClearValue(InFixture.View.DepthConvention)) < .01f);
	const auto Before = InFixture.DeviceStats.DescriptorAllocations;
	InFixture.Frame(true);
	HYP_CHECK(InFixture.DeviceStats.DescriptorAllocations == Before);
	InFixture.Settings.DebugMode = 0;
}

void CheckIdleAfterMotion(FShadowFixture& InFixture)
{
	InFixture.View.Eye.X += .01f;
	InFixture.View.ViewProjection = Multiply(Perspective(1, 4.f / 3, .1f, 40, InFixture.View.DepthConvention),
	                                         LookAt(InFixture.View.Eye, {InFixture.View.Eye.X, 0, 0}));
	InFixture.Frame(true, {1, .1f, 1});
	// All captured GPU work is complete. Stable live resources must not keep scheduling retirement polls.
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	const auto Before = InFixture.Tasks.Statistics();
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	const auto After = InFixture.Tasks.Statistics();
	HYP_CHECK(Before.size() == After.size());
	for (std::size_t Index = 0; Index < Before.size(); ++Index)
	{
		HYP_CHECK(After[Index].Executed <= Before[Index].Executed + 2);
	}
}

void CheckHiddenPreviewRetirement(FShadowFixture& InFixture)
{
	for (const auto Resolution : {2048u, 1024u})
	{
		InFixture.Settings.DebugMode = 2;
		InFixture.Frame(true);
		InFixture.Settings.DebugMode = 0;
		InFixture.Settings.Resolution = Resolution;
		CheckIdleAfterMotion(InFixture);
	}
}

void CheckProviderAndDepthFormat()
{
	FShadowFixture Fixture(ERHIDepthFormat::D32S8);
	Fixture.Session->GetProviders().Register({"Engine.Scene.MainDirectionalLightDirection",
	                                          MaterialScopeBit(EMaterialScope::Scene),
	                                          [](const FMaterialProviderInputs&) -> std::optional<FMaterialValue>
	                                          {
		                                          return FMaterialValue::Float(Normalize(FVec3{8, 0, 3}));
	                                          }});
	FSourceModel Receiver(Fixture.Session->GetScene(), Fixture.Session->GetResources(), Plane());
	Receiver.SetTransform(Scale({4, 3, 1}));
	Await(Receiver);
	FSourceModel Caster(Fixture.Session->GetScene(), Fixture.Session->GetResources(), Plane());
	Caster.SetTransform(Multiply(Translation({8, 0, 3}), Scale({.75f, .75f, 1})));
	Await(Caster);
	const auto Lit = Fixture.Frame(false);
	const auto Shadowed = Fixture.Frame(true, {8, 0, 3});
	const auto Matrices = Fixture.Pipeline->Shadows().Cascades();
	HYP_CHECK(Fixture.Pixel(Lit, {}) - Fixture.Pixel(Shadowed, {}) > .12f);
	for (const auto bRaw : {true, false})
	{
		const auto Overridden = Fixture.Frame(true, {0, 0, 1}, bRaw);
		HYP_CHECK(Overridden.Rgba == Shadowed.Rgba);
		for (unsigned Index = 0; Index < 4; ++Index)
		{
			HYP_CHECK(Matrices[Index].ViewProjection.Values ==
			          Fixture.Pipeline->Shadows().Cascades()[Index].ViewProjection.Values);
		}
	}
}

void CheckLocalLightProvider()
{
	for (const auto Scope : {EMaterialScope::View, EMaterialScope::Pass, EMaterialScope::Material,
	                         EMaterialScope::Object, EMaterialScope::Draw})
	{
		FShadowFixture Fixture;
		Fixture.Session->GetProviders().Register({"Engine.Scene.MainDirectionalLightDirection",
		                                          MaterialScopeBit(EMaterialScope::Scene) | MaterialScopeBit(Scope),
		                                          [](const FMaterialProviderInputs&) -> std::optional<FMaterialValue>
		                                          {
			                                          return FMaterialValue::Float(FVec3{0, 0, 1});
		                                          }});
		FSourceModel Model(Fixture.Session->GetScene(), Fixture.Session->GetResources(), Plane());
		Await(Model);
		Fixture.Frame(true);
		HYP_CHECK(!Fixture.Statistics.bShadows && Fixture.Statistics.Views.size() == 1);
	}
}

struct FCountingShadowStrategy final : IRenderBatchStrategy
{
	mutable unsigned ShadowEvaluations{};
	FInstanceBatchStrategy Delegate;

	FRenderBatchDecision Evaluate(const FRenderBatchCandidate& InCandidate,
	                              const FRHICapabilities& InCapabilities) const override
	{
		ShadowEvaluations += InCandidate.Signature.Structure->Target.ColorCount == 0;
		return Delegate.Evaluate(InCandidate, InCapabilities);
	}

	bool CanCombine(const FRenderBatchCandidate& InA, const FRenderBatchCandidate& InB) const override
	{
		return Delegate.CanCombine(InA, InB);
	}
};

void CheckStableShadowPlans()
{
	FShadowFixture Fixture;
	auto Strategy = std::make_unique<FCountingShadowStrategy>();
	auto& Counts = *Strategy;
	Fixture.Session->GetBatchSystem().Register(std::move(Strategy));
	const auto Asset = Plane();
	FSourceModel A(Fixture.Session->GetScene(), Fixture.Session->GetResources(), Asset);
	FSourceModel B(Fixture.Session->GetScene(), Fixture.Session->GetResources(), Asset);
	Await(A);
	Await(B);
	Fixture.Frame(true, {0, 0, 1});
	Fixture.Frame(true, {0, 0, 1});
	const auto Before = Fixture.Pipeline->Shadows().Cascades();
	for (unsigned Frame = 0; Frame < 5; ++Frame)
	{
		Counts.ShadowEvaluations = 0;
		Fixture.View.Eye.X += .00001f;
		Fixture.View.ViewProjection = Multiply(Perspective(1, 4.f / 3, .1f, 40, Fixture.View.DepthConvention),
		                                       LookAt(Fixture.View.Eye, {Fixture.View.Eye.X, 0, 0}));
		Fixture.Frame(true, {0, 0, 1});
		HYP_CHECK(Counts.ShadowEvaluations == 0);
		for (unsigned Index = 0; Index < 4; ++Index)
		{
			HYP_CHECK(Before[Index].ViewProjection.Values ==
			          Fixture.Pipeline->Shadows().Cascades()[Index].ViewProjection.Values);
			HYP_CHECK(Fixture.Statistics.Views[Index].Visibility.Draws > 0 ||
			          Fixture.Statistics.Views[Index].Visibility.VisibleItems == 0);
			HYP_CHECK(Fixture.Statistics.Views[Index].Visibility.Batches.PackedBytes == 0);
		}
	}
	A.SetTransform(Translation({.1f, 0, 0}));
	Fixture.Frame(true, {0, 0, 1});
	HYP_CHECK(Counts.ShadowEvaluations > 0);
	Counts.ShadowEvaluations = 0;
	A.Remove();
	Fixture.Frame(true, {0, 0, 1});
	HYP_CHECK(Counts.ShadowEvaluations > 0);
}

void CheckTimingCapture(FShadowFixture& InFixture)
{
	for (const auto Capacity : {3u, 2u})
	{
		InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Rhi, 0},
		                                              [&]
		                                              {
			                                              InFixture.Device->BeginGpuTimingCapture(Capacity);
		                                              }));
		const auto First = InFixture.DeviceStats.SubmittedFrames + 1;
		for (unsigned Frame = 0; Frame < 3; ++Frame)
		{
			InFixture.Frame(true);
		}
		InFixture.Tasks.Wait(InFixture.Tasks.Dispatch(
		    {EDomain::Rhi, 0},
		    [&]
		    {
			    InFixture.Device->WaitIdle();
			    const auto Capture = InFixture.Device->EndGpuTimingCapture();
			    HYP_CHECK(Capture.Frames.size() == Capacity && Capture.DroppedFrames == 3 - Capacity);
			    for (unsigned Index = 0; Index < Capacity; ++Index)
			    {
				    HYP_CHECK(Capture.Frames[Index].Frame == First + Index);
				    HYP_CHECK(Capture.Frames[Index].Passes.size() == 6);
				    HYP_CHECK(Capture.Frames[Index].Swapchain == Capture.Frames.front().Swapchain);
			    }
			    HYP_CHECK(InFixture.Device->EndGpuTimingCapture().Frames.empty());
		    }));
	}
}
} // namespace

int main()
{
	try
	{
		CheckProviderAndDepthFormat();
		CheckLocalLightProvider();
		CheckStableShadowPlans();
		for (const auto Convention : {EDepthConvention::Standard, EDepthConvention::Reversed})
		{
			FShadowFixture Fixture(ERHIDepthFormat::D32, Convention);
			FSourceModel Receiver(Fixture.Session->GetScene(), Fixture.Session->GetResources(), Plane());
			Receiver.SetTransform(Scale({4, 3, 1}));
			Await(Receiver);
			CheckOffscreenAndRemoval(Fixture);
			CheckMaskAndMirroring(Fixture);
			CheckBlendAndBatching(Fixture);
			CheckResolutionAndPreview(Fixture);
			CheckIdleAfterMotion(Fixture);
			CheckHiddenPreviewRetirement(Fixture);
			CheckTimingCapture(Fixture);
			for (const auto Direction : {FVec3{0, 1, 0}, FVec3{0, -1, 0}, FVec3{1, 0, 0}, FVec3{0, 0, -1}})
			{
				Fixture.Frame(true, Direction);
			}
			Receiver.Remove();
			Fixture.Frame(true);
			for (const auto& View : Fixture.Statistics.Views)
			{
				HYP_CHECK(View.Visibility.Draws == 0);
			}
		}
		std::cout << "Shadow pixels: offscreen caster, UV1 mask, alpha factors, mirror, blend, removal, instancing, "
		             "motion and empty cascades passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
