#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/Model.h"
#include "Hyperion/Renderer/ModelPreparation.h"
#include "Hyperion/Renderer/NativeModel.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/SceneCameraController.h"
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include "Support/GraphTestSupport.h"
#include "Support/ModelAssetSupport.h"
#include "Support/NativeAssetSupport.h"
#include "Support/ShaderSourceSupport.h"
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

FModelSource Quads()
{
	FModelSource Model;
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
	FNativeOnlyFileSystem Local;
};

struct FModelReadbackContext
{
	FTaskSystem& Tasks;
	IRHIDevice& Device;
	IRHISwapchain& Swapchain;
	FRenderSession& Session;
};

FImage RenderModelReadback(const FModelReadbackContext& InContext, FModelSource InModel, bool bInCheckConstantRanges)
{
	FSourceModel Model(InContext.Session.GetScene(), InContext.Session.GetResources(),
	                   std::make_shared<const FModelSource>(std::move(InModel)));
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

template<class RenderOperation> void CheckMaterialPixels(const RenderOperation& InRender)
{
	// Near red is submitted first; far blue must not overwrite it. Output is sRGB(.25).
	Pixel(InRender(Quads(), true), 160, 120, {.5371f, 0, 0});
	auto Blend = Quads();
	Blend.Materials[0].BaseColor = {1, 0, 0, .5f};
	Blend.Materials[0].AlphaMode = EAlphaMode::Blend;
	const auto Blended = InRender(Blend);
	Pixel(Blended, 160, 120, {.7354f, 0, .7354f});
	SaveImage(std::filesystem::path(HYP_TEST_OUTPUT_DIR) / "captures/model-alpha.png", Blended);
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
	SaveImage(std::filesystem::path(HYP_TEST_OUTPUT_DIR) / "captures/model-mask.png", Masked);
	Mask.Images[0].Rgba = {128, 128, 128, 255, 255, 255, 255, 255};
	Mask.Materials[0].BaseColorTexture.TexCoord = 1;
	Mask.Primitives[0].TexCoords1 = {.25f, .5f, .25f, .5f, .25f, .5f, .25f, .5f};
	Pixel(InRender(Mask), 200, 120, {.502f, .502f, .502f});
	Mask.Images[0].Rgba = {0, 0, 0, 255, 255, 255, 255, 255};
	const auto Prepared = SplitModelSource(Mask);
	const auto Texture = std::find_if(Prepared.Products.begin(), Prepared.Products.end(),
	                                  [](const auto& InProduct)
	                                  {
		                                  return InProduct.Key == "image-0-srgb";
	                                  });
	HYP_CHECK(Texture != Prepared.Products.end());
	HYP_CHECK(std::static_pointer_cast<const FTextureAsset>(Texture->Object)->Mips.back().Bytes[0] == 188);
	auto Mirrored = Quads();
	Mirrored.Nodes[0].Local = ComposeTRS({0, 0, .25f}, {0, 0, 0, 1}, {-1, 1, 1});
	Pixel(InRender(Mirrored), 160, 120, {.5371f, 0, 0});
	auto Backface = Quads();
	std::reverse(Backface.Primitives[0].Indices.begin(), Backface.Primitives[0].Indices.end());
	Backface.Materials[0].bDoubleSided = true;
	Pixel(InRender(Backface), 160, 120, {.5371f, 0, 0});
}

struct FAsyncModelFixture
{
	FSceneInstance Scene;
	TAsyncResult<FSceneModelData> Load;
	FCancellationToken Cancellation;
	FSceneCameraController Camera;
	FSceneHandle Model;
	std::string Failure;

	FAsyncModelFixture(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets,
	                   const std::filesystem::path& InPath)
	    : Scene(InSession, InTasks, InAssets)
	{
		const auto View = Scene.AddNode(MakeSceneCameraNode("camera", {2, 2, 7}, {}));
		const auto Light = Scene.AddNode(MakeSceneDirectionalLightNode("light"));
		const auto Sky = Scene.AddNode(MakeSceneEnvironmentLightNode("sky"));
		Scene.SetSettings({View, {}});
		Load = LoadNativeModel(InAssets, InTasks, InPath, Cancellation, &InSession.GetResources());
	}

	void Tick(FSize InSize)
	{
		try
		{
			if (!Model.Generation && Load.Ready() && Failure.empty())
			{
				Model = Scene.Add(FSceneModel{"Test model", Load.GetReady()});
				FitSceneCamera(Scene, float(InSize.Width) / InSize.Height, true);
			}
			Scene.Tick();
		}
		catch (const std::exception& Error)
		{
			Failure = Error.what();
		}
	}

	bool Ready() const
	{
		return Model.Generation && Scene.GetStatus().bReady;
	}

	const std::string& Error() const
	{
		return Failure;
	}

	void Stop()
	{
		Cancellation.Cancel();
		Scene.Close();
	}
};

FImage RenderAsyncModelFrame(FTaskSystem& InTasks, FWindow& InWindow, IRHISwapchain& InSwapchain,
                             FAsyncModelFixture& InModel, FRenderSession& InSession, FSize InSize)
{
	InWindow.Poll();
	FSceneViewRequest Request;
	Request.Width = InSize.Width;
	Request.Height = InSize.Height;
	InModel.Tick(InSize);
	const auto Seed = InSession.FreezeSceneFrame(InModel.Scene.GetToken());
	FImage Image;
	InTasks.Wait(InTasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              FRenderGraph Graph;
		                              auto Clear = MakeColorPass(Graph, "pending");
		                              Clear.Color->Actions.Load = EAttachmentLoad::Clear;
		                              Clear.Name = "Background";
		                              Graph.Add(std::move(Clear));
		                              const auto Resolved = InSession.ResolveSceneFrame(*Seed, Request);
		                              if (Resolved.HasCamera())
		                              {
			                              InSession.BuildViews(Graph, std::span(&Resolved.View, 1),
			                                                   InSession.FrameTargets(), Resolved.Frame);
		                              }
		                              else
		                              {
			                              InSession.BuildSceneClear(Graph, Resolved, {});
		                              }
		                              Image = ExecuteGraph(Graph, InTasks, InSwapchain, InSize, false, true);
	                              }));
	return Image;
}

template<class FrameOperation>
void CheckCameraInput(FAsyncModelFixture& InModel, const FrameOperation& InFrame, const FImage& InReadyImage)
{
	FInputEvent Wheel;
	Wheel.Type = EEventType::MouseWheel;
	Wheel.Y = 2;
	InModel.Camera.Input(InModel.Scene, std::span(&Wheel, 1), true, false);
	HYP_CHECK(Difference(InReadyImage, InFrame(InModel)) < .0001f);
	InModel.Camera.Input(InModel.Scene, std::span(&Wheel, 1), false, false);
	HYP_CHECK(Difference(InReadyImage, InFrame(InModel)) > .003f);
	FitSceneCamera(InModel.Scene, 320.f / 240, true);
	HYP_CHECK(Difference(InReadyImage, InFrame(InModel)) < .0001f);
	std::array<FInputEvent, 4> Drag;
	Drag[0].Type = EEventType::MouseMove;
	Drag[0].X = 100;
	Drag[0].Y = 100;
	Drag[1].Type = EEventType::MouseButton;
	Drag[1].Button = 1;
	Drag[1].bDown = true;
	Drag[1].X = 100;
	Drag[1].Y = 100;
	Drag[2] = Drag[0];
	Drag[2].X = 180;
	Drag[3] = Drag[1];
	Drag[3].bDown = false;
	// A button event supplies the initial pointer position; no preceding move is required.
	InModel.Camera.Input(InModel.Scene, std::span(Drag).subspan(1), false, false);
	const auto Orbited = InFrame(InModel);
	HYP_CHECK(Difference(InReadyImage, Orbited) > .003f);
	InModel.Camera.Input(InModel.Scene, std::span(&Drag[1], 1), false, false);
	InModel.Camera.Input(InModel.Scene, std::span(&Drag[2], 1), true, false);
	HYP_CHECK(Difference(Orbited, InFrame(InModel)) < .0001f);
	SaveImage(std::filesystem::path(HYP_TEST_OUTPUT_DIR) / "captures/model-orbit.png", Orbited);
	HYP_CHECK(InFrame(InModel, {480, 200}).Width == 480);
	auto& Scene = InModel.Scene;
	const auto Camera = *Scene.GetSettings().DefaultCamera;
	auto Lens = *Scene.FindNode(Camera)->Camera();
	Lens.Far += 100;
	Scene.SetCamera(Camera, Lens);
	InModel.Camera.Input(InModel.Scene, {}, false, false);
	InFrame(InModel);
	HYP_CHECK(Scene.FindNode(Camera)->Camera()->Far == Lens.Far);
	InModel.Camera.Input(InModel.Scene, std::span(&Wheel, 1), true, false);
	HYP_CHECK(Scene.FindNode(Camera)->Camera()->Far == Lens.Far);
}

template<class FrameOperation>
void CheckGatedFrames(FGatedFileSystem& InStorage, FAsyncModelFixture& InModel, const FrameOperation& InFrame)
{
	// The physical read remains gated while Main, Render and RHI finish two frames.
	bool bResponsive{};
	try
	{
		const auto LoadDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (!InStorage.bStarted && std::chrono::steady_clock::now() < LoadDeadline)
		{
			InFrame(InModel);
		}
		InFrame(InModel);
		InFrame(InModel, {480, 200});
		bResponsive = InStorage.bStarted && !InModel.Ready() && InModel.Error().empty();
	}
	catch (...)
	{
		InStorage.Release();
		InModel.Stop();
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
		FShaderCompiler Compiler(TestShaderRoot(), std::filesystem::path(HYP_TEST_OUTPUT_DIR) / "shader-cache");
		FRenderSession Session(Tasks, *Device, Compiler);
		const FModelReadbackContext RenderContext{Tasks, *Device, *Swapchain, Session};
		const auto RenderModel = [&](FModelSource InModel, bool bInCheckConstantRanges = false)
		{
			return RenderModelReadback(RenderContext, std::move(InModel), bInCheckConstantRanges);
		};
		CheckMaterialPixels(RenderModel);

		auto Storage = std::make_shared<FGatedFileSystem>();
		FIOService IO(Tasks, Storage);
		FAssetService Assets(IO);

		FAsyncModelFixture ModelFixture(Session, Tasks, Assets,
		                                std::filesystem::path(HYP_TEST_OUTPUT_DIR) /
		                                    "fixtures/native/Showcase-gltf.hasset");
		const auto Frame = [&](FAsyncModelFixture& InModel, FSize InSize = {320, 240})
		{
			return RenderAsyncModelFrame(Tasks, Window, *Swapchain, InModel, Session, InSize);
		};
		CheckGatedFrames(*Storage, ModelFixture, Frame);
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
		FImage ReadyImage;
		while (!ModelFixture.Ready() && ModelFixture.Error().empty() && std::chrono::steady_clock::now() < Deadline)
		{
			ReadyImage = Frame(ModelFixture);
		}
		HYP_CHECK(ModelFixture.Ready() && Storage->bStarted);
		CheckCameraInput(ModelFixture, Frame, ReadyImage);
		ModelFixture.Stop();
		FAsyncModelFixture Broken(Session, Tasks, Assets,
		                          std::filesystem::path(HYP_TEST_OUTPUT_DIR) / "fixtures/native/Missing.hasset");
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
