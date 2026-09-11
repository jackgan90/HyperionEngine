#pragma once
#include "Hyperion/Assets/NativeAsset.h"
#include "Hyperion/IO/IOService.h"

namespace Hyperion
{
struct FLoadedAsset
{
	FAssetHeader Header;
	std::shared_ptr<const FRecordDescriptor> Type;
	std::shared_ptr<const void> Object;
	std::filesystem::path Path;
	std::vector<std::string> Diagnostics;
	std::size_t RetainedBytes{};

	template<class T> std::shared_ptr<const T> As() const
	{
		if (!Type || Type->CppType != typeid(T))
		{
			throw std::runtime_error("Asset C++ type mismatch: " + Header.TypeId);
		}
		return std::static_pointer_cast<const T>(Object);
	}
};

class FAssetRequest
{
public:
	FAssetRequest() = default;

	explicit FAssetRequest(TAsyncResult<FLoadedAsset> InResult, std::optional<FAssetRef> InExpected = {})
	    : Result(std::move(InResult)), Expected(std::move(InExpected))
	{
	}

	void Cancel() const
	{
		Cancellation.Cancel();
	}

	bool Ready() const
	{
		return Cancellation.IsCancelled() || Result.Ready();
	}

	std::shared_ptr<const FLoadedAsset> GetReady() const;
	std::shared_ptr<const FLoadedAsset> Get(FTaskSystem& InTasks) const;

	const FTaskHandle& Task() const
	{
		return Result.Task();
	}

private:
	friend class FAssetService;
	void Validate(const FLoadedAsset& InAsset) const;
	TAsyncResult<FLoadedAsset> Result;
	FCancellationToken Cancellation;
	std::optional<FAssetRef> Expected;
};

template<class T> class TAssetRequest
{
public:
	TAssetRequest() = default;

	explicit TAssetRequest(FAssetRequest InRequest) : Request(std::move(InRequest))
	{
	}

	void Cancel() const
	{
		Request.Cancel();
	}

	bool Ready() const
	{
		return Request.Ready();
	}

	std::shared_ptr<const T> GetReady() const
	{
		return Request.GetReady()->template As<T>();
	}

	std::shared_ptr<const T> Get(FTaskSystem& InTasks) const
	{
		return Request.Get(InTasks)->template As<T>();
	}

	std::shared_ptr<const FLoadedAsset> GetAsset(FTaskSystem& InTasks) const
	{
		return Request.Get(InTasks);
	}

private:
	FAssetRequest Request;
};

struct FAssetCacheOptions
{
	std::size_t MaxEntries = 64;
	std::size_t MaxBytes = 256u * 1024u * 1024u;
	std::size_t MaxInFlight = 256;
	std::size_t MaxGraphAssets = 4096;
};

struct FAssetCacheStats
{
	std::size_t Entries{};
	std::size_t InFlight{};
	std::size_t RetainedBytes{};
};

struct FAssetDependencyFailure
{
	std::filesystem::path Parent;
	std::string Field;
	FAssetRef Reference;
	std::string Error;
};

struct FAssetGraph
{
	std::shared_ptr<const FLoadedAsset> Root;
	std::map<std::filesystem::path, std::shared_ptr<const FLoadedAsset>> Assets;
	std::vector<FAssetDependencyFailure> Failures;
};

// Own before requests; destroy/drain before IO and Tasks. CPU readiness is separate from GPU readiness.
class FAssetService
{
public:
	explicit FAssetService(FIOService& InIO, FAssetCacheOptions InOptions = {});
	~FAssetService();
	FRecordRegistry& Types();
	FAssetRequest LoadAsync(const std::filesystem::path& InPath);
	FAssetRequest LoadReferenceAsync(const FAssetRef& InReference, const std::filesystem::path& InContainingAsset);
	FAssetRequest LoadByIdAsync(std::string_view InId);
	TAsyncResult<FAssetGraph> LoadGraphAsync(const std::filesystem::path& InPath);
	void SetCatalog(const FAssetCatalog& InCatalog, const std::filesystem::path& InDirectory);
	std::filesystem::path Resolve(const FAssetRef& InReference, const std::filesystem::path& InContainingAsset) const;

	template<class T> TAssetRequest<T> LoadAsync(const std::filesystem::path& InPath)
	{
		Types().Register<T>();
		return TAssetRequest<T>(LoadAsync(InPath));
	}

	template<class T>
	TAssetRequest<T> LoadReferenceAsync(const FAssetRef& InReference, const std::filesystem::path& InContainingAsset)
	{
		Types().Register<T>();
		if (InReference.TypeId != RecordType<T>().Id)
		{
			throw std::runtime_error("Asset reference expected type mismatch: " + InReference.TypeId);
		}
		return TAssetRequest<T>(LoadReferenceAsync(InReference, InContainingAsset));
	}

	template<class T> TAsyncResult<bool> SaveAsync(std::filesystem::path InPath, std::shared_ptr<const T> InSnapshot)
	{
		if (!InSnapshot)
		{
			throw std::invalid_argument("Null asset snapshot");
		}
		Types().Register<T>();
		// Capture value ownership at admission; later caller edits cannot race the serializer.
		return Save(std::move(InPath), RecordType<T>(), std::make_shared<const T>(*InSnapshot));
	}

	void Invalidate(const std::filesystem::path& InPath);
	void ClearCache();
	FAssetCacheStats Statistics();
	void Drain();

private:
	FAssetRequest Load(const std::filesystem::path& InPath, bool bInGraphDependency);
	TAsyncResult<bool> Save(std::filesystem::path InPath, const FRecordDescriptor& InType,
	                        std::shared_ptr<const void> InSnapshot);
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
