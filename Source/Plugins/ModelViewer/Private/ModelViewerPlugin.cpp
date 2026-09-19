#include "Hyperion/ModelViewer/ModelViewerPlugin.h"
#include "Hyperion/Renderer/NativeModel.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/SceneCameraController.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
struct FModelViewerPlugin::FImpl
{
	FImpl(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets, std::filesystem::path InPath)
	    : Session(InSession), Tasks(InTasks), Assets(InAssets), Path(std::move(InPath)),
	      Scene(InSession, InTasks, InAssets)
	{
	}

	FRenderSession& Session;
	FTaskSystem& Tasks;
	FAssetService& Assets;
	std::filesystem::path Path;
	FCancellationToken Cancellation;
	TAsyncResult<FSceneModelData> Preparation;
	FSceneInstance Scene;
	FSceneHandle Model;
	std::string Status = "Loading model...";
	std::string Error;
	bool bIsReady{};
	FSceneCameraController CameraController;
	bool bFitRequested = true;
	FVec3 Center;
	float Radius = 1;
};

FModelViewerPlugin::FModelViewerPlugin(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets,
                                       std::filesystem::path InPath)
    : Impl(std::make_unique<FImpl>(InSession, InTasks, InAssets, std::move(InPath)))
{
	InTasks.Require({EDomain::Main});
}

FModelViewerPlugin::~FModelViewerPlugin() = default;

FSceneInstance& FModelViewerPlugin::GetSceneInstance()
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Scene;
}

void FModelViewerPlugin::Start()
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	const FVec3 Direction{std::sin(.35f) * std::cos(.3f), std::sin(.3f), std::cos(.35f) * std::cos(.3f)};
	FSceneCamera Camera;
	Camera.VerticalRadians = 1;
	const auto CameraHandle = P.Scene.AddNode(MakeSceneCameraNode({}, ScaleVector(Direction, 5), {}, Camera));
	const auto Light = P.Scene.AddNode(MakeSceneDirectionalLightNode({}));
	const auto Environment = P.Scene.AddNode(MakeSceneEnvironmentLightNode({}));
	P.Scene.SetSettings({CameraHandle, Light, Environment});
	P.Cancellation = {};
	P.CameraController.Reset();
	P.Preparation = LoadNativeModel(P.Assets, P.Tasks, P.Path, P.Cancellation, &P.Session.GetResources());
}

void FModelViewerPlugin::Update(FRenderFrame& InFrame)
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	FSceneViewRequest Request;
	Request.Width = InFrame.Size.Width;
	Request.Height = InFrame.Size.Height;
	Request.DepthConvention = InFrame.View.DepthConvention;
	InFrame.SceneView = Request;
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
				P.Scene.Tick();
				return;
			}
			const auto Loaded = P.Preparation.GetReady();
			const auto& Bounds = Loaded->Bounds;
			P.Center = ScaleVector(Add(Bounds.Minimum, Bounds.Maximum), .5f);
			P.Radius = std::max(.01f, Length(Subtract(Bounds.Maximum, P.Center)));
			P.Model = P.Scene.Add(FSceneModel{Loaded->Asset->Name, Loaded});
			P.Preparation = {};
			P.Status = "Uploading model to GPU...";
		}
		const float Aspect = float(std::max(1u, InFrame.Size.Width)) / std::max(1u, InFrame.Size.Height);
		if (P.bFitRequested)
		{
			FitSceneCamera(P.Scene, Aspect, true);
			P.bFitRequested = false;
		}
		P.Scene.Tick();
		if (auto Error = P.Scene.GetError(P.Model); !Error.empty())
		{
			throw std::runtime_error(Error);
		}
		if (!P.Scene.GetStatus().Error.empty())
		{
			throw std::runtime_error(P.Scene.GetStatus().Error);
		}
		P.bIsReady = P.Scene.GetStatus().bReady;
		if (P.bIsReady)
		{
			P.Status = "Ready | " + std::to_string(P.Scene.Find(P.Model)->Data->Instances.size()) + " primitives";
			for (const auto& Draw : P.Scene.GetDrawResults(P.Model))
			{
				if (!Draw.Error.empty())
				{
					P.Status = "Draw unavailable (view " + std::to_string(Draw.View) + "): " + Draw.Error;
					break;
				}
			}
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
	P.CameraController.Input(P.Scene, {}, bInMouseCaptured, bInKeyboardCaptured);
	for (const auto& Event : InEvents)
	{
		if (Event.Type == EEventType::Focus || Event.Type == EEventType::MouseButton ||
		    Event.Type == EEventType::MouseMove)
		{
			P.CameraController.Input(P.Scene, std::span(&Event, 1), bInMouseCaptured, bInKeyboardCaptured);
		}
		if (Event.Type == EEventType::MouseWheel && !bInMouseCaptured)
		{
			DollySceneCamera(P.Scene, std::pow(.85f, Event.Y), P.Radius * .15f, P.Radius * 100, P.Radius * 10);
		}
		if (Event.Type == EEventType::Key && Event.bDown && !bInKeyboardCaptured)
		{
			if (Event.Key == EKey::Home)
			{
				P.bFitRequested = true;
			}
			if (Event.Key == EKey::Left)
			{
				OrbitSceneCamera(P.Scene, -.1f, 0);
			}
			if (Event.Key == EKey::Right)
			{
				OrbitSceneCamera(P.Scene, .1f, 0);
			}
			if (Event.Key == EKey::Up)
			{
				OrbitSceneCamera(P.Scene, 0, .1f);
			}
			if (Event.Key == EKey::Down)
			{
				OrbitSceneCamera(P.Scene, 0, -.1f);
			}
		}
	}
}

void FModelViewerPlugin::Stop() noexcept
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	P.Cancellation.Cancel();
	P.CameraController.Reset();
	try
	{
		P.Tasks.Wait(P.Preparation.Task());
	}
	catch (...)
	{
		// Cancellation or import failure still joins the producer before the plugin is released.
	}
	P.Preparation = {};
	P.Scene.Close();
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
	FPluginDescriptor Descriptor{"model-viewer",
	                             {},
	                             [&InSession, &InTasks, &InAssets, Path = InPath]
	                             {
		                             return std::make_unique<FModelViewerPlugin>(InSession, InTasks, InAssets, Path);
	                             }};
	Descriptor.Provides = {typeid(IScenePlugin)};
	InRegistry.Add(std::move(Descriptor));
}

void RegisterModelViewerPlugin(FPluginRegistry& InRegistry, const std::filesystem::path& InPath)
{
	FPluginDescriptor Descriptor;
	Descriptor.Id = "model-viewer";
	Descriptor.Dependencies = {"graphics", "assets"};
	Descriptor.Provides = {typeid(IScenePlugin)};
	Descriptor.Requires = {typeid(FRenderSession), typeid(FTaskSystem), typeid(FAssetService)};
	Descriptor.CreateWithContext = [Path = InPath](FPluginContext& InContext)
	{
		return std::make_unique<FModelViewerPlugin>(InContext.Require<FRenderSession>(),
		                                            InContext.Require<FTaskSystem>(),
		                                            InContext.Require<FAssetService>(), Path);
	};
	InRegistry.Add(std::move(Descriptor));
}
} // namespace Hyperion
