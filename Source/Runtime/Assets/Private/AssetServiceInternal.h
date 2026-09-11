#pragma once
#include "Hyperion/Assets/AssetService.h"

namespace Hyperion
{
std::filesystem::path NormalizeAssetPath(const std::filesystem::path& InPath);
void RequireNativeAssetPath(const std::filesystem::path& InPath);

struct FAssetService::FImpl
{
	struct FCacheEntry
	{
		TAsyncResult<FLoadedAsset> Request;
		std::uint64_t LastUse{};
	};

	FImpl(FIOService& InIO, FAssetCacheOptions InOptions) : IO(InIO), Options(InOptions)
	{
	}

	FIOService& IO;
	FAssetCacheOptions Options;
	FRecordRegistry Registry;
	mutable std::mutex Mutex;
	bool bClosing{};
	FCancellationToken Cancellation;
	std::map<std::filesystem::path, FCacheEntry> Cache;
	std::map<std::filesystem::path, TAsyncResult<bool>> Writes;
	std::map<std::filesystem::path, FTaskHandle> Operations;
	std::map<std::string, std::pair<FAssetRef, std::filesystem::path>> Catalog;
	std::vector<FTaskHandle> Pending;
	std::uint64_t Access{};
	FAssetCacheStats Trim();
	void ScheduleTrim(FTaskHandle InTask);
	void RequireOpen() const;
	FLoadedAsset Read(const std::filesystem::path& InPath);
};
} // namespace Hyperion
