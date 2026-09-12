#include "Hyperion/Core/ContentHash.h"
#include "SceneInstanceInternal.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
FSceneInstance::FImpl::FImpl(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets)
    : Session(InSession), Tasks(InTasks), Assets(InAssets)
{
	Tasks.Require({EDomain::Main});
	Bridge = std::make_unique<FSceneRenderBridge>(Scene, Session, Tasks);
	Status.bLoaded = true;
}

void FSceneInstance::FImpl::RequireOpen() const
{
	Tasks.Require({EDomain::Main});
	if (Status.bClosed)
	{
		throw std::logic_error("Scene instance is closed");
	}
}

FSceneInstance::FSceneInstance(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets)
    : Impl(std::make_unique<FImpl>(InSession, InTasks, InAssets))
{
}

FSceneInstance::~FSceneInstance()
{
	Close();
}

void FSceneInstance::Load(const std::filesystem::path& InPath)
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	if (InPath.empty())
	{
		throw std::invalid_argument("Scene manifest path is empty");
	}
	Close();
	P.Bridge = std::make_unique<FSceneRenderBridge>(P.Scene, P.Session, P.Tasks);
	P.Status = {};
	P.bStatusDirty = true;
	P.Path = std::filesystem::absolute(InPath).lexically_normal();
	P.ManifestRequest = P.Assets.LoadAsync<FSceneManifest>(P.Path);
}

void FSceneInstance::Tick()
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	if (P.Status.bClosed || !P.Status.Error.empty())
	{
		return;
	}
	try
	{
		if (!P.Status.bLoaded)
		{
			if (!P.ManifestRequest.Ready())
			{
				return;
			}
			P.BeginManifest();
		}
		P.PollModels();
		P.PollMaterials();
		P.PublishModels();
		P.Bridge->Flush();
		P.UpdateStatus();
	}
	catch (const std::exception& Failure)
	{
		P.Status.Error = Failure.what();
		P.Status.bReady = false;
	}
}

void FSceneInstance::Close()
{
	auto& P = *Impl;
	if (P.Status.bClosed)
	{
		return;
	}
	P.Tasks.Require({EDomain::Main});
	P.Status.bClosed = true;
	P.ManifestRequest.Cancel();
	P.MaterialCancellation.Cancel();
	try
	{
		P.Tasks.Wait(P.MaterialPreparation.Task());
	}
	catch (...)
	{
	}
	P.MaterialPreparation = {};
	P.SelectedMaterials.clear();
	P.PendingMaterials.clear();
	P.bMaterialsComplete = true;
	for (auto& [Id, Load] : P.Loads)
	{
		Load.Cancellation.Cancel();
		try
		{
			P.Tasks.Wait(Load.Preparation.Task());
		}
		catch (...)
		{
			// Join every admitted preparation after cancellation or failure.
		}
	}
	P.Scene.Clear();
	P.Bridge.reset();
	P.Loads.clear();
	P.Models.clear();
	P.Manifest.reset();
	P.Path.clear();
	P.Status = {};
	P.Status.bClosed = true;
}

FSceneHandle FSceneInstance::Add(FSceneModel InModel, std::string InAsset)
{
	auto& P = *Impl;
	P.RequireOpen();
	if (!InAsset.empty())
	{
		const auto Load = P.Loads.find(InAsset);
		if (Load == P.Loads.end())
		{
			throw std::invalid_argument("Unknown scene asset");
		}
		InModel.Data = Load->second.Data;
	}
	const auto Handle = P.Scene.Add(std::move(InModel));
	try
	{
		P.Models.push_back({Handle, std::move(InAsset), CreateIdentifier()});
	}
	catch (...)
	{
		P.Scene.Remove(Handle);
		throw;
	}
	P.bStatusDirty = true;
	return Handle;
}

bool FSceneInstance::Update(FSceneHandle InHandle, FSceneModel InModel)
{
	auto& P = *Impl;
	P.RequireOpen();
	const auto Existing = P.Scene.Find(InHandle);
	if (!Existing)
	{
		return false;
	}
	const auto Instance = std::find_if(P.Models.begin(), P.Models.end(),
	                                   [InHandle](const auto& InInstance)
	                                   {
		                                   return InInstance.Handle == InHandle;
	                                   });
	const auto Pending = Instance == P.Models.end() ? P.PendingMaterials.end() : P.PendingMaterials.find(Instance->Id);
	auto Edits = Pending == P.PendingMaterials.end() ? std::optional<FImpl::FPendingMaterial>{}
	                                                 : P.PrepareMaterialEdits(Pending->second, *Existing, InModel);
	const bool bDataChanged = Existing->Data != InModel.Data;
	if (!P.Scene.Update(InHandle, std::move(InModel)))
	{
		return false;
	}
	if (bDataChanged && Instance != P.Models.end())
	{
		Instance->Asset.clear(); // Explicit replacement is no longer a pending manifest instance.
		P.PendingMaterials.erase(Instance->Id);
		P.SelectedMaterials.erase(Instance->Id);
	}
	else if (Edits)
	{
		Pending->second = std::move(*Edits);
	}
	P.bStatusDirty = true;
	return true;
}

bool FSceneInstance::Remove(FSceneHandle InHandle)
{
	auto& P = *Impl;
	P.RequireOpen();
	if (!P.Scene.Remove(InHandle))
	{
		return false;
	}
	std::erase_if(P.Models,
	              [&P, InHandle](const auto& InModel)
	              {
		              if (InModel.Handle != InHandle)
		              {
			              return false;
		              }
		              P.PendingMaterials.erase(InModel.Id);
		              P.SelectedMaterials.erase(InModel.Id);
		              return true;
	              });
	P.bStatusDirty = true;
	return true;
}

const FSceneModel* FSceneInstance::Find(FSceneHandle InHandle) const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Scene.Find(InHandle);
}

std::vector<FSceneHandle> FSceneInstance::GetHandles() const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Scene.GetHandles();
}

std::span<const FSceneInstanceModel> FSceneInstance::GetModels() const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Models;
}

std::vector<FSceneInstanceAsset> FSceneInstance::GetAssets() const
{
	Impl->Tasks.Require({EDomain::Main});
	std::vector<FSceneInstanceAsset> Result;
	for (const auto& [Id, Load] : Impl->Loads)
	{
		Result.push_back({Id, Load.Data, Load.Error});
	}
	return Result;
}

std::shared_ptr<const FSceneManifest> FSceneInstance::GetManifest() const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Manifest;
}

const FSceneInstanceStatus& FSceneInstance::GetStatus() const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Status;
}

std::string FSceneInstance::GetError(FSceneHandle InHandle) const
{
	Impl->Tasks.Require({EDomain::Main});
	if (!Impl->Scene.Find(InHandle))
	{
		return {};
	}
	for (const auto& Model : Impl->Models)
	{
		if (Model.Handle == InHandle)
		{
			const auto Selection = Impl->SelectedMaterials.find(Model.Id);
			if (Selection != Impl->SelectedMaterials.end() && !Selection->second.Error.empty())
			{
				return Selection->second.Error;
			}
			const auto Load = Impl->Loads.find(Model.Asset);
			if (Load != Impl->Loads.end() && !Load->second.Error.empty())
			{
				return Load->second.Error;
			}
			break;
		}
	}
	return Impl->Bridge ? Impl->Bridge->GetError(InHandle) : std::string{};
}

std::vector<FRenderDrawResult> FSceneInstance::GetDrawResults(FSceneHandle InHandle) const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Bridge ? Impl->Bridge->GetDrawResults(InHandle) : std::vector<FRenderDrawResult>{};
}
} // namespace Hyperion
