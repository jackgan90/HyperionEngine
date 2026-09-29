#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/SceneBridge.h"
#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include "Hyperion/Renderer/TransientGeometry.h"
#include "Support/ModelAssetSupport.h"
#include "Support/ShaderSourceSupport.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

using namespace Hyperion;

namespace
{
std::shared_ptr<const FSceneModelData> Quad(EAlphaMode InAlpha, FVec4 InColor, bool bInUnlit = true)
{
	auto Source = std::make_shared<FModelSource>();
	FModelPrimitive Primitive;
	Primitive.Positions = {-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0};
	Primitive.Normals = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
	Primitive.TexCoords0 = {0, 1, 1, 1, 1, 0, 0, 0};
	Primitive.Indices = {0, 1, 2, 0, 2, 3};
	Primitive.Material = 0;
	FModelMaterial Material;
	Material.bUnlit = bInUnlit;
	Material.bDoubleSided = true;
	Material.BaseColor = InColor;
	Material.AlphaMode = InAlpha;
	if (InAlpha == EAlphaMode::Mask)
	{
		FModelImage Image{"Coverage", 2, 1};
		Image.Rgba = {255, 255, 255, 255, 255, 255, 255, 0};
		Source->Images.push_back(Image);
		Material.BaseColorTexture = {0, -1, 0};
	}
	Source->Materials.push_back(Material);
	Source->Primitives.push_back(Primitive);
	FModelNode Node;
	Node.Primitives = {0};
	Source->Nodes.push_back(Node);
	Source->Roots = {0};
	return PrepareSourceModel(Source);
}

template<class Predicate> void Await(const Predicate& InPredicate)
{
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
	while (!InPredicate())
	{
		HYP_CHECK(std::chrono::steady_clock::now() < Deadline);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

struct FFixture
{
	FTaskSystem Tasks{2, 2};
	FWindow Window{"Transient material regression", {320, 240}, true};
	FShaderCompiler Compiler{TestShaderRoot(), "transient-material-cache"};
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::unique_ptr<FRenderSession> Session;
	FScene Scene;
	std::unique_ptr<FSceneRenderBridge> Bridge;
	std::unique_ptr<FSceneRenderPipeline> Pipeline;
	FSceneViewRequest View;
	FScenePipelineSettings Settings;

	FFixture()
	{
		const auto Surface = Window.Surface();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          FRHIBackendRegistry Registry;
			                          RegisterD3D12RHIBackend(Registry);
			                          Device = Registry.CreateDevice(ERHIBackend::D3D12);
			                          Swapchain = Device->CreateSwapchain({Surface, {320, 240}, ERHIDepthFormat::D32});
		                          }));
		Session = std::make_unique<FRenderSession>(Tasks, *Device, Compiler);
		Bridge = std::make_unique<FSceneRenderBridge>(Scene, *Session, Tasks);
		FRenderFeatureList Features;
		Features.push_back(MakeTransientGeometryFeature());
		Pipeline =
		    std::make_unique<FSceneRenderPipeline>(*Session, Device->GetCapabilities(), Settings, std::move(Features));
		View.Width = 320;
		View.Height = 240;
		View.CameraOverride = FSceneCameraView{FSceneCamera{}, SceneCameraTransform({0, 0, 5}, {})};
	}

	~FFixture()
	{
		Pipeline.reset();
		Bridge.reset();
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

	FSceneHandle Add(std::shared_ptr<const FSceneModelData> InData, FMat4 InWorld = Identity())
	{
		FSceneModel Model;
		Model.Data = std::move(InData);
		Model.World = InWorld;
		const auto Handle = Scene.Add(std::move(Model));
		Await(
		    [&]
		    {
			    Bridge->Flush();
			    HYP_CHECK(Bridge->GetError(Handle).empty());
			    return Bridge->IsReady(Handle);
		    });
		return Handle;
	}

	FImage Frame(std::shared_ptr<const FTransientGeometry> InPreview = {})
	{
		Window.Poll();
		Bridge->Flush();
		const auto Seed = Session->FreezeSceneFrame(Bridge->GetToken());
		FImage Image;
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          Pipeline->Configure(Settings);
			                          Pipeline->SetTransientGeometry(InPreview);
			                          FRenderGraph Graph;
			                          FCascadedShadowSettings Shadows;
			                          Shadows.bEnabled = false;
			                          Pipeline->Build(Graph, View, Seed, Shadows, {.04f, .04f, .04f, 1}, {}, true);
			                          Image = ExecuteGraph(std::move(Graph), Tasks, *Swapchain,
			                                               {View.Width, View.Height}, false, true);
		                          }));
		return Image;
	}

	std::shared_ptr<FTransientGeometry> Preview(const std::shared_ptr<const FSceneModelData>& InData)
	{
		auto Resource = Session->GetResources().RequestModel(InData);
		Await(
		    [&]
		    {
			    HYP_CHECK(Resource->GetStatus() != ERenderResourceStatus::Failed);
			    return Resource->GetMaterial(0) &&
			           Resource->GetMaterial(0)->GetStatus() == ERenderMaterialStatus::Ready;
		    });
		auto Result = std::make_shared<FTransientGeometry>();
		Result->Lifetime = Session->GetResources().CreateScopeLifetime();
		FRenderPrimitiveState State;
		State.Resource = Resource;
		Result->AddModelInstance(State, {});
		HYP_CHECK(Result->Items.empty() && Result->SceneItems.size() == 1);
		return Result;
	}
};

bool Same(const FImage& InA, const FImage& InB)
{
	HYP_CHECK(InA.Rgba.size() == InB.Rgba.size());
	for (std::size_t Index = 0; Index < InA.Rgba.size(); ++Index)
	{
		if (std::abs(InA.Rgba[Index] - InB.Rgba[Index]) > 1.f / 255)
		{
			return false;
		}
	}
	return true;
}

void CheckEquivalence(FFixture& InFixture, EAlphaMode InAlpha, bool bInUnlit = true)
{
	const auto Data = Quad(InAlpha, {.9f, .08f, .03f, InAlpha == EAlphaMode::Blend ? .5f : 1.f}, bInUnlit);
	const auto Preview = InFixture.Preview(Data);
	const auto Baseline = InFixture.Frame();
	const auto Image = InFixture.Frame(Preview);
	HYP_CHECK(!Same(Image, Baseline));
	HYP_CHECK(Same(InFixture.Frame(), Baseline));
	const auto Object = InFixture.Add(Data);
	const auto Placed = InFixture.Frame();
	HYP_CHECK(Same(Image, Placed));
	// A publication frame may contain formal bindings and the previous preview simultaneously.
	Preview->ReplacedPrimitives = InFixture.Bridge->ResolveRenderPrimitives(Object);
	HYP_CHECK(!Preview->ReplacedPrimitives.empty());
	HYP_CHECK(Same(InFixture.Frame(Preview), Placed));
	HYP_CHECK(Same(InFixture.Frame(), Placed));
	Preview->ReplacedPrimitives.clear();
	InFixture.Scene.Remove(Object);
	HYP_CHECK(Same(InFixture.Frame(), Baseline));
	// Moving preview snapshots must not reuse a previous transform or persist after cancellation.
	Preview->SceneItems[0].World = Translation({1, 0, 0});
	HYP_CHECK(!Same(InFixture.Frame(Preview), Image));
	HYP_CHECK(Same(InFixture.Frame(), Baseline));
}
} // namespace

int main()
{
	try
	{
		FFixture Fixture;
		auto Sun = MakeSceneDirectionalLightNode("Preview lighting");
		Sun.Local() = SceneCameraTransform({}, {0, 0, -1});
		Sun.DirectionalLight()->Intensity = 2;
		Sun.DirectionalLight()->bCastShadows = false;
		Fixture.Scene.AddNode(Sun);
		const auto Back = Fixture.Add(Quad(EAlphaMode::Opaque, {.03f, .1f, .8f, 1}), Translation({0, 0, -1}));
		const auto Glass = Fixture.Add(Quad(EAlphaMode::Blend, {.1f, .8f, .03f, .4f}), Translation({0, 0, .5f}));
		for (const auto Pipeline : {ESceneRenderPipeline::Forward, ESceneRenderPipeline::Deferred})
		{
			Fixture.Settings.Pipeline = Pipeline;
			for (const auto Depth : {EDepthConvention::Standard, EDepthConvention::Reversed})
			{
				Fixture.View.DepthConvention = Depth;
				for (const auto Alpha : {EAlphaMode::Opaque, EAlphaMode::Mask, EAlphaMode::Blend})
				{
					CheckEquivalence(Fixture, Alpha);
				}
				CheckEquivalence(Fixture, EAlphaMode::Opaque, false);
			}
		}
		Fixture.Scene.Remove(Glass);
		Fixture.Scene.Remove(Back);
		Fixture.Frame();
		Fixture.Tasks.Wait(Fixture.Tasks.Dispatch({EDomain::Rhi, 0},
		                                          [&]
		                                          {
			                                          Fixture.Device->WaitIdle();
			                                          HYP_CHECK(Fixture.Device->Statistics().ValidationErrors == 0);
		                                          }));
		std::cout << "Transient source materials match placed models; GPU validation errors=0\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
