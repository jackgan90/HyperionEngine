#include "SceneInstanceInternal.h"
#include <algorithm>

namespace Hyperion
{
bool FSceneInstance::EditNodes(std::vector<FSceneNodeEdit> InEdits, std::uint64_t InExpectedRevision)
{
	auto& P = *Impl;
	P.RequireOpen();
	if (P.Scene.GetRevision() != InExpectedRevision)
	{
		return false;
	}
	std::vector<FSceneInstanceModel> AddedModels;
	std::vector<FSceneHandle> RetiredSkies;
	bool bModelsChanged{};
	for (auto& [Handle, Node] : InEdits)
	{
		const auto* Existing = P.Scene.FindNode(Handle);
		if (!Existing)
		{
			return false;
		}
		if (Node.Model() && Existing->Model() &&
		    (Node.Model()->Asset != Existing->Model()->Asset || Node.Model()->Data != Existing->Model()->Data))
		{
			throw std::invalid_argument("Use asset instantiation to attach or replace model geometry");
		}
		if (P.PendingMaterials.contains(Handle))
		{
			throw std::invalid_argument("Wait for material loading before editing this object");
		}
		const bool bModelChanged = Existing->Model().has_value() != Node.Model().has_value();
		bModelsChanged |= bModelChanged;
		if (bModelChanged && Node.Model())
		{
			const auto Load = P.Loads.find(Node.Model()->Asset);
			if (Load == P.Loads.end() || !Load->second.Data || Node.Model()->Data != Load->second.Data)
			{
				throw std::invalid_argument("Static Mesh must select ready geometry from the scene asset table");
			}
			AddedModels.push_back({Handle, Node.Model()->Asset, Node.Id});
		}
		const auto Sky = P.SkyLoads.find(Handle);
		const auto& Previous = Existing->EnvironmentLight();
		auto& Light = Node.EnvironmentLight();
		if (Sky != P.SkyLoads.end() && (!Light || !Previous || Light->Source != Previous->Source ||
		                                Light->Sky != Previous->Sky || !Sky->second->Error.empty()))
		{
			RetiredSkies.push_back(Handle);
		}
		if (Light)
		{
			Light->Data = Light->Source == ESceneEnvironmentSource::SkyAsset && Previous ? Previous->Data : nullptr;
		}
	}
	P.Models.reserve(P.Models.size() + AddedModels.size());
	P.RetiredSkyLoads.reserve(P.RetiredSkyLoads.size() + RetiredSkies.size());
	if (!P.Scene.EditNodes(std::move(InEdits), InExpectedRevision))
	{
		return false;
	}
	for (auto& Model : AddedModels)
	{
		P.Models.push_back(std::move(Model));
	}
	if (bModelsChanged)
	{
		P.ForgetRemovedModels();
	}
	for (const auto Handle : RetiredSkies)
	{
		const auto Sky = P.SkyLoads.find(Handle);
		P.RetiredSkyLoads.push_back(Sky->second);
		Sky->second->Cancellation.Cancel();
		P.SkyLoads.erase(Sky);
	}
	P.bModelStatusDirty = true;
	return true;
}

std::vector<FSceneHandle> FSceneInstance::AddNodes(std::vector<FSceneNode> InNodes)
{
	auto& P = *Impl;
	P.RequireOpen();
	std::vector<std::pair<std::size_t, FSceneInstanceModel>> Models;
	for (std::size_t Index = 0; Index < InNodes.size(); ++Index)
	{
		auto& Node = InNodes[Index];
		if (Node.Model())
		{
			if (!Node.Model()->Asset.empty())
			{
				const auto Load = P.Loads.find(Node.Model()->Asset);
				if (Load == P.Loads.end())
				{
					throw std::invalid_argument("Unknown scene asset");
				}
				Node.Model()->Data = Load->second.Data;
			}
			Models.emplace_back(Index, FSceneInstanceModel{{}, Node.Model()->Asset, Node.Id});
		}
	}
	P.Models.reserve(P.Models.size() + Models.size());
	const auto Handles = P.Scene.AddNodes(std::move(InNodes));
	for (auto& [Index, Model] : Models)
	{
		Model.Handle = Handles[Index];
		P.Models.push_back(std::move(Model));
	}
	P.bModelStatusDirty = true;
	return Handles;
}

bool FSceneInstance::RemoveSubtrees(std::span<const FSceneHandle> InHandles)
{
	Impl->RequireOpen();
	if (!Impl->Scene.RemoveSubtrees(InHandles))
	{
		return false;
	}
	Impl->ForgetRemovedModels();
	return true;
}
} // namespace Hyperion
