#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/RenderGraph.h"
#include "Support/TestSupport.h"
#include <iostream>

namespace
{
using namespace Hyperion;

template<class F> void Rejects(F InOperation)
{
	bool Failed = false;
	try
	{
		InOperation();
	}
	catch (const std::exception&)
	{
		Failed = true;
	}
	HYP_CHECK(Failed);
}

void CheckWindows()
{
	for (bool DestroyFirst : {false, true})
	{
		auto First = std::make_unique<FWindow>("First", FSize{64, 64}, true);
		auto Second = std::make_unique<FWindow>("Second", FSize{64, 64}, true);
		auto& Survivor = DestroyFirst ? Second : First;
		(DestroyFirst ? First : Second).reset();
		HYP_CHECK(Survivor->PixelSize().Width > 0);
		Survivor->Resize({80, 48});
		Survivor->Poll();
		HYP_CHECK(Survivor->LogicalSize().Width == 80);
		FWindow Third("Third", {64, 64}, true);
		HYP_CHECK(Third.PixelSize().Width > 0);
	}
	FWindow Reopened("Reopened after all windows closed", {64, 64}, true);
	HYP_CHECK(Reopened.PixelSize().Width > 0);
}

void CheckAssetRetry(FTaskSystem& InTasks)
{
	auto Files = std::make_shared<FMemoryFileSystem>();
	FIOService IO(InTasks, Files);
	FAssetService Assets(IO);
	RegisterGltfImporter(Assets);
	const auto Root = std::filesystem::absolute("lifecycle-assets").lexically_normal();
	FLocalFileSystem Local;
	const auto FixtureRoot = std::filesystem::path(HYP_SOURCE_DIR) / "out/fixtures";
	const auto Gltf = Local.Read(FixtureRoot / "Showcase.gltf", 1024 * 1024);
	Files->WriteAtomic(Root / "Showcase.gltf", Gltf);
	auto Failed = Assets.LoadAsync<FModelAsset>(Root / "Showcase.gltf");
	Rejects(
	    [&]
	    {
		    Failed.Get(InTasks);
	    });
	// Supply all fixture dependencies after the shared producer has failed.
	for (const auto& File : std::filesystem::directory_iterator(FixtureRoot))
	{
		if (File.is_regular_file())
		{
			Files->WriteAtomic(Root / File.path().filename(), Local.Read(File.path(), 1024 * 1024));
		}
	}
	auto Unrelated = Assets.LoadAsync<FModelAsset>(Root / "Showcase.glb").Get(InTasks);
	const auto Reads = IO.Statistics().Reads.load();
	auto Retry = Assets.LoadAsync<FModelAsset>(Root / "Showcase.gltf");
	auto Shared = Assets.LoadAsync<FModelAsset>(Root / "Showcase.gltf");
	auto Cancelled = Assets.LoadAsync<FModelAsset>(Root / "Showcase.gltf");
	Cancelled.Cancel();
	const auto Model = Retry.Get(InTasks);
	HYP_CHECK(Model == Shared.Get(InTasks));
	HYP_CHECK(IO.Statistics().Reads == Reads + 3);
	HYP_CHECK(Assets.LoadAsync<FModelAsset>(Root / "Showcase.glb").Get(InTasks) == Unrelated);
	Rejects(
	    [&]
	    {
		    Failed.GetReady();
	    });
	Rejects(
	    [&]
	    {
		    Cancelled.GetReady();
	    });
	Assets.Drain();
}

FRenderGraph ClearGraph()
{
	FRenderGraph Graph;
	FColorPass Clear;
	Clear.Commands.Name = "Clear";
	Clear.Load = EColorLoad::Clear;
	Clear.Commands.ClearColor = {.25f, .5f, .75f, 1};
	Graph.Add(Clear);
	return Graph;
}

void CheckFrameRecovery(FTaskSystem& InTasks)
{
	FWindow Window("Frame recovery", {64, 64}, true);
	FRHIBackendRegistry Registry;
	RegisterD3D12RHIBackend(Registry);
	const auto Surface = Window.Surface();
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	InTasks.Wait(InTasks.Dispatch({EDomain::Rhi, 0},
	                              [&]
	                              {
		                              Device = Registry.CreateDevice(ERHIBackend::D3D12);
		                              Swapchain = Device->CreateSwapchain({Surface, {64, 64}});
	                              }));
	const auto Good = ClearGraph();
	auto Bad = ClearGraph();
	FColorPass Invalid;
	Invalid.Commands.Name = "Invalid draw";
	Invalid.Commands.Draws.emplace_back();
	Bad.Add(Invalid);
	for (int Attempt = 0; Attempt < 3; ++Attempt)
	{
		Rejects(
		    [&]
		    {
			    InTasks.Wait(InTasks.Dispatch({EDomain::Render},
			                                  [&]
			                                  {
				                                  ExecuteGraph(Bad, InTasks, *Swapchain, {64, 64}, false, false);
			                                  }));
		    });
		InTasks.Wait(InTasks.Dispatch({EDomain::Render},
		                              [&]
		                              {
			                              const auto Image =
			                                  ExecuteGraph(Good, InTasks, *Swapchain, {64, 64}, false, true);
			                              HYP_CHECK(Image.Width == 64 && std::abs(Image.Rgba[0] - .25f) < .01f);
		                              }));
	}
	InTasks.Wait(InTasks.Dispatch({EDomain::Rhi, 0},
	                              [&]
	                              {
		                              Swapchain->CancelFrame();
		                              Swapchain->BeginFrame({80, 48});
		                              const auto Plan = Good.Compile();
		                              const auto Abandoned = Swapchain->Record(0, Plan[0]);
		                              Swapchain->CancelFrame();
		                              Swapchain->CancelFrame();
		                              Swapchain->BeginFrame({80, 48});
		                              const std::array OldLists{Abandoned};
		                              Rejects(
		                                  [&]
		                                  {
			                                  Swapchain->EndFrame(OldLists, false);
		                                  });
		                              Swapchain->CancelFrame();
		                              Device->WaitIdle();
		                              HYP_CHECK(Device->Statistics().ValidationErrors == 0);
		                              Swapchain.reset();
		                              Device.reset();
	                              }));
}
} // namespace

int main()
{
	try
	{
		CheckWindows();
		FTaskSystem Tasks(1, 2);
		CheckAssetRetry(Tasks);
		CheckFrameRecovery(Tasks);
		Tasks.Shutdown();
		std::cout << "Window lifetime, asset retry and frame recovery passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
