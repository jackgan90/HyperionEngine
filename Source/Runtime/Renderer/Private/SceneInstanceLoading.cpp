#include "Hyperion/Core/Profiling.h"
#include "SceneInstanceInternal.h"

namespace Hyperion
{
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

void FSceneInstance::FImpl::BeginManifest()
{
	Manifest = ManifestRequest.GetReady();
	for (const auto& Entry : Manifest->Instances)
	{
		FSceneModel Model;
		Model.Name = Entry.Id;
		Model.World = ComposeTRS(Entry.Translation, Entry.Rotation, Entry.Scale);
		Model.bVisible = Entry.bVisible;
		Models.push_back({Scene.Add(std::move(Model)), Entry.Asset});
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
	Status.bLoaded = true;
	bStatusDirty = true;
}

void FSceneInstance::FImpl::PollModels()
{
	for (auto& [Id, Load] : Loads)
	{
		if (Load.bComplete || !Load.Preparation.Ready())
		{
			continue;
		}
		try
		{
			Load.Data = Load.Preparation.GetReady();
			for (const auto& Instance : Models)
			{
				const auto Model = Scene.Find(Instance.Handle);
				if (Model && Instance.Asset == Id)
				{
					auto Updated = *Model;
					Updated.Data = Load.Data;
					Scene.Update(Instance.Handle, std::move(Updated));
				}
			}
		}
		catch (const std::exception& Failure)
		{
			Load.Error = Failure.what();
		}
		Load.bComplete = true;
		bStatusDirty = true;
	}
}

void FSceneInstance::FImpl::UpdateStatus()
{
	HYP_PERF_SCOPE_C(Frame, UpdateSceneStatus);
	const auto Revision = Bridge->GetStatusRevision();
	if (!bStatusDirty && Status.bReady && StatusRevision == Revision)
	{
		return;
	}
	Status.Models = Models.size();
	Status.ReadyModels = 0;
	Status.FailedModels = 0;
	for (const auto& Model : Models)
	{
		Status.ReadyModels += Bridge->IsReady(Model.Handle) ? 1 : 0;
		const auto Load = Loads.find(Model.Asset);
		const bool bLoadFailed = Load != Loads.end() && !Load->second.Error.empty();
		Status.FailedModels += bLoadFailed || !Bridge->GetError(Model.Handle).empty() ? 1 : 0;
	}
	Status.bReady = Status.bLoaded && Status.ReadyModels == Status.Models;
	StatusRevision = Revision;
	bStatusDirty = false;
}
} // namespace Hyperion
