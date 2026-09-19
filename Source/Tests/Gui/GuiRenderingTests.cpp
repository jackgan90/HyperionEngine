#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/GuiRenderer/GuiRenderer.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace
{
using namespace Hyperion;

void CheckFontSnapshots(FGui& InGui, FGuiRenderer& InRenderer, FTaskSystem& InTasks, IRHISwapchain& InSwapchain)
{
	const auto MakeFrame = [&](float InScale)
	{
		InGui.SetApplicationScale(InScale);
		InGui.BeginFrame({400, 300}, {400, 300}, 1.f / 60, {});
		InGui.BeginPanel("Font snapshot", {0, 0}, {400, 300});
		InGui.Text("Application Scale 0123456789");
		InGui.Button("Button");
		InGui.EndPanel();
		return InGui.Render();
	};
	MakeFrame(1);
	const auto Small = MakeFrame(1);
	const auto Large = MakeFrame(2);
	HYP_CHECK(Small.FontAtlas != Large.FontAtlas);
	const auto Render = [&](const FGuiDrawData& InData)
	{
		FImage Result;
		InTasks.Wait(InTasks.Dispatch({EDomain::Render},
		                              [&]
		                              {
			                              FRenderGraph Graph;
			                              InRenderer.BuildDeferred(Graph, InData, {}, true);
			                              Result = ExecuteGraph(std::move(Graph), InTasks, InSwapchain, {400, 300},
			                                                    false, true);
		                              }));
		return Result;
	};
	// Both snapshots were produced before either was submitted. Rendering the older
	// snapshot again must restore its atlas, rather than using mutable GUI state.
	const auto First = Render(Small);
	const auto Second = Render(Large);
	HYP_CHECK(First.Rgba != Second.Rgba);
	HYP_CHECK(Render(Small).Rgba == First.Rgba);
	HYP_CHECK(Render(Large).Rgba == Second.Rgba);
	InGui.SetApplicationScale(1);
}

void CheckTextureComposition()
{
	FTaskSystem Tasks(2, 1);
	FWindow Window("GUI texture contract", {400, 300}, true);
	FRHIBackendRegistry Registry;
	RegisterD3D12RHIBackend(Registry);
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	const auto Surface = Window.Surface();
	const auto Size = Window.PixelSize();
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          Device = Registry.CreateDevice(ERHIBackend::D3D12, {});
		                          Swapchain = Device->CreateSwapchain({Surface, Size});
	                          }));
	FShaderCompiler Compiler(std::filesystem::path(HYP_SOURCE_DIR) / "Content/Shaders",
	                         std::filesystem::path(HYP_SOURCE_DIR) / "out/shader-cache");
	FRenderSession Session(Tasks, *Device, Compiler);
	FGui Gui;
	FGuiRenderer Renderer(*Device, Compiler, Tasks, Gui.FontImage(), &Session.GetResources());
	Renderer.Start();
	FRenderTargetSource Source{
	    ERenderTargetKind::Texture,
	    std::make_shared<const FMaterialTextureSource>(FMaterialColorTexture{64, 64, EMaterialColorFormat::Rgba8Unorm}),
	    Session.GetResources().CreateScopeLifetime(), false};
	Gui.BeginFrame({400, 300}, {400, 300}, 1.f / 60, {});
	Gui.BeginPanel("Image", {0, 0}, {400, 300});
	const auto Region = Gui.Image(7);
	Gui.EndPanel();
	auto Data = Gui.Render();
	HYP_CHECK(std::any_of(Data.Commands.begin(), Data.Commands.end(),
	                      [](const auto& InCommand)
	                      {
		                      return InCommand.TextureId == 7;
	                      }));
	FImage Image;
	const auto Render = [&]
	{
		Tasks.Wait(Tasks.Dispatch(
		    {EDomain::Render},
		    [&]
		    {
			    FRenderGraph Graph;
			    FRenderSceneSnapshot Snapshot;
			    Snapshot.Targets.Color =
			        FRenderColorTarget{Source, {EAttachmentLoad::Clear}, {.25f, .5f, .75f, 1}, EGraphColorView::Srgb};
			    Graph.Add(Session.GetResources().GetPreparation().DeclarePass(Graph, Snapshot));
			    Renderer.BuildDeferred(Graph, Data, {{7, Source}}, true);
			    Image = ExecuteGraph(std::move(Graph), Tasks, *Swapchain, {400, 300}, false, true);
		    }));
	};
	Render();
	const auto X = static_cast<std::uint32_t>((Region.Bounds.X + Region.Bounds.Z) / 2);
	const auto Y = static_cast<std::uint32_t>((Region.Bounds.Y + Region.Bounds.W) / 2);
	const auto Offset = (Y * Image.Width + X) * 4;
	HYP_CHECK(std::abs(Image.Rgba[Offset] - .5371f) < .012f);
	HYP_CHECK(std::abs(Image.Rgba[Offset + 1] - .7354f) < .012f);
	HYP_CHECK(std::abs(Image.Rgba[Offset + 2] - .8808f) < .012f);
	const auto Original = Image;
	const auto Cold = Device->Statistics();
	Render();
	const auto Warm = Device->Statistics();
	HYP_CHECK(Image.Rgba == Original.Rgba);
	HYP_CHECK(Warm.BindingSetsCreated == Cold.BindingSetsCreated);
	HYP_CHECK(Warm.ConstantBytesWritten == Cold.ConstantBytesWritten);
	HYP_CHECK(Warm.DescriptorCopies == Cold.DescriptorCopies);
	// Reusing an ID with a new image must refresh the cached native binding.
	Source.Texture =
	    std::make_shared<const FMaterialTextureSource>(FMaterialColorTexture{80, 80, EMaterialColorFormat::Rgba8Unorm});
	Render();
	HYP_CHECK(Device->Statistics().BindingSetsCreated == Warm.BindingSetsCreated + 1);
	std::cout << "Stable GUI frame: 0 new binding sets, 0 constant bytes, 0 descriptor copies\n";
	bool bRejected{};
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          try
		                          {
			                          Renderer.Prepare(Data);
		                          }
		                          catch (const std::invalid_argument&)
		                          {
			                          bRejected = true;
		                          }
	                          }));
	HYP_CHECK(bRejected);
	Render();
	const std::weak_ptr<const void> CachedOwner = Source.Lifetime;
	CheckFontSnapshots(Gui, Renderer, Tasks, *Swapchain);
	Render();
	Source = {};
	Tasks.Wait(Tasks.Dispatch({EDomain::Render},
	                          [&]
	                          {
		                          FRenderGraph Empty;
		                          Renderer.BuildDeferred(Empty, {}, {}, false);
	                          }));
	HYP_CHECK(!CachedOwner.expired());
	Renderer.Stop();
	HYP_CHECK(CachedOwner.expired());
	FGuiRenderer Standalone(*Device, Compiler, Tasks, Gui.FontImage());
	Standalone.Start();
	Tasks.Wait(Tasks.Dispatch({EDomain::Render},
	                          [&]
	                          {
		                          FRenderGraph Graph;
		                          Standalone.BuildDeferred(Graph, {}, {}, true);
		                          Graph.Export(Graph.ImportBackbuffer(), EResourceState::Present);
		                          const auto Clear =
		                              ExecuteGraph(std::move(Graph), Tasks, *Swapchain, {400, 300}, false, true);
		                          HYP_CHECK(std::abs(Clear.Rgba[0] - .09985f) < .012f);
	                          }));
	Standalone.Stop();
	Session.Close();
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
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
		CheckTextureComposition();
		std::cout << "GUI texture identities, sRGB offscreen composition and missing bindings passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
