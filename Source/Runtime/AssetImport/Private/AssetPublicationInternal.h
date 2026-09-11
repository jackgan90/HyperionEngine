#pragma once
#include "AssetImportInternal.h"
#include <set>

namespace Hyperion
{
struct FPublishedAsset
{
	FAssetRef Reference;
	std::filesystem::path Path;
	std::optional<FAssetRef> SourceIdentity;
};

struct FPublication
{
	FIOService& IO;
	FCancellationToken Cancellation;
	const std::vector<FAssetImporter>& Importers;
	std::function<FConvertedAsset(const std::filesystem::path&, std::string_view)> Convert;
	std::filesystem::path Source;
	std::filesystem::path Output;
	std::string SourceType;
	FAssetProvenance Provenance;
	std::optional<FAssetDocument> Previous;
	std::optional<FEncodedAsset> Root;
	std::map<std::pair<std::filesystem::path, std::string>, FConvertedAsset> Converted;
	std::map<std::pair<std::filesystem::path, std::string>, FPublishedAsset> Published;
	std::set<std::pair<std::filesystem::path, std::string>> Active;
	std::map<std::filesystem::path, std::string> Sources;
	std::map<std::string, std::string> PreviousIds;
	std::size_t Written{};
	std::size_t Bytes{};

	void Prepare(const FAssetImportOptions& InOptions);
	bool IsCurrent();
	void CheckSources() const;
	void Track(const FConvertedAsset& InAsset);
	FPublishedAsset Build(const std::filesystem::path& InSource, const FConvertedAsset& InAsset, bool bInRoot);
	void Rewrite(void* InObject, const FRecordDescriptor& InType, const std::filesystem::path& InSource,
	             const std::filesystem::path& InDestination);
	void Write(const std::filesystem::path& InPath, const FEncodedAsset& InAsset);
};
} // namespace Hyperion
