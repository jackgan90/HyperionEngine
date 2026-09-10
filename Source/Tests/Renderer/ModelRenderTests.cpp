#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/ModelViewer/ModelViewerPlugin.h"
#include "Hyperion/Renderer/Model.h"
#include "Hyperion/Renderer/ModelPreparation.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Support/GraphTestSupport.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

namespace
{
using namespace Hyperion;

FModelAsset Quads()
{
	FModelAsset Model;
	for (int Index = 0; Index < 2; ++Index)
	{
		FModelPrimitive Primitive;
		Primitive.Positions = {-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0};
		Primitive.Indices = {0, 1, 2, 0, 2, 3};
		Primitive.TexCoords0 = {0, 1, 1, 1, 1, 0, 0, 0};
		Primitive.Material = Index;
		Model.Primitives.push_back(Primitive);
		FModelMaterial Material;
		Material.bUnlit = true;
		Material.BaseColor = Index == 0 ? FVec4{.25f, 0, 0, 1} : FVec4{0, 0, 1, 1};
		Model.Materials.push_back(Material);
		FModelNode Node;
		Node.Primitives = {static_cast<std::uint32_t>(Index)};
		Node.Local = ComposeTRS({0, 0, Index == 0 ? .25f : 0.f}, {0, 0, 0, 1}, {1, 1, 1});
		Model.Nodes.push_back(Node);
		Model.Roots.push_back(static_cast<std::uint32_t>(Index));
	}
	return Model;
}

void Pixel(const FImage& InImage, unsigned InX, unsigned InY, FVec3 InExpected)
{
	const auto Offset = (std::size_t(InY) * InImage.Width + InX) * 4;
	HYP_CHECK(std::abs(InImage.Rgba[Offset] - InExpected.X) < .025f);
	HYP_CHECK(std::abs(InImage.Rgba[Offset + 1] - InExpected.Y) < .025f);
	HYP_CHECK(std::abs(InImage.Rgba[Offset + 2] - InExpected.Z) < .025f);
}

float Difference(const FImage& InA, const FImage& InB)
{
	HYP_CHECK(InA.Rgba.size() == InB.Rgba.size());
	double Sum{};
	for (std::size_t Index = 0; Index < InA.Rgba.size(); ++Index)
	{
		Sum += std::abs(InA.Rgba[Index] - InB.Rgba[Index]);
	}
	return static_cast<float>(Sum / InA.Rgba.size());
}

class FGatedFileSystem final : public IFileSystem
{
public:
	FBytes Read(const std::filesystem::path& InPath, std::size_t InLimit) override
	{
		std::unique_lock Lock(Mutex);
		bStarted = true;
		Signal.wait(Lock,
		            [&]
		            {
			            return bReleased;
		            });
		Lock.unlock();
		return Local.Read(InPath, InLimit);
	}

	void WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes) override
	{
		Local.WriteAtomic(InPath, InBytes);
	}

	void Release()
	{
		std::lock_guard Lock(Mutex);
		bReleased = true;
		Signal.notify_all();
	}

	std::atomic<bool> bStarted{};

private:
	std::mutex Mutex;
	std::condition_variable Signal;
	bool bReleased{};
	FLocalFileSystem Local;
};

struct FModelReadbackContext
{
	FTaskSystem& Tasks;
	IRHIDevice& Device;
	IRHISwapchain& Swapchain;
	FRenderSession& Session;
};

FImage RenderModelReadback(const FModelReadbackContext& InContext, FModelAsset InModel, bool bInCheckConstantRanges)
{
	FModel Model(InContext.Session.GetScene(), InContext.Session.GetResources(),
	             std::make_shared<const FModelAsset>(std::move(InModel)));
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (!Model.IsReady() && Model.GetError().empty() && std::chrono::steady_clock::now() < Deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if (!Model.GetError().empty())
	{
		throw std::runtime_error(Model.GetError());
	}
	HYP_CHECK(Model.IsReady());
	FPassCommands PreparedDraw;
	PreparedDraw.Color = FColorAttachment{FRenderTarget::Backbuffer()};
	InContext.Tasks.Wait(InContext.Tasks.Dispatch(
	    {EDomain::Render},
	    [&]
	    {
		    FRenderView View{
		        Multiply(Perspective(1, 4.f / 3, .1f, 10), LookAt({0, 0, 3}, {0, 0, 0})), {0, 0, 3}, 320, 240};
		    FRenderGraph Graph;
		    auto Clear = MakeColorPass(Graph, "pending");
		    Clear.Name = "Clear model";
		    Clear.Color->Actions.Load = EAttachmentLoad::Clear;
		    Graph.Add(std::move(Clear));
		    InContext.Session.Build(Graph, View, InContext.Session.FrameTargets());
		    std::vector<FPassCommands> Passes;
		    InContext.Tasks.Wait(InContext.Tasks.Dispatch({EDomain::Rhi, 0},
		                                                  [&]
		                                                  {
			                                                  Passes = Graph.Compile();
		                                                  }));
		    HYP_CHECK(Passes.size() == 3);
		    PreparedDraw = Passes[1];
	    }));
	FImage Image;
	InContext.Tasks.Wait(InContext.Tasks.Dispatch(
	    {EDomain::Rhi, 0},
	    [&]
	    {
		    auto Draw = std::move(PreparedDraw);
		    Draw.Name = "Known material pixels";
		    Draw.Color->Actions.Load = EAttachmentLoad::Clear;
		    Draw.Transitions = {{FRenderTarget::Backbuffer(), EResourceState::Present, EResourceState::RenderTarget}};
		    InContext.Swapchain.BeginFrame({320, 240});
		    if (bInCheckConstantRanges)
		    {
			    auto Invalid = Draw;
			    for (std::uint64_t Offset : {std::uint64_t(1), UINT64_MAX, std::uint64_t(1024)})
			    {
				    Invalid.Draws[0].ConstantBindings[0].Slice.Offset = Offset;
				    bool bRejected = false;
				    try
				    {
					    InContext.Swapchain.Record(0, Invalid);
				    }
				    catch (const std::invalid_argument&)
				    {
					    bRejected = true;
				    }
				    HYP_CHECK(bRejected);
			    }
		    }
		    FPassCommands Present;
		    Present.Name = "Present";
		    Present.Transitions = {
		        {FRenderTarget::Backbuffer(), EResourceState::RenderTarget, EResourceState::Present}};
		    const std::array Lists{InContext.Swapchain.Record(0, Draw), InContext.Swapchain.Record(1, Present)};
		    Image = InContext.Swapchain.EndFrame(Lists, false, true);
	    }));
	return Image;
}

template<class RenderOperation>
void CheckMaterialPixels(const RenderOperation& InRender, const std::filesystem::path& InRoot)
{
	// Near red is submitted first; far blue must not overwrite it. Output is sRGB(.25).
	Pixel(InRender(Quads(), true), 160, 120, {.5371f, 0, 0});
	auto Blend = Quads();
	Blend.Materials[0].BaseColor = {1, 0, 0, .5f};
	Blend.Materials[0].AlphaMode = EAlphaMode::Blend;
	const auto Blended = InRender(Blend);
	Pixel(Blended, 160, 120, {.7354f, 0, .7354f});
	SaveImage(InRoot / "out/captures/model-alpha.png", Blended);
	auto OffAxisBlend = Quads();
	OffAxisBlend.Materials[0].BaseColor = {1, 0, 0, .5f};
	OffAxisBlend.Materials[1].BaseColor = {0, 0, 1, .5f};
	OffAxisBlend.Materials[0].AlphaMode = OffAxisBlend.Materials[1].AlphaMode = EAlphaMode::Blend;
	OffAxisBlend.Nodes[0].Local = Translation({0, 0, 1});
	OffAxisBlend.Primitives[0].Positions = {-1, -1, 0, 7, -1, 0, 7, 1, 0, -1, 1, 0};
	Pixel(InRender(OffAxisBlend), 160, 120, {.7354f, 0, .5371f});
	auto Mask = Quads();
	Mask.Materials[0].BaseColor = {1, 1, 1, 1};
	Mask.Materials[0].AlphaMode = EAlphaMode::Mask;
	Mask.Materials[0].BaseColorTexture = {0, 0, 0};
	Mask.Images.push_back({"Mask", 2, 1, {255, 0, 0, 0, 255, 0, 0, 255}});
	FModelSampler Nearest;
	Nearest.Min = Nearest.Mag = ESamplerFilter::Nearest;
	Mask.Samplers.push_back(Nearest);
	const auto Masked = InRender(Mask);
	Pixel(Masked, 120, 120, {0, 0, 1});
	Pixel(Masked, 200, 120, {1, 0, 0});
	SaveImage(InRoot / "out/captures/model-mask.png", Masked);
	Mask.Images[0].Rgba = {128, 128, 128, 255, 255, 255, 255, 255};
	Mask.Materials[0].BaseColorTexture.TexCoord = 1;
	Mask.Primitives[0].TexCoords1 = {.25f, .5f, .25f, .5f, .25f, .5f, .25f, .5f};
	Pixel(InRender(Mask), 200, 120, {.502f, .502f, .502f});
	Mask.Images[0].Rgba = {0, 0, 0, 255, 255, 255, 255, 255};
	auto Prepared = PrepareModel(std::make_shared<const FModelAsset>(Mask));
	HYP_CHECK(Prepared.Textures[1].Mips.back().Rgba[0] == 188);
	auto Mirrored = Quads();
	Mirrored.Nodes[0].Local = ComposeTRS({0, 0, .25f}, {0, 0, 0, 1}, {-1, 1, 1});
	Pixel(InRender(Mirrored), 160, 120, {.5371f, 0, 0});
	auto Backface = Quads();
	std::reverse(Backface.Primitives[0].Indices.begin(), Backface.Primitives[0].Indices.end());
	Backface.Materials[0].bDoubleSided = true;
	Pixel(InRender(Backface), 160, 120, {.5371f, 0, 0});
}

FImage RenderViewerFrame(FTaskSystem& InTasks, FWindow& InWindow, IRHISwapchain& InSwapchain,
                         const FAppSettings& InSettings, FModelViewerPlugin& InPlugin, FRenderSession& InSession,
                         FSize InSize)
{
	InWindow.Poll();
	FRenderFrame Frame{InSize, InSettings};
	Frame.View.Width = InSize.Width;
	Frame.View.Height = InSize.Height;
	InPlugin.Update(Frame);
	FImage Image;
	InTasks.Wait(InTasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              FRenderGraph Graph;
		                              auto Clear = MakeColorPass(Graph, "pending");
		                              Clear.Color->Actions.Load = EAttachmentLoad::Clear;
		                              Clear.Name = "Background";
		                              Graph.Add(std::move(Clear));
		                              InSession.Build(Graph, Frame.View, InSession.FrameTargets());
		                              Image = ExecuteGraph(Graph, InTasks, InSwapchain, InSize, false, true);
	                              }));
	return Image;
}

template<class FrameOperation>
void CheckCameraInput(FModelViewerPlugin& InPlugin, const FrameOperation& InFrame, const FImage& InReadyImage,
                      const std::filesystem::path& InRoot)
{
	FInputEvent Wheel;
	Wheel.Type = EEventType::MouseWheel;
	Wheel.Y = 2;
	InPlugin.Input(std::span(&Wheel, 1), true, false);
	HYP_CHECK(Difference(InReadyImage, InFrame(InPlugin)) < .0001f);
	InPlugin.Input(std::span(&Wheel, 1), false, false);
	HYP_CHECK(Difference(InReadyImage, InFrame(InPlugin)) > .003f);
	FInputEvent Home;
	Home.Type = EEventType::Key;
	Home.Key = EKey::Home;
	Home.bDown = true;
	InPlugin.Input(std::span(&Home, 1), false, false);
	HYP_CHECK(Difference(InReadyImage, InFrame(InPlugin)) < .0001f);
	std::array<FInputEvent, 4> Drag;
	Drag[0].Type = EEventType::MouseMove;
	Drag[0].X = 100;
	Drag[0].Y = 100;
	Drag[1].Type = EEventType::MouseButton;
	Drag[1].Button = 1;
	Drag[1].bDown = true;
	Drag[2] = Drag[0];
	Drag[2].X = 180;
	Drag[3] = Drag[1];
	Drag[3].bDown = false;
	InPlugin.Input(Drag, false, false);
	const auto Orbited = InFrame(InPlugin);
	HYP_CHECK(Difference(InReadyImage, Orbited) > .003f);
	SaveImage(InRoot / "out/captures/model-orbit.png", Orbited);
	HYP_CHECK(InFrame(InPlugin, {480, 200}).Width == 480);
}

template<class FrameOperation>
void CheckGatedFrames(FGatedFileSystem& InStorage, FModelViewerPlugin& InPlugin, const FrameOperation& InFrame)
{
	// The physical read remains gated while Main, Render and RHI finish two frames.
	bool bResponsive{};
	try
	{
		const auto LoadDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (!InStorage.bStarted && std::chrono::steady_clock::now() < LoadDeadline)
		{
			InFrame(InPlugin);
		}
		InFrame(InPlugin);
		InFrame(InPlugin, {480, 200});
		bResponsive = InStorage.bStarted && !InPlugin.Ready() && InPlugin.Error().empty();
	}
	catch (...)
	{
		InStorage.Release();
		InPlugin.Stop();
		throw;
	}
	InStorage.Release();
	HYP_CHECK(bResponsive);
}

} // namespace

int main()
{
	using namespace Hyperion;
	try
	{
		const auto Root = std::filesystem::path(HYP_SOURCE_DIR);
		FTaskSystem Tasks(1, 1);
		FWindow Window("Model material and async acceptance", {320, 240}, true);
		const FRHISwapchainDesc SwapchainDesc{Window.Surface(), Window.PixelSize()};
		FRHIBackendRegistry Registry;
		RegisterD3D12RHIBackend(Registry);
		std::unique_ptr<IRHIDevice> Device;
		std::unique_ptr<IRHISwapchain> Swapchain;
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Device = Registry.CreateDevice(ERHIBackend::D3D12, {});
			                          Swapchain = Device->CreateSwapchain(SwapchainDesc);
		                          }));
		FShaderCompiler Compiler(Root / "shaders", Root / "out/shader-cache");
		FRenderSession Session(Tasks, *Device, Compiler);
		const FModelReadbackContext RenderContext{Tasks, *Device, *Swapchain, Session};
		const auto RenderModel = [&](FModelAsset InModel, bool bInCheckConstantRanges = false)
		{
			return RenderModelReadback(RenderContext, std::move(InModel), bInCheckConstantRanges);
		};
		CheckMaterialPixels(RenderModel, Root);

		auto Storage = std::make_shared<FGatedFileSystem>();
		FIOService IO(Tasks, Storage);
		FAssetService Assets(IO);
		RegisterGltfImporter(Assets);
		FAppSettings Settings;
		FModelViewerPlugin Plugin(Session, Tasks, Assets, Root / "out/fixtures/Showcase.gltf");
		Plugin.Start();
		const auto Frame = [&](FModelViewerPlugin& InPlugin, FSize InSize = {320, 240})
		{
			return RenderViewerFrame(Tasks, Window, *Swapchain, Settings, InPlugin, Session, InSize);
		};
		CheckGatedFrames(*Storage, Plugin, Frame);
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
		FImage ReadyImage;
		while (!Plugin.Ready() && Plugin.Error().empty() && std::chrono::steady_clock::now() < Deadline)
		{
			ReadyImage = Frame(Plugin);
		}
		HYP_CHECK(Plugin.Ready() && Storage->bStarted);
		CheckCameraInput(Plugin, Frame, ReadyImage, Root);
		Plugin.Stop();
		FModelViewerPlugin Broken(Session, Tasks, Assets, Root / "out/fixtures/Missing.gltf");
		Broken.Start();
		while (Broken.Error().empty() && std::chrono::steady_clock::now() < Deadline)
		{
			Frame(Broken);
		}
		HYP_CHECK(!Broken.Error().empty() && !Broken.Ready());
		Broken.Stop();
		Assets.Drain();
		Session.Close();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Device->WaitIdle();
			                          HYP_CHECK(Device->Statistics().ValidationErrors == 0);
			                          Swapchain.reset();
			                          Device.reset();
		                          }));
		Tasks.Shutdown();
		std::cout << "Depth, linear alpha blend, alpha mask, sRGB/UV1/mips, mirrored/double-sided geometry, gated IO "
		             "frames, camera and error recovery passed\n";
	}
	catch (const std::exception& InError)
	{
		std::cerr << InError.what() << '\n';
		return 1;
	}
}
