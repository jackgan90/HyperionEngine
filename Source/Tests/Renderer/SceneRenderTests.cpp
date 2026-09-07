#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/Model.h"
#include "Hyperion/Renderer/RenderSession.h"
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
	                         std::filesystem::path(HYP_SOURCE_DIR) / "out/shader-cache"};
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::unique_ptr<FRenderSession> Session;

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

	FImage Frame(std::size_t InExpectedItems)
	{
		Window.Poll();
		FImage Image;
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
			    HYP_CHECK(Session->Build(Graph, View) == InExpectedItems);
			    HYP_CHECK(Graph.Compile().size() <= 3);
			    Image = ExecuteGraph(Graph, Tasks, *Swapchain, {320, 240}, false, true);
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
