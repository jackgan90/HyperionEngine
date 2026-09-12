#include "Hyperion/Renderer/SceneMaterialAssets.h"
#include "Hyperion/IO/Path.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
FAssetRef LoadedReference(const FLoadedAsset& InAsset)
{
	return {InAsset.Header.Id, PathToUtf8(InAsset.Path), InAsset.Header.TypeId, InAsset.Header.Revision};
}

void LoadValueTextures(const FSceneMaterialAsset& InSelection, const std::filesystem::path& InContaining,
                       FAssetService& InAssets, FTaskSystem& InTasks,
                       std::map<FAssetRef, std::shared_ptr<const FTextureAsset>>& OutValues,
                       std::map<FAssetRef, std::shared_ptr<const FTextureAsset>>& OutCanonical,
                       const FCancellationToken& InCancellation)
{
	for (const auto& Dependency : CollectAssetDependencies(RecordType<FSceneMaterialAsset>(), &InSelection))
	{
		if (Dependency.Reference.TypeId == RecordType<FTextureAsset>().Id && !OutValues.contains(Dependency.Reference))
		{
			InCancellation.Check();
			const auto Loaded = InAssets.LoadReferenceAsync(Dependency.Reference, InContaining).Get(InTasks);
			const auto Texture = Loaded->As<FTextureAsset>();
			OutValues.emplace(Dependency.Reference, Texture);
			OutCanonical.emplace(LoadedReference(*Loaded), Texture);
		}
	}
}

FSceneMaterialSelection PrepareSelection(const FSceneMaterialAsset& InSelection,
                                         const std::filesystem::path& InContaining, FAssetService& InAssets,
                                         FTaskSystem& InTasks, FRenderResourceService& InResources,
                                         const FCancellationToken& InCancellation)
{
	InCancellation.Check();
	ValidateSceneMaterialAsset(InSelection);
	FSceneMaterialSelection Result;
	if (InSelection.Reference)
	{
		const auto Graph = InAssets.LoadGraphAsync(*InSelection.Reference, InContaining).Get(InTasks);
		if (!Graph->Root || !Graph->Failures.empty())
		{
			throw std::runtime_error(Graph->Failures.empty()
			                             ? "Material graph has no root"
			                             : Graph->Failures.front().Field + ": " + Graph->Failures.front().Error);
		}
		const auto Data = ResolveMaterialAssetGraph(*Graph, *Graph->Root, InAssets);
		Result.AssetSnapshot = InResources.PrepareMaterialAsset(Data);
		Result.Snapshot = Result.AssetSnapshot;
		Result.Reference = LoadedReference(*Graph->Root);
		for (const auto& [Path, Asset] : Graph->Assets)
		{
			if (Asset->Type->CppType == typeid(FTextureAsset))
			{
				Result.TextureAssets.emplace(LoadedReference(*Asset), Asset->As<FTextureAsset>());
			}
		}
	}
	std::map<FAssetRef, std::shared_ptr<const FTextureAsset>> Textures;
	LoadValueTextures(InSelection, InContaining, InAssets, InTasks, Textures, Result.TextureAssets, InCancellation);
	if (!InSelection.Values.empty())
	{
		FMaterialInstance Edited(Result.AssetSnapshot);
		for (auto& Value : InResources.PrepareAssetValues(InSelection.Values, Textures))
		{
			Edited.Set(Value.Name, std::move(Value.Value));
		}
		Result.Snapshot = Edited.Freeze();
	}
	Result.Overrides = InResources.PrepareAssetValues(InSelection.Overrides, Textures);
	InCancellation.Check();
	return Result;
}

FMaterialAssetValue PersistValue(const FSceneMaterialSelection& InSelection, const FMaterialValue& InValue)
{
	return PersistMaterialValue(InValue,
	                            [&](const std::shared_ptr<const FMaterialTextureSource>& InSource)
	                            {
		                            for (const auto& [Reference, Texture] : InSelection.TextureAssets)
		                            {
			                            if (InSource->GetAsset() == Texture)
			                            {
				                            return Reference;
			                            }
		                            }
		                            throw std::runtime_error(
		                                "Scene material texture has no persistent native reference");
	                            });
}
} // namespace

TAsyncResult<FSceneMaterialSelection> LoadSceneMaterialSelection(FAssetService& InAssets, FTaskSystem& InTasks,
                                                                 FRenderResourceService& InResources,
                                                                 FSceneMaterialAsset InSelection,
                                                                 std::filesystem::path InContainingAsset,
                                                                 FCancellationToken InCancellation)
{
	RegisterSceneAssetTypes(InAssets.Types());
	return DispatchAsync<FSceneMaterialSelection>(
	    InTasks, {EDomain::Worker},
	    [&InAssets, &InTasks, &InResources, Selection = std::move(InSelection),
	     Containing = std::move(InContainingAsset), InCancellation]
	    {
		    return PrepareSelection(Selection, Containing, InAssets, InTasks, InResources, InCancellation);
	    },
	    InCancellation);
}

FSceneMaterialAsset PersistSceneMaterialSelection(const FSceneMaterialSelection& InSelection)
{
	FSceneMaterialAsset Result;
	Result.Reference = InSelection.Reference;
	const auto Snapshot = InSelection.Instance ? InSelection.Instance->Freeze() : InSelection.Snapshot;
	if (Snapshot)
	{
		if (!Result.Reference || !InSelection.AssetSnapshot ||
		    Snapshot->Definition != InSelection.AssetSnapshot->Definition ||
		    Snapshot->Schema != InSelection.AssetSnapshot->Schema)
		{
			throw std::runtime_error("Scene material selection has no matching persistent asset definition");
		}
		for (const auto& Entry : Snapshot->Overrides)
		{
			const auto& Base = InSelection.AssetSnapshot->Overrides;
			const auto Found = std::find_if(Base.begin(), Base.end(),
			                                [&](const auto& InEntry)
			                                {
				                                return InEntry.Name == Entry.Name;
			                                });
			if (Found == Base.end() || Found->Value != Entry.Value)
			{
				Result.Values.push_back({Entry.Name, PersistValue(InSelection, Entry.Value)});
			}
		}
		for (const auto& Entry : InSelection.AssetSnapshot->Overrides)
		{
			if (std::none_of(Snapshot->Overrides.begin(), Snapshot->Overrides.end(),
			                 [&](const auto& InEntry)
			                 {
				                 return InEntry.Name == Entry.Name;
			                 }))
			{
				// Clearing a base value would change fallback semantics; represent the effective default explicitly.
				const auto& Parameter = Snapshot->Schema->Get(Snapshot->Schema->Find(Entry.Name));
				if (!Parameter.Default)
				{
					throw std::runtime_error("Cannot persist removal of a required asset material value");
				}
				Result.Values.push_back({Entry.Name, PersistValue(InSelection, *Parameter.Default)});
			}
		}
	}
	else if (Result.Reference)
	{
		throw std::runtime_error("Cannot persist an unresolved material asset selection");
	}
	for (const auto& Entry : InSelection.Overrides)
	{
		Result.Overrides.push_back({Entry.Name, PersistValue(InSelection, Entry.Value)});
	}
	ValidateSceneMaterialAsset(Result);
	return Result;
}
} // namespace Hyperion
