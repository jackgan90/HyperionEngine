#include "AssetPublicationInternal.h"
#include "Hyperion/IO/Path.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FAssetLibraryIndex>()
{
	static const auto Type =
	    MakeRecord<FAssetLibraryIndex>("hyperion.assetlibraryindex", {Member("entries", &FAssetLibraryIndex::Entries)});
	return Type;
}

std::filesystem::path ImportProductPath(const std::filesystem::path& InSource, std::string_view InKey)
{
	return InSource.parent_path() / (InSource.filename().native() + std::filesystem::path(".parts").native()) /
	       PathFromUtf8(InKey);
}

void FPublication::LoadLibrary()
{
	const auto Existing = IO.TryReadAsync(Library / ".asset-library.hasset", Cancellation).Get(IO.TaskSystem());
	if (*Existing)
	{
		const auto Document = DecodeAsset(**Existing);
		LibraryIndex = *std::static_pointer_cast<FAssetLibraryIndex>(
		    ReadRecord(RecordType<FAssetLibraryIndex>(), Document.Object));
		LibraryHeader = Document.Header;
	}
}

void FPublication::SaveLibrary()
{
	auto Encoded = EncodeAsset(RecordType<FAssetLibraryIndex>(), &LibraryIndex, LibraryHeader);
	Write(Library / ".asset-library.hasset", Encoded);
	LibraryHeader = std::move(Encoded.Header);
}

void FPublication::AddProducts(const std::filesystem::path& InSource, const FConvertedAsset& InAsset)
{
	for (const auto& Product : InAsset.Products)
	{
		const auto Path = ImportProductPath(InSource, Product.Key);
		FConvertedAsset ConvertedProduct;
		ConvertedProduct.Type = Product.Type;
		ConvertedProduct.Object = Product.Object;
		ConvertedProduct.Importer = InAsset.Importer;
		ConvertedProduct.ImporterVersion = InAsset.ImporterVersion;
		// Root conversion already charges retained product bytes and tracks all source reads.
		ConvertedProduct.ProductRoot = InSource;
		ConvertedProduct.StableKey = Product.SharedKey.empty() ? "product/" + ImportPathString(InSource) + "/" +
		                                                             Product.Key + "|" + Product.Type->Id
		                                                       : "shared/" + Product.SharedKey + "|" + Product.Type->Id;
		if (!Converted.emplace(std::make_pair(Path, Product.Type->Id), std::move(ConvertedProduct)).second)
		{
			throw std::runtime_error("Duplicate named product path during publication: " + ImportPathString(Path));
		}
	}
}
} // namespace Hyperion
