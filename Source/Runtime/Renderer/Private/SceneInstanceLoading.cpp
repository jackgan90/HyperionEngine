#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "SceneInstanceInternal.h"

namespace Hyperion
{
void FSceneInstance::FImpl::BeginManifest()
{
	Manifest = ManifestRequest.GetReady();
	for (const auto& Entry : Manifest->Instances)
	{
		FSceneModel Model;
		Model.Name = Entry.Name.empty() ? Entry.Id : Entry.Name;
		Model.World = Entry.Transform;
		Model.Material = Entry.Material;
		Model.bVisible = Entry.bVisible;
		Models.push_back({Scene.Add(std::move(Model)), Entry.Asset, Entry.Id});
	}
	for (const auto& Entry : Manifest->Assets)
	{
		auto& Load = Loads[Entry.Id];
		try
		{
			Load.Preparation =
			    LoadNativeModel(Assets, Tasks, Entry.Reference, Path, Load.Cancellation, &Session.GetResources());
		}
		catch (const std::exception& Failure)
		{
			Load.Error = Failure.what();
			Load.bComplete = true;
		}
	}
	BeginMaterials();
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
		const auto Selection = SelectedMaterials.find(Model.Id);
		const bool bMaterialFailed = Selection != SelectedMaterials.end() && !Selection->second.Error.empty();
		Status.FailedModels += bLoadFailed || bMaterialFailed || !Bridge->GetError(Model.Handle).empty() ? 1 : 0;
	}
	Status.bReady = Status.bLoaded && bMaterialsComplete && Status.ReadyModels == Status.Models;
	StatusRevision = Revision;
	bStatusDirty = false;
}
} // namespace Hyperion
