#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/SceneViewer/SceneViewerPlugin.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <iostream>
#include <thread>

namespace
{
using namespace Hyperion;

struct FViewerFixture
{
	FTaskSystem Tasks{1, 1};
	FIOService IO{Tasks};
	FAssetService Assets{IO};
	FWindow Window{"Scene controls", {640, 480}, true};
	FShaderCompiler Compiler{std::filesystem::path(HYP_SOURCE_DIR) / "shaders",
	                         std::filesystem::path(HYP_SOURCE_DIR) / "out/shader-cache"};
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::unique_ptr<FRenderSession> Session;
	std::unique_ptr<FSceneViewerPlugin> Plugin;
	FRenderFrame Frame{{640, 480}, {}};
	FSceneVisibilityStats Statistics;
	FImage Image;

	FViewerFixture()
	{
		RegisterGltfImporter(Assets);
		RegisterSceneManifestLoader(Assets);
		const auto Surface = Window.Surface();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          FRHIBackendRegistry Registry;
			                          RegisterD3D12RHIBackend(Registry);
			                          Device = Registry.CreateDevice(ERHIBackend::D3D12);
			                          Swapchain = Device->CreateSwapchain({Surface, {640, 480}});
		                          }));
		Session = std::make_unique<FRenderSession>(Tasks, *Device, Compiler);
		const std::string Text =
		    R"({"type":"hyperion.scene","schema_version":1,"assets":[{"id":"a","path":")" +
		    (std::filesystem::path(HYP_SOURCE_DIR) / "assets/Models/Showcase.gltf").generic_string() +
		    R"("}],"instances":[{"id":"one","asset":"a"}],"camera":{"eye":[0,1,7],"target":[0,0,0]}})";
		const auto Bytes = std::as_bytes(std::span(Text));
		IO.WriteAsync("SceneControls.json", {Bytes.begin(), Bytes.end()}).Get(Tasks);
		Plugin = std::make_unique<FSceneViewerPlugin>(*Session, Tasks, Assets, "SceneControls.json");
		Frame.View.Width = 640;
		Frame.View.Height = 480;
		Plugin->Start();
	}

	~FViewerFixture()
	{
		Plugin->Stop();
		Plugin.reset();
		Assets.Drain();
		Session->Close();
		Session.reset();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Swapchain.reset();
			                          Device.reset();
		                          }));
	}

	void Tick()
	{
		Window.Poll();
		Frame.View.CullingViewProjection.reset();
		Plugin->Update(Frame);
		HYP_CHECK(Plugin->Error().empty());
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          FRenderGraph Graph;
			                          FColorPass Clear;
			                          Clear.Load = EColorLoad::Clear;
			                          Clear.Commands.Name = "Clear";
			                          Graph.Add(Clear);
			                          Session->Build(Graph, Frame.View);
			                          Statistics = Session->Statistics();
			                          Image = ExecuteGraph(Graph, Tasks, *Swapchain, {640, 480}, false, true);
		                          }));
	}
};

void CheckControls(FViewerFixture& InFixture)
{
	auto& Plugin = *InFixture.Plugin;
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (!Plugin.Ready() && std::chrono::steady_clock::now() < Deadline)
	{
		InFixture.Tick();
	}
	HYP_CHECK(Plugin.Ready());
	const auto Original = InFixture.Image.Rgba;
	const auto Count = InFixture.Statistics.VisibleItems;
	HYP_CHECK(Count > 0);
	Plugin.ToggleSelected();
	InFixture.Tick();
	HYP_CHECK(InFixture.Statistics.VisibleItems == 0 && InFixture.Image.Rgba != Original);
	Plugin.ToggleSelected();
	Plugin.MoveSelected(100);
	InFixture.Tick();
	HYP_CHECK(InFixture.Statistics.CollectedPrimitives == 0);
	Plugin.MoveSelected(-100);
	InFixture.Tick();
	HYP_CHECK(InFixture.Image.Rgba == Original);
	Plugin.DuplicateSelected();
	InFixture.Tick();
	HYP_CHECK(Plugin.ModelCount() == 2 && InFixture.Statistics.Groups == 2);
	Plugin.RemoveSelected();
	InFixture.Tick();
	HYP_CHECK(Plugin.ModelCount() == 1 && InFixture.Image.Rgba == Original);
	Plugin.SetFrozen(true);
	FInputEvent Right;
	Right.Type = EEventType::Key;
	Right.Key = EKey::Right;
	Right.bDown = true;
	for (unsigned Index = 0; Index < 30; ++Index)
	{
		Plugin.Input({&Right, 1}, false, false);
	}
	InFixture.Tick();
	HYP_CHECK(InFixture.Statistics.VisibleItems == Count && InFixture.Image.Rgba != Original);
	Plugin.SetFrozen(false);
	InFixture.Tick();
	HYP_CHECK(InFixture.Statistics.CollectedPrimitives == 0);
	Plugin.Fit();
	InFixture.Tick();
	HYP_CHECK(InFixture.Statistics.VisibleItems == Count);
	Plugin.RemoveSelected();
	InFixture.Tick();
	HYP_CHECK(Plugin.ModelCount() == 0 && InFixture.Statistics.Groups == 0);
	Plugin.AddModel();
	InFixture.Tick();
	HYP_CHECK(Plugin.ModelCount() == 1);
}
} // namespace

int main()
{
	try
	{
		FViewerFixture Fixture;
		CheckControls(Fixture);
		std::cout
		    << "Scene Viewer visible hide/move/duplicate/remove, frozen view, fit and empty-scene recovery passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
