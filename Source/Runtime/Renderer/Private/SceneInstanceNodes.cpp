#include "SceneInstanceInternal.h"
#include <algorithm>

namespace Hyperion
{
void FSceneInstance::FImpl::RefreshModels()
{
	std::vector<FSceneInstanceModel> Updated;
	for (const auto Handle : Scene.GetNodes(ESceneNodeKind::Model))
	{
		const auto& Node = *Scene.FindNode(Handle);
		Updated.push_back({Handle, Node.Model->Asset, Node.Id});
	}
	Models = std::move(Updated);
	bModelStatusDirty = true;
}

void FSceneInstance::FImpl::ForgetRemovedModels()
{
	std::erase_if(PendingMaterials,
	              [this](const auto& InEntry)
	              {
		              return !Scene.FindNode(InEntry.first);
	              });
	std::erase_if(SelectedMaterials,
	              [this](const auto& InEntry)
	              {
		              return !Scene.FindNode(InEntry.first);
	              });
	std::erase_if(Models,
	              [this](const auto& InModel)
	              {
		              return !Scene.FindNode(InModel.Handle);
	              });
	bModelStatusDirty = true;
}

FSceneHandle FSceneInstance::FImpl::AddNode(FSceneNode InNode, bool bInResolveData)
{
	if (InNode.Model && !InNode.Model->Asset.empty())
	{
		const auto Load = Loads.find(InNode.Model->Asset);
		if (Load == Loads.end())
		{
			throw std::invalid_argument("Unknown scene asset");
		}
		if (bInResolveData)
		{
			InNode.Model->Data = Load->second.Data;
		}
	}
	Models.reserve(Models.size() + (InNode.Model ? 1 : 0));
	const auto Handle = Scene.AddNode(std::move(InNode));
	const auto& Node = *Scene.FindNode(Handle);
	if (Node.Model)
	{
		Models.push_back({Handle, Node.Model->Asset, Node.Id});
	}
	bModelStatusDirty = true;
	return Handle;
}

FSceneHandle FSceneInstance::AddNode(FSceneNode InNode)
{
	Impl->RequireOpen();
	return Impl->AddNode(std::move(InNode), true);
}

FSceneHandle FSceneInstance::DuplicateNode(FSceneHandle InHandle)
{
	auto& P = *Impl;
	P.RequireOpen();
	const auto* Source = P.Scene.FindNode(InHandle);
	if (!Source)
	{
		return {};
	}
	auto Node = *Source;
	Node.Id.clear();
	Node.Name += " copy";
	// Allocate material bookkeeping before inserting the logical node. Copies keep their own edit masks.
	decltype(P.PendingMaterials) Pending;
	decltype(P.SelectedMaterials) Selected;
	if (const auto It = P.PendingMaterials.find(InHandle); It != P.PendingMaterials.end())
	{
		Pending.emplace(InHandle, It->second);
	}
	if (const auto It = P.SelectedMaterials.find(InHandle); It != P.SelectedMaterials.end())
	{
		Selected.emplace(InHandle, It->second);
	}
	// Do not install ready geometry ahead of an original node that is still awaiting material preparation.
	const auto Handle = P.AddNode(std::move(Node), false);
	if (!Pending.empty())
	{
		auto Entry = Pending.extract(Pending.begin());
		Entry.key() = Handle;
		P.PendingMaterials.insert(std::move(Entry));
	}
	if (!Selected.empty())
	{
		auto Entry = Selected.extract(Selected.begin());
		Entry.key() = Handle;
		Entry.mapped().Handle = Handle;
		P.SelectedMaterials.insert(std::move(Entry));
	}
	return Handle;
}

bool FSceneInstance::SetModelComponent(FSceneHandle InHandle, FSceneModelComponent InModel)
{
	auto& P = *Impl;
	P.RequireOpen();
	const auto Existing = P.Scene.FindModelComponent(InHandle);
	if (!Existing)
	{
		return false;
	}
	const bool bDataChanged = Existing->Data != InModel.Data;
	if (InModel.Asset != Existing->Asset)
	{
		throw std::invalid_argument("Model asset association cannot be reassigned by a content edit");
	}
	if (bDataChanged)
	{
		InModel.Asset.clear();
	}
	const auto Pending = P.PendingMaterials.find(InHandle);
	auto Edits = Pending == P.PendingMaterials.end() ? std::optional<FImpl::FPendingMaterial>{}
	                                                 : P.PrepareMaterialEdits(Pending->second, *Existing, InModel);
	const bool bResult = P.Scene.SetModelComponent(InHandle, std::move(InModel));
	if (bDataChanged)
	{
		P.PendingMaterials.erase(InHandle);
		P.SelectedMaterials.erase(InHandle);
		for (auto& Model : P.Models)
		{
			if (Model.Handle == InHandle)
			{
				Model.Asset.clear();
			}
		}
	}
	else if (Edits)
	{
		Pending->second = std::move(*Edits);
	}
	P.bModelStatusDirty |= bResult;
	return bResult;
}

bool FSceneInstance::RemoveSubtree(FSceneHandle InHandle)
{
	Impl->RequireOpen();
	if (!Impl->Scene.RemoveSubtree(InHandle))
	{
		return false;
	}
	Impl->ForgetRemovedModels();
	return true;
}

bool FSceneInstance::RemoveNodeKeepChildren(FSceneHandle InHandle)
{
	Impl->RequireOpen();
	if (!Impl->Scene.RemoveNodeKeepChildren(InHandle))
	{
		return false;
	}
	Impl->ForgetRemovedModels();
	return true;
}

const FSceneNode* FSceneInstance::FindNode(FSceneHandle InHandle) const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Scene.FindNode(InHandle);
}

FSceneHandle FSceneInstance::FindHandle(std::string_view InId) const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Scene.FindHandle(InId);
}

bool FSceneInstance::GetNodeView(FSceneHandle InHandle, FSceneNodeView& OutView) const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Scene.GetNodeView(InHandle, OutView);
}

std::vector<FSceneHandle> FSceneInstance::GetNodes() const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Scene.GetNodes();
}

std::vector<FSceneHandle> FSceneInstance::GetNodes(ESceneNodeKind InKind) const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Scene.GetNodes(InKind);
}

std::vector<FSceneHandle> FSceneInstance::GetRoots() const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Scene.GetRoots();
}

std::vector<FSceneHandle> FSceneInstance::GetChildren(FSceneHandle InHandle) const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Scene.GetChildren(InHandle);
}

bool FSceneInstance::GetCameraPose(FSceneHandle InHandle, FSceneCameraPose& OutPose) const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Scene.GetCameraPose(InHandle, OutPose);
}

const FSceneSettings& FSceneInstance::GetSettings() const
{
	Impl->Tasks.Require({EDomain::Main});
	return Impl->Scene.GetSettings();
}

bool FSceneInstance::SetName(FSceneHandle InHandle, std::string InName)
{
	Impl->RequireOpen();
	const bool bResult = Impl->Scene.SetName(InHandle, std::move(InName));
	return bResult;
}

bool FSceneInstance::SetEnabled(FSceneHandle InHandle, bool bInEnabled)
{
	Impl->RequireOpen();
	const bool bResult = Impl->Scene.SetEnabled(InHandle, bInEnabled);
	return bResult;
}

bool FSceneInstance::SetModelVisible(FSceneHandle InHandle, bool bInVisible)
{
	Impl->RequireOpen();
	const bool bResult = Impl->Scene.SetModelVisible(InHandle, bInVisible);
	return bResult;
}

bool FSceneInstance::SetCameraView(FSceneHandle InHandle, FMat4 InWorld, FSceneCamera InCamera)
{
	Impl->RequireOpen();
	const bool bResult = Impl->Scene.SetCameraView(InHandle, InWorld, InCamera);
	return bResult;
}

bool FSceneInstance::SetCamera(FSceneHandle InHandle, FSceneCamera InCamera)
{
	Impl->RequireOpen();
	const bool bResult = Impl->Scene.SetCamera(InHandle, InCamera);
	return bResult;
}

bool FSceneInstance::SetDirectionalLight(FSceneHandle InHandle, FSceneDirectionalLight InLight)
{
	Impl->RequireOpen();
	const bool bResult = Impl->Scene.SetDirectionalLight(InHandle, InLight);
	return bResult;
}

bool FSceneInstance::SetEnvironmentLight(FSceneHandle InHandle, FSceneEnvironmentLight InLight)
{
	Impl->RequireOpen();
	const bool bResult = Impl->Scene.SetEnvironmentLight(InHandle, InLight);
	return bResult;
}

bool FSceneInstance::SetPointLight(FSceneHandle InHandle, FScenePointLight InLight)
{
	Impl->RequireOpen();
	return Impl->Scene.SetPointLight(InHandle, InLight);
}

bool FSceneInstance::SetSpotLight(FSceneHandle InHandle, FSceneSpotLight InLight)
{
	Impl->RequireOpen();
	return Impl->Scene.SetSpotLight(InHandle, InLight);
}

bool FSceneInstance::SetLocalTransform(FSceneHandle InHandle, FMat4 InLocal)
{
	Impl->RequireOpen();
	const bool bResult = Impl->Scene.SetLocalTransform(InHandle, InLocal);
	return bResult;
}

bool FSceneInstance::SetWorldTransform(FSceneHandle InHandle, FMat4 InWorld)
{
	Impl->RequireOpen();
	const bool bResult = Impl->Scene.SetWorldTransform(InHandle, InWorld);
	return bResult;
}

bool FSceneInstance::Reparent(FSceneHandle InHandle, std::optional<FSceneHandle> InParent, ESceneReparentMode InMode)
{
	Impl->RequireOpen();
	const bool bResult = Impl->Scene.Reparent(InHandle, InParent, InMode);
	return bResult;
}

bool FSceneInstance::SetSettings(FSceneSettings InSettings)
{
	Impl->RequireOpen();
	const bool bResult = Impl->Scene.SetSettings(InSettings);
	return bResult;
}

FScenePublicationToken FSceneInstance::GetToken() const
{
	Impl->RequireOpen();
	return Impl->Bridge->GetToken();
}

FTaskHandle FSceneInstance::GetReceipt() const
{
	Impl->RequireOpen();
	return Impl->Bridge->GetReceipt();
}
} // namespace Hyperion
