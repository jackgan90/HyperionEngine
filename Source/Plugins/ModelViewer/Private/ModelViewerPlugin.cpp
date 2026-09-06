#include "Hyperion/ModelViewer/ModelViewerPlugin.h"
#include "Hyperion/Renderer/ModelRenderer.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
struct FModelViewerPlugin::FImpl
{
	struct FPreparedScene
	{
		FPreparedModel Model;
		FShaderArtifact Vertex;
		FShaderArtifact Pixel;
	};

	IRHIDevice& Device;
	FShaderCompiler& Compiler;
	FTaskSystem& Tasks;
	FAssetService& Assets;
	std::filesystem::path Path;
	TAssetRequest<FModelAsset> Request;
	TAsyncResult<FPreparedScene> Preparation;
	TAsyncResult<std::shared_ptr<FModelRenderer>> Upload;
	std::shared_ptr<FModelRenderer> Renderer;
	std::string Status = "Loading model...";
	std::string Error;
	bool bUploadStarted{};
	bool bIsReady{};
	bool bDragging{};
	bool bFitRequested = true;
	FVec2 LastMouse;
	FVec3 Center;
	float Radius = 1;
	float Distance = 5;
	float Yaw = .35f;
	float Pitch = .3f;
};

FModelViewerPlugin::FModelViewerPlugin(IRHIDevice& InDevice, FShaderCompiler& InCompiler, FTaskSystem& InTasks,
                                       FAssetService& InAssets, std::filesystem::path InPath)
    : Impl(std::make_unique<FImpl>(FImpl{InDevice, InCompiler, InTasks, InAssets, std::move(InPath)}))
{
}

FModelViewerPlugin::~FModelViewerPlugin() = default;

void FModelViewerPlugin::Start()
{
	auto& P = *Impl;
	P.Request = P.Assets.LoadAsync<FModelAsset>(P.Path);
	P.Preparation = DispatchAsync<FImpl::FPreparedScene>(
	    P.Tasks, {EDomain::Worker},
	    [&P]
	    {
		    auto Model = P.Request.Get(P.Tasks);
		    FImpl::FPreparedScene Prepared;
		    Prepared.Model = PrepareModel(Model);
		    Prepared.Vertex = P.Compiler.Compile("Model.hlsl", "VSMain", EShaderStage::Vertex,
		                                         P.Device.GetCapabilities().ShaderFormat);
		    Prepared.Pixel = P.Compiler.Compile("Model.hlsl", "PSMain", EShaderStage::Pixel,
		                                        P.Device.GetCapabilities().ShaderFormat);
		    return Prepared;
	    });
}

void FModelViewerPlugin::Build(FRenderGraph& InGraph, const FRenderFrame& InFrame)
{
	auto& P = *Impl;
	if (!P.Error.empty())
	{
		return;
	}
	try
	{
		if (!P.bUploadStarted)
		{
			if (!P.Preparation.Ready())
			{
				return;
			}
			auto Prepared = P.Preparation.GetReady();
			P.Center = ScaleVector(Add(Prepared->Model.Bounds.Minimum, Prepared->Model.Bounds.Maximum), .5f);
			P.Radius = std::max(.01f, Length(Subtract(Prepared->Model.Bounds.Maximum, P.Center)));
			P.Status = "Uploading model to GPU...";
			P.Upload = DispatchAsync<std::shared_ptr<FModelRenderer>>(P.Tasks, {EDomain::Rhi, 0},
			                                                          [&P, Prepared]
			                                                          {
				                                                          return std::make_shared<FModelRenderer>(
				                                                              P.Device, Prepared->Model,
				                                                              Prepared->Vertex, Prepared->Pixel);
			                                                          });
			P.bUploadStarted = true;
			P.Preparation = {};
		}
		if (!P.Renderer)
		{
			if (!P.Upload.Ready())
			{
				return;
			}
			P.Renderer = *P.Upload.GetReady();
			P.Upload = {};
		}
		const float Aspect = float(InFrame.Size.Width) / InFrame.Size.Height;
		if (P.bFitRequested)
		{
			const float Limit = std::min(.5f, std::atan(std::tan(.5f) * Aspect));
			P.Distance = P.Radius / std::sin(Limit) * 1.12f;
			P.bFitRequested = false;
		}
		const FVec3 Direction{std::sin(P.Yaw) * std::cos(P.Pitch), std::sin(P.Pitch),
		                      std::cos(P.Yaw) * std::cos(P.Pitch)};
		const FVec3 Eye = Add(P.Center, ScaleVector(Direction, P.Distance));
		const auto ViewProjection =
		    Multiply(Perspective(1, Aspect, std::max(.0001f, P.Radius * .001f), P.Distance + P.Radius * 10),
		             LookAt(Eye, P.Center));
		std::vector<FDrawPacket> Draws;
		P.Tasks.Wait(P.Tasks.Dispatch({EDomain::Rhi, 0},
		                              [&]
		                              {
			                              P.bIsReady = P.Renderer->Ready();
			                              if (P.bIsReady)
			                              {
				                              Draws = P.Renderer->Draws(ViewProjection, Eye, InFrame.Size);
			                              }
		                              }));
		if (!P.bIsReady)
		{
			return;
		}
		P.Status = "Ready | " + std::to_string(Draws.size()) + " draws";
		FColorPass Pass;
		Pass.Commands.Name = "Static model";
		Pass.Commands.bUseDepth = true;
		Pass.Commands.bClearDepth = true;
		Pass.Commands.bSrgbTarget = true;
		Pass.Commands.Draws = std::move(Draws);
		InGraph.Add(std::move(Pass));
	}
	catch (const std::exception& Error)
	{
		P.Error = Error.what();
		P.Status = "Load failed: " + P.Error;
		P.bIsReady = false;
	}
}

void FModelViewerPlugin::Input(std::span<const FInputEvent> InEvents, bool bInMouseCaptured, bool bInKeyboardCaptured)
{
	auto& P = *Impl;
	for (const auto& Event : InEvents)
	{
		if (Event.Type == EEventType::Focus && !Event.bDown)
		{
			P.bDragging = false;
		}
		if (Event.Type == EEventType::MouseButton && Event.Button == 1)
		{
			P.bDragging = Event.bDown && !bInMouseCaptured;
		}
		if (Event.Type == EEventType::MouseMove)
		{
			if (P.bDragging)
			{
				P.Yaw -= (Event.X - P.LastMouse.X) * .006f;
				P.Pitch = std::clamp(P.Pitch + (Event.Y - P.LastMouse.Y) * .006f, -1.5f, 1.5f);
			}
			P.LastMouse = {Event.X, Event.Y};
		}
		if (Event.Type == EEventType::MouseWheel && !bInMouseCaptured)
		{
			P.Distance = std::clamp(P.Distance * std::pow(.85f, Event.Y), P.Radius * .15f, P.Radius * 100);
		}
		if (Event.Type == EEventType::Key && Event.bDown && !bInKeyboardCaptured)
		{
			if (Event.Key == EKey::Home)
			{
				P.bFitRequested = true;
			}
			if (Event.Key == EKey::Left)
			{
				P.Yaw -= .1f;
			}
			if (Event.Key == EKey::Right)
			{
				P.Yaw += .1f;
			}
			if (Event.Key == EKey::Up)
			{
				P.Pitch = std::min(1.5f, P.Pitch + .1f);
			}
			if (Event.Key == EKey::Down)
			{
				P.Pitch = std::max(-1.5f, P.Pitch - .1f);
			}
		}
	}
}

void FModelViewerPlugin::Stop() noexcept
{
	auto& P = *Impl;
	P.Request.Cancel();
	try
	{
		if (P.Preparation.Task())
		{
			P.Tasks.Wait(P.Preparation.Task());
		}
	}
	catch (...)
	{
	}
	try
	{
		if (P.Upload.Task())
		{
			P.Tasks.Wait(P.Upload.Task());
		}
	}
	catch (...)
	{
	}
	P.Renderer.reset();
	P.Preparation = {};
	P.Upload = {};
}

const std::string& FModelViewerPlugin::Status() const
{
	return Impl->Status;
}

const std::string& FModelViewerPlugin::Error() const
{
	return Impl->Error;
}

bool FModelViewerPlugin::Ready() const
{
	return Impl->bIsReady;
}

void RegisterModelViewerPlugin(FPluginRegistry& InRegistry, IRHIDevice& InDevice, FShaderCompiler& InCompiler,
                               FTaskSystem& InTasks, FAssetService& InAssets, const std::filesystem::path& InPath)
{
	InRegistry.Add({"model-viewer",
	                {},
	                [&InDevice, &InCompiler, &InTasks, &InAssets, Path = InPath]
	                {
		                return std::make_unique<FModelViewerPlugin>(InDevice, InCompiler, InTasks, InAssets, Path);
	                }});
}
} // namespace Hyperion
