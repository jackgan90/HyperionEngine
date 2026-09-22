#include "Hyperion/Renderer/RenderSession.h"
#include "SceneInstanceInternal.h"

namespace Hyperion
{
namespace
{
bool SameSelection(const FSceneMaterialSelection& InCurrent, const FSceneMaterialAsset& InCaptured)
{
	return HashArchive(WriteValue(PersistSceneMaterialSelection(InCurrent))) == HashArchive(WriteValue(InCaptured));
}
} // namespace

void FSceneInstance::RefreshAssets(std::span<const FAssetSaveResult> InSaved)
{
	Impl->RequireOpen();
	Impl->bRefreshAllAssets |= InSaved.empty();
	for (const auto& Saved : InSaved)
	{
		Impl->ChangedAssetIds.insert(Saved.Header.Id);
		Impl->ChangedAssetPaths.insert(Impl->Assets.NormalizePath(Saved.Path));
	}
	Impl->bRefreshRequested = true;
}

bool FSceneInstance::FImpl::IsChangedReference(const FAssetRef& InReference,
                                               const std::filesystem::path& InContaining) const
{
	return bRefreshAllAssets || ChangedAssetIds.contains(InReference.Id) ||
	       ChangedAssetPaths.contains(Assets.Resolve(InReference, InContaining));
}

bool FSceneInstance::FImpl::UsesChangedAssets(const FLoad& InLoad) const
{
	if (IsChangedReference(InLoad.Reference, Path) || !InLoad.Data)
	{
		return true;
	}
	const auto Containing = Assets.Resolve(InLoad.Reference, Path);
	for (std::size_t Index = 0; Index < InLoad.Data->Materials.size(); ++Index)
	{
		const auto& Slot = InLoad.Data->Asset->MaterialSlots.at(Index);
		if (IsChangedReference(Slot, Containing))
		{
			return true;
		}
		for (const auto& [Reference, Texture] : InLoad.Data->Materials[Index]->Textures)
		{
			if (IsChangedReference(Reference, Assets.Resolve(Slot, Containing)))
			{
				return true;
			}
		}
	}
	return false;
}

bool FSceneInstance::FImpl::UsesChangedAssets(const FSceneMaterialSelection& InSelection) const
{
	if (bRefreshAllAssets || (InSelection.Reference && IsChangedReference(*InSelection.Reference, Path)))
	{
		return true;
	}
	for (const auto& [Reference, Texture] : InSelection.TextureAssets)
	{
		if (IsChangedReference(Reference, Path))
		{
			return true;
		}
	}
	return false;
}

FSceneNode FSceneInstance::RebindAssetResources(FSceneNode InNode)
{
	auto& P = *Impl;
	P.RequireOpen();
	if (auto& Model = InNode.Model(); Model)
	{
		if (const auto It = P.Loads.find(Model->Asset); It != P.Loads.end())
		{
			Model->Data = It->second.Data;
		}
		const auto Rebind = [&](const FSceneMaterialSelection& InSelection)
		{
			const auto Authored = PersistSceneMaterialSelection(InSelection);
			return *LoadSceneMaterialSelection(P.Assets, P.Tasks, P.Session.GetResources(), Authored, P.Path)
			            .Get(P.Tasks);
		};
		Model->Surface = Rebind(Model->Surface);
		for (auto& [Section, Selection] : Model->SectionSurfaces)
		{
			Selection = Rebind(Selection);
		}
	}
	if (auto& Light = InNode.EnvironmentLight(); Light && Light->Sky)
	{
		Light->Sky->Revision.clear();
		Light->Data.reset();
	}
	return InNode;
}

void FSceneInstance::FImpl::BeginAssetRefresh()
{
	std::map<std::string, std::pair<FAssetRef, std::shared_ptr<const FSceneModelData>>> References;
	for (const auto& [Id, Load] : Loads)
	{
		if (!UsesChangedAssets(Load))
		{
			continue;
		}
		auto Reference = Load.Reference;
		Reference.Revision.clear();
		References.emplace(Id, std::make_pair(std::move(Reference), Load.Data));
	}
	std::vector<FRefreshSelection> Selections;
	for (const auto Handle : Scene.GetNodes(ESceneNodeKind::Model))
	{
		const auto& Model = *Scene.FindModelComponent(Handle);
		bool bAffected = References.contains(Model.Asset) || UsesChangedAssets(Model.Surface);
		for (const auto& [Section, Value] : Model.SectionSurfaces)
		{
			bAffected |= UsesChangedAssets(Value);
		}
		if (!bAffected)
		{
			continue;
		}
		FRefreshSelection Selection;
		Selection.Handle = Handle;
		Selection.Surface = PersistSceneMaterialSelection(Model.Surface);
		for (const auto& [Section, Value] : Model.SectionSurfaces)
		{
			Selection.Sections.emplace(Section, PersistSceneMaterialSelection(Value));
		}
		Selections.push_back(std::move(Selection));
	}
	RefreshCancellation = {};
	AssetRefresh = DispatchAsync<FAssetRefresh>(
	    Tasks, {EDomain::Worker},
	    [this, References = std::move(References), Selections = std::move(Selections),
	     Token = RefreshCancellation]() mutable
	    {
		    FAssetRefresh Result;
		    for (const auto& [Id, Source] : References)
		    {
			    Token.Check();
			    Result.Models.emplace(Id, LoadNativeModel(Assets, Tasks, Source.first, Path, Token,
			                                              &Session.GetResources(), bPrepareQueries, Source.second)
			                                  .Get(Tasks));
		    }
		    for (auto& Selection : Selections)
		    {
			    Token.Check();
			    Selection.PreparedSurface =
			        *LoadSceneMaterialSelection(Assets, Tasks, Session.GetResources(), Selection.Surface, Path, Token)
			             .Get(Tasks);
			    for (const auto& [Section, Value] : Selection.Sections)
			    {
				    Selection.PreparedSections.emplace(
				        Section, *LoadSceneMaterialSelection(Assets, Tasks, Session.GetResources(), Value, Path, Token)
				                      .Get(Tasks));
			    }
		    }
		    Result.Selections = std::move(Selections);
		    return Result;
	    },
	    RefreshCancellation);
	bRefreshRequested = false;
}

bool FSceneInstance::FImpl::HasUncapturedRefreshConsumers(const FAssetRefresh& InRefresh) const
{
	std::set<FSceneHandle> Captured;
	for (const auto& Selection : InRefresh.Selections)
	{
		Captured.insert(Selection.Handle);
	}
	for (const auto Handle : Scene.GetNodes(ESceneNodeKind::Model))
	{
		if (Captured.contains(Handle))
		{
			continue;
		}
		const auto& Model = *Scene.FindModelComponent(Handle);
		const auto Load = Loads.find(Model.Asset);
		if ((Load != Loads.end() && UsesChangedAssets(Load->second)) || UsesChangedAssets(Model.Surface))
		{
			return true;
		}
		for (const auto& [Section, Selection] : Model.SectionSurfaces)
		{
			if (UsesChangedAssets(Selection))
			{
				return true;
			}
		}
	}
	return false;
}

void FSceneInstance::FImpl::PublishAssetRefresh(const FAssetRefresh& InRefresh)
{
	// A new/duplicated consumer can still hold the old data captured between Main ticks.
	// Retain the changed identities until a follow-up refresh has included it.
	bRefreshRequested |= HasUncapturedRefreshConsumers(InRefresh);
	for (const auto& [Id, Data] : InRefresh.Models)
	{
		if (auto It = Loads.find(Id); It != Loads.end())
		{
			It->second.Data = Data;
		}
	}
	for (const auto& Selection : InRefresh.Selections)
	{
		const auto* Current = Scene.FindModelComponent(Selection.Handle);
		if (!Current)
		{
			continue;
		}
		auto Updated = *Current;
		if (Current->SectionSurfaces.size() != Selection.Sections.size())
		{
			bRefreshRequested = true;
		}
		if (const auto It = InRefresh.Models.find(Updated.Asset); It != InRefresh.Models.end())
		{
			Updated.Data = It->second;
		}
		if (SameSelection(Current->Surface, Selection.Surface))
		{
			Updated.Surface = Selection.PreparedSurface;
		}
		else
		{
			bRefreshRequested = true;
		}
		for (const auto& [Section, Value] : Selection.Sections)
		{
			const auto It = Current->SectionSurfaces.find(Section);
			if (It != Current->SectionSurfaces.end() && SameSelection(It->second, Value))
			{
				Updated.SectionSurfaces[Section] = Selection.PreparedSections.at(Section);
			}
			else
			{
				bRefreshRequested = true;
			}
		}
		Scene.SetModelComponent(Selection.Handle, std::move(Updated));
	}
	for (auto It = SkyLoads.begin(); It != SkyLoads.end();)
	{
		const auto Handle = It->first;
		const auto Load = It->second;
		bool bAffected = IsChangedReference(Load->Reference, Path);
		if (Load->Data)
		{
			for (const auto& Reference : Load->Data->Dependencies)
			{
				bAffected |= IsChangedReference(Reference, Path);
			}
		}
		if (!bAffected)
		{
			++It;
			continue;
		}
		Load->Cancellation.Cancel();
		RetiredSkyLoads.push_back(Load);
		if (const auto* Current = Scene.FindEnvironmentLight(Handle); Current && Current->Sky)
		{
			auto Light = *Current;
			Light.Sky->Revision.clear();
			Scene.SetEnvironmentLight(Handle, std::move(Light));
		}
		It = SkyLoads.erase(It);
	}
	bModelStatusDirty = true;
}

void FSceneInstance::FImpl::PollAssetRefresh()
{
	try
	{
		if (AssetRefresh && AssetRefresh->Ready())
		{
			const auto Result = AssetRefresh->GetReady();
			AssetRefresh.reset();
			// A newer publication supersedes this result, including saves admitted while it was loading.
			if (!bRefreshRequested)
			{
				PrepareRefreshResources(Result);
			}
		}
		if (RefreshResources && AreRefreshResourcesReady())
		{
			if (!bRefreshRequested)
			{
				PublishAssetRefresh(*RefreshResources->Data);
				if (!bRefreshRequested)
				{
					ChangedAssetIds.clear();
					ChangedAssetPaths.clear();
					bRefreshAllAssets = false;
				}
			}
			RefreshResources.reset();
			Status.AssetRefreshError.clear();
		}
		if (bRefreshRequested && !AssetRefresh && !RefreshResources && Status.bLoaded && bMaterialsComplete)
		{
			BeginAssetRefresh();
		}
	}
	catch (const std::exception& Failure)
	{
		AssetRefresh.reset();
		RefreshResources.reset();
		bRefreshRequested = false;
		Status.AssetRefreshError = Failure.what();
	}
}
} // namespace Hyperion
