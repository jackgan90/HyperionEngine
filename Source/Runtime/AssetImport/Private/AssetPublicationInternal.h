#pragma once
#include "AssetImportInternal.h"
#include <set>

namespace Hyperion
{
std::filesystem::path ImportProductPath(const std::filesystem::path& InSource, std::string_view InKey);

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
	std::map<std::filesystem::path, FAssetRef> ExternalAssets;
	std::map<std::string, std::string> PreviousIds;
	std::size_t Written{};
	std::size_t Bytes{};
	std::filesystem::path Library;
	std::map<std::string, FAssetRef> LibraryProducts;
	std::map<std::string, FPublishedAsset> ExistingAssets;
	std::map<std::string, FPublishedAsset> TextureProducts;
	std::map<std::filesystem::path, FEncodedAsset> Staged;
	std::map<std::string, std::string> ProductRevisions;
	std::map<std::string, std::pair<std::string, std::string>> ProductContents;
	std::unique_ptr<FAssetService> NativeAssets;
	std::string RootId;
	std::filesystem::path SourceRoot;
	std::string SourceId;
	std::string StableSourceKey(const std::filesystem::path& InPath) const;
	std::string PortableKey(std::string InKey) const;
	bool PreserveExternal(FAssetRef& InReference);

	void LoadLibrary();
	FAssetService& IndexedAssets();
	void ClaimProduct(const std::string& InId, const std::string& InKey, const std::string& InContent);
	void Commit();
	std::string SelectId(const std::string& InKey, const FConvertedAsset& InAsset, bool bInRoot) const;
	std::filesystem::path ProductDestination(std::string_view InId, const FConvertedAsset& InAsset) const;
	std::optional<FPublishedAsset> ReuseTexture(const std::string& InContent);
	FAssetHeader MakeHeader(const std::string& InId, const std::string& InKey, const std::string& InTextureContent,
	                        bool bInRoot);
	void AddProducts(const std::filesystem::path& InSource, const FConvertedAsset& InAsset);

	void Prepare(const FAssetImportOptions& InOptions);
	bool IsCurrent();
	bool SourcesCurrent(const FAssetGraph& InGraph) const;
	void CheckSources() const;
	void Track(const FConvertedAsset& InAsset);
	FPublishedAsset Build(const std::filesystem::path& InSource, const FConvertedAsset& InAsset, bool bInRoot);
	void Rewrite(void* InObject, const FRecordDescriptor& InType, const std::filesystem::path& InSource,
	             const std::filesystem::path& InDestination);
	void Write(const std::filesystem::path& InPath, const FEncodedAsset& InAsset);
};
} // namespace Hyperion
