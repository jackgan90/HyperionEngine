#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "SceneViewerInternal.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
FSceneViewerPlugin::FSceneViewerPlugin(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets,
                                       std::filesystem::path InPath)
    : Impl(std::make_unique<FImpl>(InSession, InTasks, InAssets, std::move(InPath)))
{
	InTasks.Require({EDomain::Main});
	Impl->Owner = this;
}

FSceneViewerPlugin::~FSceneViewerPlugin() = default;

void FSceneViewerPlugin::Start()
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	P.Scene.Load(P.Path);
}

void FSceneViewerPlugin::FImpl::BeginManifest()
{
	Manifest = Scene.GetManifest();
	Target = Manifest->Target;
	const auto Direction = Subtract(Manifest->Eye, Target);
	Distance = Length(Direction);
	Yaw = std::atan2(Direction.X, Direction.Z);
	Pitch = std::asin(std::clamp(Direction.Y / Distance, -.99f, .99f));
}

void FSceneViewerPlugin::Update(FRenderFrame& InFrame)
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	P.PollSave();
	if (P.bStopped || !P.Error.empty())
	{
		return;
	}
	try
	{
		if (!P.Manifest)
		{
			P.Scene.Tick();
			if (!P.Scene.GetStatus().Error.empty())
			{
				throw std::runtime_error(P.Scene.GetStatus().Error);
			}
			if (!P.Scene.GetManifest())
			{
				return;
			}
			P.BeginManifest();
		}
		if (P.bAnimate)
		{
			const float Previous = std::sin(P.AnimationTime);
			P.AnimationTime += .02f;
			MoveSelected((std::sin(P.AnimationTime) - Previous) * 30);
		}
		P.Scene.Tick();
		const auto& State = P.Scene.GetStatus();
		if (!State.Error.empty())
		{
			throw std::runtime_error(State.Error);
		}
		P.UpdateCamera(InFrame);
		P.Status = std::to_string(State.ReadyModels) + "/" + std::to_string(State.Models) + " models ready | " +
		           std::to_string(State.FailedModels) + " failed";
	}
	catch (const std::exception& Failure)
	{
		P.Error = Failure.what();
		P.Status = "Scene failed: " + P.Error;
	}
}

void FSceneViewerPlugin::Stop() noexcept
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	if (P.bStopped)
	{
		return;
	}
	if (P.Save)
	{
		try
		{
			P.Save->Get(P.Tasks);
		}
		catch (...)
		{
		}
		P.PollSave();
	}
	P.bStopped = true;
	P.Scene.Close();
}

bool FSceneViewerPlugin::Ready() const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Scene.GetStatus().bReady;
}

const std::string& FSceneViewerPlugin::Status() const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Status;
}

const std::string& FSceneViewerPlugin::Error() const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Error;
}

std::size_t FSceneViewerPlugin::ModelCount() const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Scene.GetModels().size();
}

void RegisterSceneViewerPlugin(FPluginRegistry& InRegistry, FRenderSession& InSession, FTaskSystem& InTasks,
                               FAssetService& InAssets, const std::filesystem::path& InPath)
{
	InRegistry.Add({"scene-viewer",
	                {},
	                [&InSession, &InTasks, &InAssets, Path = InPath]
	                {
		                return std::make_unique<FSceneViewerPlugin>(InSession, InTasks, InAssets, Path);
	                }});
}
} // namespace Hyperion
