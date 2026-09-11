#pragma once
#include "Hyperion/AssetImport/AssetImportService.h"

namespace Hyperion
{
std::string ImportPathString(const std::filesystem::path& InPath);
std::string ImportRelativePath(const std::filesystem::path& InPath, const std::filesystem::path& InBase);
std::filesystem::path ImportPath(const std::filesystem::path& InPath);
std::string ImportExtension(const std::filesystem::path& InPath);

struct FAssetImportService::FImpl
{
	explicit FImpl(FIOService& InIO) : IO(InIO)
	{
	}

	FIOService& IO;
	std::mutex Mutex;
	bool bStarted{};
	bool bClosing{};
	FCancellationToken Cancellation;
	std::vector<FAssetImporter> Importers;
	std::map<std::pair<std::filesystem::path, std::string>, TAsyncResult<FConvertedAsset>> Cache;
	std::map<std::filesystem::path, FTaskHandle> Publications;
	std::vector<FTaskHandle> Pending;

	FConvertedAsset Convert(const std::filesystem::path& InPath, std::string_view InType);
	FAssetImportResult Publish(const std::filesystem::path& InSource, const std::filesystem::path& InOutput,
	                           const FAssetImportOptions& InOptions);
	void Trim();
	void ScheduleTrim(FTaskHandle InTask);
};
} // namespace Hyperion
