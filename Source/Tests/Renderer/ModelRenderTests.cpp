#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/ModelViewer/ModelViewerPlugin.h"
#include "Hyperion/Renderer/ModelRenderer.h"
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
		Material.Unlit = true;
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
		Started = true;
		Signal.wait(Lock,
		            [&]
		            {
			            return Released;
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
		Released = true;
		Signal.notify_all();
	}

	std::atomic<bool> Started{};

private:
	std::mutex Mutex;
	std::condition_variable Signal;
	bool Released{};
	FLocalFileSystem Local;
};
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
		FShaderArtifact Vertex;
		FShaderArtifact Fragment;
		Tasks.Wait(Tasks.Dispatch(
		    {EDomain::Worker},
		    [&]
		    {
			    Vertex = Compiler.Compile("Model.hlsl", "VSMain", EShaderStage::Vertex, EShaderFormat::Dxil);
			    Fragment = Compiler.Compile("Model.hlsl", "PSMain", EShaderStage::Pixel, EShaderFormat::Dxil);
		    }));
		const auto RenderModel = [&](FModelAsset InModel, bool InCheckConstantRanges = false)
		{
			auto Prepared = PrepareModel(std::make_shared<const FModelAsset>(std::move(InModel)));
			FImage Image;
			Tasks.Wait(Tasks.Dispatch(
			    {EDomain::Rhi, 0},
			    [&]
			    {
				    FModelRenderer Renderer(*Device, Prepared, Vertex, Fragment);
				    // A single readback-test barrier; the production path only polls upload
				    // fences.
				    Device->WaitIdle();
				    HYP_CHECK(Renderer.Ready());
				    Swapchain->BeginFrame({320, 240});
				    FPassCommands Draw;
				    Draw.Name = "Known material pixels";
				    Draw.Clear = Draw.UseDepth = Draw.ClearDepth = Draw.SrgbTarget = true;
				    Draw.TransitionFrom = EResourceState::Present;
				    Draw.TransitionTo = EResourceState::RenderTarget;
				    Draw.Draws =
				        Renderer.Draws(Multiply(Perspective(1, 4.f / 3, .1f, 10), LookAt({0, 0, 3}, {0, 0, 0})),
				                       {0, 0, 3}, {320, 240});
				    if (InCheckConstantRanges)
				    {
					    auto Invalid = Draw;
					    for (std::uint64_t Offset : {std::uint64_t(1), UINT64_MAX, std::uint64_t(1024)})
					    {
						    Invalid.Draws[0].MaterialConstantOffset = Offset;
						    bool Rejected = false;
						    try
						    {
							    Swapchain->Record(0, Invalid);
						    }
						    catch (const std::invalid_argument&)
						    {
							    Rejected = true;
						    }
						    HYP_CHECK(Rejected);
					    }
				    }
				    FPassCommands Present;
				    Present.Name = "Present";
				    Present.TransitionFrom = EResourceState::RenderTarget;
				    Present.TransitionTo = EResourceState::Present;
				    const std::array Lists{Swapchain->Record(0, Draw), Swapchain->Record(1, Present)};
				    Image = Swapchain->EndFrame(Lists, false, true);
			    }));
			return Image;
		};
		// Near red is submitted first; far blue must not overwrite it. Output is sRGB(.25).
		Pixel(RenderModel(Quads(), true), 160, 120, {.5371f, 0, 0});
		auto Blend = Quads();
		Blend.Materials[0].BaseColor = {1, 0, 0, .5f};
		Blend.Materials[0].AlphaMode = EAlphaMode::Blend;
		const auto Blended = RenderModel(Blend);
		Pixel(Blended, 160, 120, {.7354f, 0, .7354f});
		SaveImage(Root / "out/captures/model-alpha.png", Blended);
		auto OffAxisBlend = Quads();
		OffAxisBlend.Materials[0].BaseColor = {1, 0, 0, .5f};
		OffAxisBlend.Materials[1].BaseColor = {0, 0, 1, .5f};
		OffAxisBlend.Materials[0].AlphaMode = OffAxisBlend.Materials[1].AlphaMode = EAlphaMode::Blend;
		OffAxisBlend.Nodes[0].Local = Translation({0, 0, 1});
		OffAxisBlend.Primitives[0].Positions = {-1, -1, 0, 7, -1, 0, 7, 1, 0, -1, 1, 0};
		Pixel(RenderModel(OffAxisBlend), 160, 120, {.7354f, 0, .5371f});
		auto Mask = Quads();
		Mask.Materials[0].BaseColor = {1, 1, 1, 1};
		Mask.Materials[0].AlphaMode = EAlphaMode::Mask;
		Mask.Materials[0].BaseColorTexture = {0, 0, 0};
		Mask.Images.push_back({"Mask", 2, 1, {255, 0, 0, 0, 255, 0, 0, 255}});
		FModelSampler Nearest;
		Nearest.Min = Nearest.Mag = ESamplerFilter::Nearest;
		Mask.Samplers.push_back(Nearest);
		const auto Masked = RenderModel(Mask);
		Pixel(Masked, 120, 120, {0, 0, 1});
		Pixel(Masked, 200, 120, {1, 0, 0});
		SaveImage(Root / "out/captures/model-mask.png", Masked);
		Mask.Images[0].Rgba = {128, 128, 128, 255, 255, 255, 255, 255};
		Mask.Materials[0].BaseColorTexture.TexCoord = 1;
		Mask.Primitives[0].TexCoords1 = {.25f, .5f, .25f, .5f, .25f, .5f, .25f, .5f};
		Pixel(RenderModel(Mask), 200, 120, {.502f, .502f, .502f});
		Mask.Images[0].Rgba = {0, 0, 0, 255, 255, 255, 255, 255};
		auto Prepared = PrepareModel(std::make_shared<const FModelAsset>(Mask));
		HYP_CHECK(Prepared.Textures[1].Mips.back().Rgba[0] == 188);
		auto Mirrored = Quads();
		Mirrored.Nodes[0].Local = ComposeTRS({0, 0, .25f}, {0, 0, 0, 1}, {-1, 1, 1});
		Pixel(RenderModel(Mirrored), 160, 120, {.5371f, 0, 0});
		auto Backface = Quads();
		std::reverse(Backface.Primitives[0].Indices.begin(), Backface.Primitives[0].Indices.end());
		Backface.Materials[0].DoubleSided = true;
		Pixel(RenderModel(Backface), 160, 120, {.5371f, 0, 0});

		auto Storage = std::make_shared<FGatedFileSystem>();
		FIOService IO(Tasks, Storage);
		FAssetService Assets(IO);
		RegisterGltfImporter(Assets);
		FAppSettings Settings;
		FModelViewerPlugin Plugin(*Device, Compiler, Tasks, Assets, Root / "out/fixtures/Showcase.gltf");
		Plugin.Start();
		const auto Frame = [&](FModelViewerPlugin& InPlugin, FSize InSize = {320, 240})
		{
			Window.Poll();
			FImage Image;
			Tasks.Wait(Tasks.Dispatch({EDomain::Render},
			                          [&]
			                          {
				                          FRenderGraph Graph;
				                          FColorPass Clear;
				                          Clear.Load = EColorLoad::Clear;
				                          Clear.Commands.Name = "Background";
				                          Graph.Add(std::move(Clear));
				                          InPlugin.Build(Graph, {InSize, Settings});
				                          Image = ExecuteGraph(Graph, Tasks, *Swapchain, InSize, false, true);
			                          }));
			return Image;
		};
		// The physical read remains gated while Main, Render and RHI finish two frames.
		bool Responsive{};
		try
		{
			const auto LoadDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
			while (!Storage->Started && std::chrono::steady_clock::now() < LoadDeadline)
			{
				Frame(Plugin);
			}
			Frame(Plugin);
			Frame(Plugin, {480, 200});
			Responsive = Storage->Started && !Plugin.Ready() && Plugin.Error().empty();
		}
		catch (...)
		{
			Storage->Release();
			Plugin.Stop();
			throw;
		}
		Storage->Release();
		HYP_CHECK(Responsive);
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
		FImage ReadyImage;
		while (!Plugin.Ready() && Plugin.Error().empty() && std::chrono::steady_clock::now() < Deadline)
		{
			ReadyImage = Frame(Plugin);
		}
		HYP_CHECK(Plugin.Ready() && Storage->Started);
		FInputEvent Wheel;
		Wheel.Type = EEventType::MouseWheel;
		Wheel.Y = 2;
		Plugin.Input(std::span(&Wheel, 1), true, false);
		HYP_CHECK(Difference(ReadyImage, Frame(Plugin)) < .0001f);
		Plugin.Input(std::span(&Wheel, 1), false, false);
		HYP_CHECK(Difference(ReadyImage, Frame(Plugin)) > .003f);
		FInputEvent Home;
		Home.Type = EEventType::Key;
		Home.Key = EKey::Home;
		Home.Down = true;
		Plugin.Input(std::span(&Home, 1), false, false);
		HYP_CHECK(Difference(ReadyImage, Frame(Plugin)) < .0001f);
		std::array<FInputEvent, 4> Drag;
		Drag[0].Type = EEventType::MouseMove;
		Drag[0].X = 100;
		Drag[0].Y = 100;
		Drag[1].Type = EEventType::MouseButton;
		Drag[1].Button = 1;
		Drag[1].Down = true;
		Drag[2] = Drag[0];
		Drag[2].X = 180;
		Drag[3] = Drag[1];
		Drag[3].Down = false;
		Plugin.Input(Drag, false, false);
		const auto Orbited = Frame(Plugin);
		HYP_CHECK(Difference(ReadyImage, Orbited) > .003f);
		SaveImage(Root / "out/captures/model-orbit.png", Orbited);
		HYP_CHECK(Frame(Plugin, {480, 200}).Width == 480);
		Plugin.Stop();
		FModelViewerPlugin Broken(*Device, Compiler, Tasks, Assets, Root / "out/fixtures/Missing.gltf");
		Broken.Start();
		while (Broken.Error().empty() && std::chrono::steady_clock::now() < Deadline)
		{
			Frame(Broken);
		}
		HYP_CHECK(!Broken.Error().empty() && !Broken.Ready());
		Broken.Stop();
		Assets.Drain();
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
