#include "AssetPublicationInternal.h"
#include "Hyperion/Assets/AssetAuthoring.h"
#include "Hyperion/Assets/AssetRegistry.h"
#include "Hyperion/Core/ContentHash.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Scene/SceneManifest.h"
#include <cctype>

namespace Hyperion
{
std::string FPublication::StableSourceKey(const std::filesystem::path& InPath) const
{
	const auto Relative = InPath.lexically_relative(SourceRoot);
	if (Relative.empty() || Relative.is_absolute())
	{
		throw std::runtime_error("Import source is outside configured source root: " + ImportPathString(InPath));
	}
	return SourceId + "/" + ImportPathString(Relative);
}

std::string FPublication::PortableKey(std::string InKey) const
{
	if (InKey.starts_with("image/"))
	{
		const auto Srgb = InKey.rfind("/srgb/");
		const auto Linear = InKey.rfind("/linear/");
		const auto End = Srgb == std::string::npos ? Linear : Srgb;
		if (End != std::string::npos)
		{
			return "image/" + StableSourceKey(PathFromUtf8(InKey.substr(6, End - 6))) + InKey.substr(End);
		}
	}
	if (!SourceRoot.empty())
	{
		const auto Prefix = ImportPathString(SourceRoot) + "/";
		const auto Replacement = SourceId + "/";
		for (auto Position = InKey.find(Prefix); Position != std::string::npos;
		     Position = InKey.find(Prefix, Position + Replacement.size()))
		{
			InKey.replace(Position, Prefix.size(), Replacement);
		}
	}
	if (InKey.find(":/") != std::string::npos || InKey.find('\\') != std::string::npos)
	{
		throw std::runtime_error("Import product key contains a local machine path");
	}
	return InKey;
}

std::filesystem::path ImportProductPath(const std::filesystem::path& InSource, std::string_view InKey)
{
	return InSource.parent_path() / (InSource.filename().native() + std::filesystem::path(".parts").native()) /
	       PathFromUtf8(InKey);
}

void FPublication::LoadLibrary()
{
	const auto Discovery = DispatchAsync<FAssetDiscovery>(
	                           IO.TaskSystem(), {EDomain::Io},
	                           [Storage = IO.FileSystem(), Directory = Library]
	                           {
		                           return DiscoverAssets(*Storage, Directory);
	                           },
	                           Cancellation)
	                           .Get(IO.TaskSystem());
	for (const auto& [Path, Error] : Discovery->Errors)
	{
		if (Path != Output)
		{
			throw std::runtime_error(ImportPathString(Path) + ": " + Error);
		}
	}
	for (const auto& Entry : Discovery->Entries)
	{
		ExistingAssets.emplace(Entry.Header.Id, FPublishedAsset{{Entry.Header.Id, ImportPathString(Entry.Path),
		                                                         Entry.Header.TypeId, Entry.Header.Revision},
		                                                        Entry.Path});
	}
	for (const auto& Entry : Discovery->Entries)
	{
		if (!Entry.Header.Import)
		{
			continue;
		}
		if (const auto It = Entry.Header.Import->Settings.find("texture_content");
		    It != Entry.Header.Import->Settings.end())
		{
			TextureProducts.try_emplace(It->second, ExistingAssets.at(Entry.Header.Id));
		}
		for (const auto& [Key, Id] : Entry.Header.Import->OutputIds)
		{
			const auto Target = ExistingAssets.find(Id);
			if (Key == "$root" || Target == ExistingAssets.end())
			{
				continue;
			}
			const auto [It, bInserted] = LibraryProducts.emplace(Key, Target->second.Reference);
			if (!bInserted && It->second.Id != Id)
			{
				throw std::runtime_error("Conflicting import product identity: " + Key);
			}
		}
	}
}

FAssetService& FPublication::IndexedAssets()
{
	if (!NativeAssets)
	{
		auto Assets = std::make_unique<FAssetService>(IO);
		for (const auto& Importer : Importers)
		{
			Assets->Types().Register(*Importer.Type);
		}
		Assets->Types().Register<FSceneManifest>();
		DispatchAsync<bool>(
		    IO.TaskSystem(), {EDomain::Io},
		    [&]
		    {
			    IndexDiscoveredAssets(*Assets, Library);
			    return true;
		    },
		    Cancellation)
		    .Get(IO.TaskSystem());
		NativeAssets = std::move(Assets);
	}
	return *NativeAssets;
}

std::string FPublication::SelectId(const std::string& InKey, const FConvertedAsset& InAsset, bool bInRoot) const
{
	if (bInRoot)
	{
		return RootId;
	}
	if (const auto It = PreviousIds.find(InKey); It != PreviousIds.end() && ExistingAssets.contains(It->second))
	{
		return It->second;
	}
	if (const auto It = LibraryProducts.find(InKey); It != LibraryProducts.end())
	{
		return It->second.Id;
	}
	return InAsset.NativeHeader ? InAsset.NativeHeader->Id : CreateIdentifier();
}

std::filesystem::path FPublication::ProductDestination(std::string_view InId, const FConvertedAsset& InAsset) const
{
	if (const auto It = ExistingAssets.find(std::string(InId)); It != ExistingAssets.end())
	{
		if (It->second.Reference.TypeId != InAsset.Type->Id)
		{
			throw std::runtime_error("Import product changed native type");
		}
		return It->second.Path;
	}
	return AssetProductPath(*InAsset.Type, InAsset.Object.get(), Library, InId);
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
		ConvertedProduct.StableKey =
		    Product.SharedKey.empty()
		        ? "product/" + StableSourceKey(InSource) + "/" + Product.Key + "|" + Product.Type->Id
		        : "shared/" + PortableKey(Product.SharedKey) + "|" + Product.Type->Id;
		if (!Converted.emplace(std::make_pair(Path, Product.Type->Id), std::move(ConvertedProduct)).second)
		{
			throw std::runtime_error("Duplicate named product path during publication: " + ImportPathString(Path));
		}
	}
}
} // namespace Hyperion
