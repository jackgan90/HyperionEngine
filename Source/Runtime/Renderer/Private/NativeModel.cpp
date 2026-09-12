#include "Hyperion/Renderer/NativeModel.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Renderer/RenderResources.h"
#include "Hyperion/Scene/SceneManifest.h"

namespace Hyperion
{
namespace
{
void RequireCompleteGraph(const FAssetGraph& InGraph)
{
	if (!InGraph.Root || !InGraph.Failures.empty())
	{
		throw std::runtime_error(InGraph.Failures.empty()
		                             ? "Native asset graph has no root"
		                             : PathToUtf8(InGraph.Failures.front().Parent) + ":" +
		                                   InGraph.Failures.front().Field + ": " + InGraph.Failures.front().Error);
	}
}

std::shared_ptr<const FLoadedAsset> GraphReference(const FAssetGraph& InGraph, const FAssetService& InAssets,
                                                   const FAssetRef& InReference,
                                                   const std::filesystem::path& InContaining)
{
	const auto Path = InAssets.Resolve(InReference, InContaining);
	const auto Found = InGraph.Assets.find(Path);
	if (Found == InGraph.Assets.end())
	{
		throw std::runtime_error("Unresolved native asset dependency: " + PathToUtf8(Path));
	}
	const auto& Header = Found->second->Header;
	if (Header.TypeId != InReference.TypeId || (!InReference.Id.empty() && InReference.Id != Header.Id) ||
	    (!InReference.Revision.empty() && InReference.Revision != Header.Revision))
	{
		throw std::runtime_error("Native asset graph reference identity/type/revision mismatch");
	}
	return Found->second;
}
} // namespace

std::shared_ptr<const FMaterialAssetData> ResolveMaterialAssetGraph(const FAssetGraph& InGraph,
                                                                    const FLoadedAsset& InMaterial,
                                                                    const FAssetService& InAssets)
{
	RequireCompleteGraph(InGraph);
	auto Data = std::make_shared<FMaterialAssetData>();
	Data->Asset = InMaterial.As<FMaterialAsset>();
	for (const auto& Dependency : InMaterial.Header.Dependencies)
	{
		if (Dependency.Reference.TypeId != RecordType<FTextureAsset>().Id)
		{
			throw std::runtime_error("Material references a non-texture asset");
		}
		const auto Texture = GraphReference(InGraph, InAssets, Dependency.Reference, InMaterial.Path);
		Data->Textures.emplace(Dependency.Reference, Texture->As<FTextureAsset>());
	}
	return Data;
}

std::shared_ptr<const FSceneModelData> ResolveModelAssetGraph(const FAssetGraph& InGraph, const FAssetService& InAssets)
{
	RequireCompleteGraph(InGraph);
	const auto Model = InGraph.Root->As<FModelAsset>();
	std::vector<std::shared_ptr<const FMaterialAssetData>> Materials;
	std::map<const FMaterialAsset*, std::shared_ptr<const FMaterialAssetData>> Unique;
	for (const auto& Reference : Model->MaterialSlots)
	{
		const auto Loaded = GraphReference(InGraph, InAssets, Reference, InGraph.Root->Path);
		const auto Asset = Loaded->As<FMaterialAsset>();
		auto& Data = Unique[Asset.get()];
		if (!Data)
		{
			Data = ResolveMaterialAssetGraph(InGraph, *Loaded, InAssets);
		}
		Materials.push_back(Data);
	}
	return PrepareSceneModel(Model, std::move(Materials));
}

TAsyncResult<FSceneModelData> LoadNativeModel(FAssetService& InAssets, FTaskSystem& InTasks,
                                              const FAssetRef& InReference,
                                              const std::filesystem::path& InContainingAsset,
                                              FCancellationToken InCancellation, FRenderResourceService* InResources)
{
	RegisterSceneAssetTypes(InAssets.Types());
	if (InReference.TypeId != RecordType<FModelAsset>().Id)
	{
		throw std::invalid_argument("Native model request requires the model asset type");
	}
	auto Graph = InAssets.LoadGraphAsync(InReference, InContainingAsset);
	return DispatchAsync<FSceneModelData>(
	    InTasks, {EDomain::Worker},
	    [Graph, &InAssets, &InTasks, InCancellation, InResources]
	    {
		    const auto Loaded = Graph.Get(InTasks);
		    InCancellation.Check();
		    auto Data = *ResolveModelAssetGraph(*Loaded, InAssets);
		    if (InResources)
		    {
			    for (const auto& Material : Data.Materials)
			    {
				    InCancellation.Check();
				    Data.MaterialSnapshots.push_back(InResources->PrepareMaterialAsset(Material));
			    }
		    }
		    return Data;
	    },
	    InCancellation);
}

TAsyncResult<FSceneModelData> LoadNativeModel(FAssetService& InAssets, FTaskSystem& InTasks,
                                              const std::filesystem::path& InPath, FCancellationToken InCancellation,
                                              FRenderResourceService* InResources)
{
	return LoadNativeModel(
	    InAssets, InTasks,
	    {"", PathToUtf8(std::filesystem::absolute(InPath).lexically_normal()), RecordType<FModelAsset>().Id, ""}, {},
	    InCancellation, InResources);
}
} // namespace Hyperion
