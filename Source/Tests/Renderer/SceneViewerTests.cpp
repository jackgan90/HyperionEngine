#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/SceneViewer/SceneViewerPlugin.h"
#include "Support/GraphTestSupport.h"
#include "Support/NativeAssetSupport.h"
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
	FIOService IO{Tasks, std::make_shared<FNativeOnlyFileSystem>()};
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
		RegisterSceneAssetTypes(Assets.Types());
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
		FSceneManifest Manifest;
		Manifest.Assets = {
		    {"a",
		     {"", (std::filesystem::path(HYP_SOURCE_DIR) / "out/content/Models/Showcase.hasset").generic_string(),
		      RecordType<FModelAsset>().Id, ""}}};
		Manifest.Instances = {{"one", "a"}};
		Manifest.Eye = {0, 1, 7};
		Assets.SaveAsync("SceneControls.hasset", std::make_shared<const FSceneManifest>(Manifest)).Get(Tasks);
		Plugin = std::make_unique<FSceneViewerPlugin>(*Session, Tasks, Assets, "SceneControls.hasset");
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
			                          auto Clear = MakeColorPass(Graph, "pending");
			                          Clear.Color->Actions.Load = EAttachmentLoad::Clear;
			                          Clear.Name = "Clear";
			                          Graph.Add(Clear);
			                          Session->Build(Graph, Frame.View, Session->FrameTargets());
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

void CheckSaveReload(FViewerFixture& InFixture)
{
	auto& Plugin = *InFixture.Plugin;
	Plugin.MoveSelected(1.25f);
	Plugin.DuplicateSelected();
	Plugin.ToggleSelected();
	const auto ReadyDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	do
	{
		InFixture.Tick();
	} while (!Plugin.Ready() && std::chrono::steady_clock::now() < ReadyDeadline);
	HYP_CHECK(Plugin.Ready());
	InFixture.Tick();
	HYP_CHECK(InFixture.Statistics.VisibleItems > 0);
	const auto Before = InFixture.Image.Rgba;
	const auto Output = std::filesystem::absolute("scene-save/subdirectory/edited.hasset");
	Plugin.SaveAsync(Output).Get(InFixture.Tasks);
	InFixture.Tick();
	HYP_CHECK(Plugin.SaveStatus().find("Saved scene:") == 0);
	const auto Saved = InFixture.Assets.LoadAsync<FSceneManifest>(Output).Get(InFixture.Tasks);
	HYP_CHECK(Saved->Instances.size() == 2 && Saved->Instances[0].Id != Saved->Instances[1].Id);
	HYP_CHECK(!Saved->Instances[1].bVisible);
	HYP_CHECK(Saved->Eye.X == InFixture.Frame.View.Eye.X && Saved->Eye.Y == InFixture.Frame.View.Eye.Y);
	Plugin.Stop();
	InFixture.Plugin =
	    std::make_unique<FSceneViewerPlugin>(*InFixture.Session, InFixture.Tasks, InFixture.Assets, Output);
	InFixture.Plugin->Start();
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (!InFixture.Plugin->Ready() && std::chrono::steady_clock::now() < Deadline)
	{
		InFixture.Tick();
	}
	HYP_CHECK(InFixture.Plugin->Ready() && InFixture.Plugin->ModelCount() == 2);
	InFixture.Tick();
	float MaximumDifference{};
	double TotalDifference{};
	std::size_t Different{};
	for (std::size_t Index = 0; Index < Before.size(); ++Index)
	{
		const auto Difference = std::abs(InFixture.Image.Rgba[Index] - Before[Index]);
		MaximumDifference = std::max(MaximumDifference, Difference);
		TotalDifference += Difference;
		Different += Difference > 1.f / 255.f;
	}
	std::cout << "Save reload pixel max=" << MaximumDifference << " mean=" << TotalDifference / Before.size()
	          << " over_one_step=" << Different << " of " << Before.size() << '\n';
	HYP_CHECK(MaximumDifference <= 1.f / 255.f);
}
} // namespace

int main()
{
	try
	{
		FViewerFixture Fixture;
		CheckControls(Fixture);
		CheckSaveReload(Fixture);
		std::cout
		    << "Scene Viewer visible hide/move/duplicate/remove, frozen view, fit and empty-scene recovery passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
