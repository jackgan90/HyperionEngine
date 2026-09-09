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
}

FSceneViewerPlugin::~FSceneViewerPlugin() = default;

void RegisterSceneManifestLoader(FAssetService& InAssets)
{
	InAssets.Register({RecordType<FSceneManifest>().Id,
	                   {".json"},
	                   [](FAssetLoadContext& InContext)
	                   {
		                   const auto& Bytes = *InContext.Bytes;
		                   return std::make_shared<FSceneManifest>(
		                       DecodeSceneManifest({reinterpret_cast<const char*>(Bytes.data()), Bytes.size()}));
	                   }});
}

void FSceneViewerPlugin::Start()
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	P.Bridge = std::make_unique<FSceneRenderBridge>(P.Scene, P.Session, P.Tasks);
	P.ManifestRequest = P.Assets.LoadAsync<FSceneManifest>(P.Path);
}

void FSceneViewerPlugin::FImpl::BeginManifest()
{
	Manifest = ManifestRequest.GetReady();
	Target = Manifest->Target;
	const auto Direction = Subtract(Manifest->Eye, Target);
	Distance = Length(Direction);
	Yaw = std::atan2(Direction.X, Direction.Z);
	Pitch = std::asin(std::clamp(Direction.Y / Distance, -.99f, .99f));
	for (const auto& Entry : Manifest->Instances)
	{
		FSceneModel Model;
		Model.Name = Entry.Id;
		Model.World = ComposeTRS(Entry.Translation, Entry.Rotation, Entry.Scale);
		Model.bVisible = Entry.bVisible;
		Instances.push_back({Scene.Add(std::move(Model)), Entry.Asset});
	}
	for (const auto& Entry : Manifest->Assets)
	{
		auto& Load = Loads[Entry.Id];
		Load.Request = Assets.LoadAsync<FModelAsset>(Path.parent_path() / Entry.Path);
		Load.Preparation = DispatchAsync<FSceneModelData>(Tasks, {EDomain::Worker},
		                                                  [Request = Load.Request, Tasks = &Tasks]
		                                                  {
			                                                  return *PrepareSceneModel(Request.Get(*Tasks));
		                                                  });
	}
}

void FSceneViewerPlugin::FImpl::PollModels()
{
	for (auto& [Id, Load] : Loads)
	{
		if (Load.bComplete || !Load.Preparation.Ready())
		{
			continue;
		}
		try
		{
			const auto Data = Load.Preparation.GetReady();
			for (const auto& Instance : Instances)
			{
				const auto Model = Scene.Find(Instance.Handle);
				if (Model && Instance.Asset == Id)
				{
					auto Updated = *Model;
					Updated.Data = Data;
					Scene.Update(Instance.Handle, std::move(Updated));
				}
			}
		}
		catch (const std::exception& Failure)
		{
			Load.Error = Failure.what();
		}
		Load.bComplete = true;
	}
}

void FSceneViewerPlugin::FImpl::UpdateStatus()
{
	HYP_PERF_SCOPE_C(Frame, UpdateSceneStatus);
	const auto Revision = Bridge->GetStatusRevision();
	if (bReady && StatusRevision == Revision)
	{
		return;
	}
	std::size_t ReadyCount{};
	std::size_t FailedCount{};
	for (const auto& Instance : Instances)
	{
		ReadyCount += Bridge->IsReady(Instance.Handle) ? 1 : 0;
		FailedCount += !Loads.at(Instance.Asset).Error.empty() || !Bridge->GetError(Instance.Handle).empty() ? 1 : 0;
	}
	bReady = ReadyCount == Instances.size();
	StatusRevision = Revision;
	Status = std::to_string(ReadyCount) + "/" + std::to_string(Instances.size()) + " models ready | " +
	         std::to_string(FailedCount) + " failed";
}

void FSceneViewerPlugin::Update(FRenderFrame& InFrame)
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	if (P.bStopped || !P.Error.empty())
	{
		return;
	}
	try
	{
		if (!P.Manifest)
		{
			if (!P.ManifestRequest.Ready())
			{
				return;
			}
			P.BeginManifest();
		}
		P.PollModels();
		if (P.bAnimate)
		{
			const float Previous = std::sin(P.AnimationTime);
			P.AnimationTime += .02f;
			MoveSelected((std::sin(P.AnimationTime) - Previous) * 30);
		}
		P.Bridge->Flush();
		P.UpdateCamera(InFrame);
		P.UpdateStatus();
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
	P.bStopped = true;
	P.ManifestRequest.Cancel();
	for (auto& [Id, Load] : P.Loads)
	{
		Load.Request.Cancel();
		try
		{
			P.Tasks.Wait(Load.Preparation.Task());
		}
		catch (...)
		{
			// Join every preparation even after cancellation or failure.
		}
	}
	P.Scene.Clear();
	P.Bridge.reset();
	P.Loads.clear();
	P.Instances.clear();
	P.bReady = false;
}

bool FSceneViewerPlugin::Ready() const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->bReady;
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
	return Impl->Instances.size();
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
