#include "Hyperion/ModelViewer/ModelViewerPlugin.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/SceneBridge.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
struct FModelViewerPlugin::FImpl
{
	FImpl(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets, std::filesystem::path InPath)
	    : Session(InSession), Tasks(InTasks), Assets(InAssets), Path(std::move(InPath))
	{
	}

	struct FLoadedModel
	{
		std::shared_ptr<const FSceneModelData> Data;
		FBounds Bounds;
	};

	FRenderSession& Session;
	FTaskSystem& Tasks;
	FAssetService& Assets;
	std::filesystem::path Path;
	TAssetRequest<FModelAsset> Request;
	TAsyncResult<FLoadedModel> Preparation;
	FScene Scene;
	FSceneHandle Model;
	std::unique_ptr<FSceneRenderBridge> Bridge;
	std::string Status = "Loading model...";
	std::string Error;
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

FModelViewerPlugin::FModelViewerPlugin(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets,
                                       std::filesystem::path InPath)
    : Impl(std::make_unique<FImpl>(InSession, InTasks, InAssets, std::move(InPath)))
{
	InTasks.Require({EDomain::Main});
}

FModelViewerPlugin::~FModelViewerPlugin() = default;

void FModelViewerPlugin::Start()
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	P.Bridge = std::make_unique<FSceneRenderBridge>(P.Scene, P.Session, P.Tasks);
	P.Request = P.Assets.LoadAsync<FModelAsset>(P.Path);
	P.Preparation = DispatchAsync<FImpl::FLoadedModel>(P.Tasks, {EDomain::Worker},
	                                                   [Request = P.Request, Tasks = &P.Tasks]
	                                                   {
		                                                   auto Asset = Request.Get(*Tasks);
		                                                   const auto Data = PrepareSceneModel(Asset);
		                                                   return FImpl::FLoadedModel{Data, Data->Bounds};
	                                                   });
}

void FModelViewerPlugin::Update(FRenderFrame& InFrame)
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	if (!P.Error.empty())
	{
		return;
	}
	try
	{
		if (!P.Model.Generation)
		{
			if (!P.Preparation.Ready())
			{
				return;
			}
			const auto Loaded = P.Preparation.GetReady();
			const auto& Bounds = Loaded->Bounds;
			P.Center = ScaleVector(Add(Bounds.Minimum, Bounds.Maximum), .5f);
			P.Radius = std::max(.01f, Length(Subtract(Bounds.Maximum, P.Center)));
			P.Model = P.Scene.Add(FSceneModel{Loaded->Data->Asset->Name, Loaded->Data});
			P.Preparation = {};
			P.Status = "Uploading model to GPU...";
		}
		P.Bridge->Flush();
		if (auto Error = P.Bridge->GetError(P.Model); !Error.empty())
		{
			throw std::runtime_error(Error);
		}
		const float Aspect = float(std::max(1u, InFrame.Size.Width)) / std::max(1u, InFrame.Size.Height);
		if (P.bFitRequested)
		{
			const float Limit = std::min(.5f, std::atan(std::tan(.5f) * Aspect));
			P.Distance = P.Radius / std::sin(Limit) * 1.12f;
			P.bFitRequested = false;
		}
		const FVec3 Direction{std::sin(P.Yaw) * std::cos(P.Pitch), std::sin(P.Pitch),
		                      std::cos(P.Yaw) * std::cos(P.Pitch)};
		const FVec3 Eye = Add(P.Center, ScaleVector(Direction, P.Distance));
		InFrame.View.ViewProjection =
		    Multiply(Perspective(1, Aspect, std::max(.0001f, P.Radius * .001f), P.Distance + P.Radius * 10),
		             LookAt(Eye, P.Center));
		InFrame.View.Eye = Eye;
		P.bIsReady = P.Bridge->IsReady(P.Model);
		if (P.bIsReady)
		{
			P.Status = "Ready | " + std::to_string(P.Bridge->PrimitiveCount(P.Model)) + " primitives";
		}
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
	P.Tasks.Require({EDomain::Main});
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
	P.Tasks.Require({EDomain::Main});
	P.Request.Cancel();
	try
	{
		P.Tasks.Wait(P.Preparation.Task());
	}
	catch (...)
	{
		// Cancellation or import failure still joins the producer before the plugin is released.
	}
	P.Preparation = {};
	P.Scene.Clear();
	P.Bridge.reset();
	P.Model = {};
	P.bIsReady = false;
}

const std::string& FModelViewerPlugin::Status() const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Status;
}

const std::string& FModelViewerPlugin::Error() const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Error;
}

bool FModelViewerPlugin::Ready() const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->bIsReady;
}

void RegisterModelViewerPlugin(FPluginRegistry& InRegistry, FRenderSession& InSession, FTaskSystem& InTasks,
                               FAssetService& InAssets, const std::filesystem::path& InPath)
{
	InRegistry.Add({"model-viewer",
	                {},
	                [&InSession, &InTasks, &InAssets, Path = InPath]
	                {
		                return std::make_unique<FModelViewerPlugin>(InSession, InTasks, InAssets, Path);
	                }});
}
} // namespace Hyperion
