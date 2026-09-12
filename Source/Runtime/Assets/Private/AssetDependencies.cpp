#include "AssetServiceInternal.h"
#include "Hyperion/IO/Path.h"
#include <set>

namespace Hyperion
{
void FAssetService::SetCatalog(const FAssetCatalog& InCatalog, const std::filesystem::path& InDirectory)
{
	(void)WriteValue(InCatalog);
	std::map<std::string, std::pair<FAssetRef, std::filesystem::path>> Catalog;
	for (const auto& Asset : InCatalog.Assets)
	{
		Catalog.emplace(Asset.Id, std::make_pair(Asset, NormalizeAssetPath(InDirectory / PathFromUtf8(Asset.Path))));
	}
	std::lock_guard Lock(Impl->Mutex);
	Impl->RequireOpen();
	Impl->Catalog = std::move(Catalog);
}

std::filesystem::path FAssetService::Resolve(const FAssetRef& InReference,
                                             const std::filesystem::path& InContainingAsset) const
{
	ValidateAssetRef(InReference);
	std::lock_guard Lock(Impl->Mutex);
	const auto It = Impl->Catalog.find(InReference.Id);
	if (It != Impl->Catalog.end() &&
	    (InReference.Revision.empty() || InReference.Revision == It->second.first.Revision))
	{
		return It->second.second;
	}
	if (InReference.Path.empty())
	{
		throw std::runtime_error("Asset ID is absent from catalog: " + InReference.Id);
	}
	return NormalizeAssetPath(InContainingAsset.parent_path() / PathFromUtf8(InReference.Path));
}

FAssetRequest FAssetService::LoadReferenceAsync(const FAssetRef& InReference,
                                                const std::filesystem::path& InContainingAsset)
{
	const auto Path = Resolve(InReference, InContainingAsset);
	auto Request = LoadAsync(Path);
	// Validation is performed on the result, so sharing never bypasses the requesting reference's identity.
	return FAssetRequest(Request.Result, InReference);
}

FAssetRequest FAssetService::LoadByIdAsync(std::string_view InId)
{
	FAssetRef Reference;
	{
		std::lock_guard Lock(Impl->Mutex);
		const auto It = Impl->Catalog.find(std::string(InId));
		if (It == Impl->Catalog.end())
		{
			throw std::runtime_error("Unknown catalog asset: " + std::string(InId));
		}
		Reference = It->second.first;
		Reference.Path = PathToUtf8(It->second.second);
	}
	return LoadReferenceAsync(Reference, {});
}

namespace
{
struct FGraphLoader
{
	FAssetService& Service;
	FTaskSystem& Tasks;
	FCancellationToken Cancellation;
	std::size_t Limit;
	std::function<std::shared_ptr<const FLoadedAsset>(const FAssetRef&, const std::filesystem::path&)> Load;
	FAssetGraph Graph;
	std::set<std::filesystem::path> Active;
	std::size_t Edges{};

	void Visit(std::shared_ptr<const FLoadedAsset> InAsset, unsigned InDepth)
	{
		Cancellation.Check();
		if (InDepth > 128 || Graph.Assets.size() >= Limit)
		{
			throw std::runtime_error("Asset dependency graph limit exceeded");
		}
		Graph.Assets.emplace(InAsset->Path, InAsset);
		Active.insert(InAsset->Path);

		struct FActiveScope
		{
			std::set<std::filesystem::path>& Active;
			const std::filesystem::path& Path;

			~FActiveScope()
			{
				Active.erase(Path);
			}
		} Scope{Active, InAsset->Path};

		for (const auto& Dependency : InAsset->Header.Dependencies)
		{
			if (++Edges > Limit * 8)
			{
				throw std::runtime_error("Asset dependency edge limit exceeded");
			}
			try
			{
				const auto Path = Service.Resolve(Dependency.Reference, InAsset->Path);
				if (Active.contains(Path))
				{
					throw std::runtime_error("Asset dependency cycle: " + InAsset->Path.generic_string() + " -> " +
					                         Path.generic_string());
				}
				auto Loaded = Load(Dependency.Reference, InAsset->Path);
				if (!Graph.Assets.contains(Path))
				{
					Visit(std::move(Loaded), InDepth + 1);
				}
			}
			catch (const std::exception& Error)
			{
				Graph.Failures.push_back({InAsset->Path, Dependency.Field, Dependency.Reference, Error.what()});
			}
		}
	}
};
} // namespace

TAsyncResult<FAssetGraph> FAssetService::LoadGraphAsync(const std::filesystem::path& InPath)
{
	return LoadGraph(LoadAsync(InPath));
}

TAsyncResult<FAssetGraph> FAssetService::LoadGraphAsync(const FAssetRef& InReference,
                                                        const std::filesystem::path& InContainingAsset)
{
	return LoadGraph(LoadReferenceAsync(InReference, InContainingAsset));
}

TAsyncResult<FAssetGraph> FAssetService::LoadGraph(FAssetRequest InRoot)
{
	std::lock_guard Lock(Impl->Mutex);
	Impl->RequireOpen();
	if (Impl->Trim().InFlight >= Impl->Options.MaxInFlight)
	{
		throw std::runtime_error("Asset in-flight request limit exceeded");
	}
	Impl->Pending.reserve(Impl->Pending.size() + 2);
	auto Result = DispatchAsync<FAssetGraph>(
	    Impl->IO.TaskSystem(), {EDomain::Worker},
	    [this, Root = std::move(InRoot)]
	    {
		    FGraphLoader Loader{*this, Impl->IO.TaskSystem(), Impl->Cancellation, Impl->Options.MaxGraphAssets,
		                        [this](const FAssetRef& InReference, const std::filesystem::path& InContaining)
		                        {
			                        auto Request = Load(Resolve(InReference, InContaining), true);
			                        return FAssetRequest(Request.Result, InReference).Get(Impl->IO.TaskSystem());
		                        }};
		    Loader.Graph.Root = Root.Get(Loader.Tasks);
		    Loader.Visit(Loader.Graph.Root, 0);
		    return std::move(Loader.Graph);
	    },
	    Impl->Cancellation);
	Impl->Pending.push_back(Result.Task());
	Impl->ScheduleTrim(Result.Task());
	return Result;
}
} // namespace Hyperion
