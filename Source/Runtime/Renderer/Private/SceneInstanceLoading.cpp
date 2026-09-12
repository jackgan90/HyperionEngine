#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "SceneInstanceInternal.h"

namespace Hyperion
{
void FSceneInstance::FImpl::BeginManifest()
{
	Manifest = ManifestRequest.GetReady();
	ValidateSceneManifest(*Manifest);
	Scene.LoadNodes(NodesFromSceneManifest(*Manifest));
	Scene.SetSettings(ResolveSceneSettings(*Manifest, Scene));
	RefreshModels();
	for (const auto& Entry : Manifest->Assets)
	{
		auto& Load = Loads[Entry.Id];
		Load.Epoch = LoadEpoch;
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
		if (Load.Epoch != LoadEpoch || Load.bComplete || !Load.Preparation.Ready())
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
	Status.PublicationError = Bridge->GetSceneError();
	const auto Camera = Scene.GetSettings().DefaultCamera;
	Status.bHasActiveCamera = Camera && Scene.FindCamera(*Camera) && Scene.IsEffectivelyEnabled(*Camera);
	Status.Nodes = Scene.GetNodes().size();
	Status.Groups = Scene.CountNodes(ESceneNodeKind::Group);
	Status.Cameras = Scene.CountNodes(ESceneNodeKind::Camera);
	Status.DirectionalLights = Scene.CountNodes(ESceneNodeKind::DirectionalLight);
	Status.EnvironmentLights = Scene.CountNodes(ESceneNodeKind::EnvironmentLight);
	Status.Models = Models.size();
	Status.ReadyModels = 0;
	Status.FailedModels = 0;
	for (const auto& Model : Models)
	{
		Status.ReadyModels += Bridge->IsReady(Model.Handle) ? 1 : 0;
		const auto Load = Loads.find(Model.Asset);
		const bool bLoadFailed = Load != Loads.end() && !Load->second.Error.empty();
		const auto Selection = SelectedMaterials.find(Model.Handle);
		const bool bMaterialFailed = Selection != SelectedMaterials.end() && !Selection->second.Error.empty();
		Status.FailedModels += bLoadFailed || bMaterialFailed || !Bridge->GetError(Model.Handle).empty() ? 1 : 0;
	}
	Status.bReady =
	    Status.PublicationError.empty() && Status.bLoaded && bMaterialsComplete && Status.ReadyModels == Status.Models;
	StatusRevision = Revision;
	bStatusDirty = false;
}
} // namespace Hyperion
