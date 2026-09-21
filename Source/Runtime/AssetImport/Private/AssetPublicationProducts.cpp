#include "AssetPublicationInternal.h"

namespace Hyperion
{
std::optional<FPublishedAsset> FPublication::ReuseTexture(const std::string& InContent)
{
	const auto Reused = TextureProducts.find(InContent);
	if (Reused == TextureProducts.end())
	{
		return {};
	}
	const auto StagedAsset = Staged.find(Reused->second.Path);
	const auto Candidate = StagedAsset != Staged.end()
	                           ? DecodeAsset(StagedAsset->second.Bytes)
	                           : DecodeAsset(IO.ReadAsync(Reused->second.Path, Cancellation).Get(IO.TaskSystem()));
	auto Record = Candidate.Object;
	std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Record.Value).at("fields").Value)["name"] =
	    WriteValue(std::string{});
	if (Candidate.Header.Id != Reused->second.Reference.Id || HashArchive(Record) != InContent)
	{
		if (StagedAsset == Staged.end())
		{
			throw std::runtime_error("Shared texture content index is stale");
		}
		TextureProducts.erase(Reused);
		return {};
	}
	auto Result = Reused->second;
	Result.Reference.Revision.clear();
	return Result;
}

void FPublication::ClaimProduct(const std::string& InId, const std::string& InKey, const std::string& InContent)
{
	const auto [It, bInserted] = ProductContents.emplace(InId, std::make_pair(InKey, InContent));
	if (!bInserted && It->second.second != InContent)
	{
		throw std::runtime_error("Conflicting products claim one native asset ID " + InId + ": " + It->second.first +
		                         " and " + InKey);
	}
}

FAssetHeader FPublication::MakeHeader(const std::string& InId, const std::string& InKey,
                                      const std::string& InTextureContent, bool bInRoot)
{
	Provenance.OutputIds[InKey] = InId;
	FAssetHeader Header;
	Header.Id = InId;
	if (bInRoot)
	{
		for (const auto& [Path, Fingerprint] : Sources)
		{
			Provenance.Sources.push_back({ImportRelativePath(Path, Source.parent_path()), Fingerprint});
		}
		Header.Import = Provenance;
		return Header;
	}
	Header.Import.emplace();
	Header.Import->Importer = "hyperion.native-product";
	Header.Import->ImporterVersion = 1;
	Header.Import->OutputIds[InKey] = InId;
	for (const auto& [ProductKey, Reference] : LibraryProducts)
	{
		if (Reference.Id == InId)
		{
			Header.Import->OutputIds[ProductKey] = InId;
		}
	}
	if (!InTextureContent.empty())
	{
		Header.Import->Settings["texture_content"] = InTextureContent;
	}
	return Header;
}
} // namespace Hyperion
