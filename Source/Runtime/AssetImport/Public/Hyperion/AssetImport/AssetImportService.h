#pragma once
#include "Hyperion/Assets/AssetService.h"

namespace Hyperion
{
struct FAssetImportContext
{
	FIOService& IO;
	std::filesystem::path Path;
	std::shared_ptr<const FBytes> Bytes;
	FCancellationToken Cancellation;
	std::vector<FAssetSource> Sources;

	std::shared_ptr<const FBytes> Read(const std::filesystem::path& InPath);
};

struct FAssetImporter
{
	std::string Id;
	std::uint32_t Version = 1;
	const FRecordDescriptor* Type{};
	std::vector<std::string> Extensions;
	std::function<std::shared_ptr<void>(FAssetImportContext&)> Convert;
};

struct FConvertedAsset
{
	std::shared_ptr<const FRecordDescriptor> Type;
	std::shared_ptr<const void> Object;
	std::vector<FAssetSource> Sources;
	std::string Importer;
	std::uint32_t ImporterVersion{};
	std::size_t RetainedBytes{};
	std::optional<FAssetHeader> NativeHeader;
};

template<class T> class TImportRequest
{
public:
	TImportRequest() = default;

	explicit TImportRequest(TAsyncResult<FConvertedAsset> InResult) : Result(std::move(InResult))
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

	std::shared_ptr<const T> GetReady() const
	{
		Cancellation.Check();
		const auto Value = Result.GetReady();
		if (Value->Type->CppType != typeid(T))
		{
			throw std::runtime_error("Source importer type mismatch");
		}
		return std::static_pointer_cast<const T>(Value->Object);
	}

	std::shared_ptr<const T> Get(FTaskSystem& InTasks) const
	{
		Cancellation.Check();
		(void)Result.Get(InTasks);
		return GetReady();
	}

private:
	TAsyncResult<FConvertedAsset> Result;
	FCancellationToken Cancellation;
};

struct FAssetImportOptions
{
	bool bScene{};
	bool bForce{};
	std::string Name;
	std::string TypeId;
};

struct FAssetImportResult
{
	FAssetHeader Header;
	std::filesystem::path Output;
	std::size_t WrittenAssets{};
	bool bUpToDate{};
};

// Source conversion is tooling-only. Runtime consumers use FAssetService on the generated native files.
class FAssetImportService
{
public:
	explicit FAssetImportService(FIOService& InIO);
	~FAssetImportService();
	void Register(FAssetImporter InImporter);

	template<class T> TImportRequest<T> LoadAsync(const std::filesystem::path& InPath)
	{
		return TImportRequest<T>(Load(InPath, RecordType<T>().Id));
	}

	TAsyncResult<FAssetImportResult> ImportAsync(std::filesystem::path InSource, std::filesystem::path InOutput,
	                                             FAssetImportOptions InOptions = {});
	void ClearCache();
	void Drain();

private:
	TAsyncResult<FConvertedAsset> Load(const std::filesystem::path& InPath, std::string_view InType);
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
