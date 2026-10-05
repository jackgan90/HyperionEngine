#include "Hyperion/Assets/AssetRegistry.h"
#include "Hyperion/Assets/AssetEntryNames.h"
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Assets/NativeAsset.h"
#include "Hyperion/IO/MountedFileSystem.h"
#include "Hyperion/IO/Path.h"
#include <set>

namespace Hyperion
{
namespace
{
FBytes ReadDiscoveryRange(IFileSystem& InFiles, const std::filesystem::path& InPath, std::size_t InOffset,
                          std::size_t InSize)
{
	auto Bytes = InFiles.ReadRange(InPath, InOffset, InSize);
	if (Bytes.size() != InSize)
	{
		throw std::runtime_error("Incomplete native metadata range: " + PathToUtf8(InPath));
	}
	return Bytes;
}
} // namespace

FAssetHeader ReadAssetHeader(IFileSystem& InFiles, const std::filesystem::path& InPath)
{
	constexpr std::size_t MetadataLimit = 32u * 1024u * 1024u;
	const auto Prefix = ReadDiscoveryRange(InFiles, InPath, 0, GetNativeAssetMetadataPrefixSize());
	const auto Metadata = ProbeNativeAssetMetadata(Prefix);
	if (Metadata.Size > MetadataLimit)
	{
		throw std::runtime_error("Native metadata exceeds discovery limits");
	}
	const auto Bytes = ReadDiscoveryRange(InFiles, InPath, Metadata.Payload.Offset, Metadata.Size);
	const auto Envelope = DecodeArchiveMetadata(Bytes, Metadata.Payload.Size);
	const auto& Fields = std::get<FArchiveNode::FObject>(Envelope.Value);
	auto Header = ReadValue<FAssetHeader>(Fields.at("header"));
	ValidateAssetHeader(Header);
	return Header;
}

FAssetDiscovery DiscoverAssets(IFileSystem& InFiles, const std::filesystem::path& InRoot)
{
	std::vector<std::filesystem::path> Pending{InFiles.Normalize(InRoot)};
	FAssetDiscovery Result;
	std::map<std::string, std::filesystem::path> Ids;
	while (!Pending.empty())
	{
		const auto Directory = std::move(Pending.back());
		Pending.pop_back();
		for (const auto& Entry : InFiles.ListDirectory(Directory))
		{
			const auto Name = PathToUtf8(Entry.Path.filename());
			if (IsReservedAssetEntry(Name))
			{
				continue;
			}
			if (!Entry.Error.empty())
			{
				Result.Errors.emplace(Entry.Path, Entry.Error);
				continue;
			}
			if (Entry.bDirectory)
			{
				Pending.push_back(Entry.Path);
			}
			else if (Entry.Path.extension() == ".hasset")
			{
				FAssetHeader Header;
				try
				{
					Header = ReadAssetHeader(InFiles, Entry.Path);
				}
				catch (const std::exception& Failure)
				{
					Result.Errors.emplace(Entry.Path, Failure.what());
					continue;
				}
				if (const auto [It, bInserted] = Ids.emplace(Header.Id, Entry.Path); !bInserted)
				{
					throw std::runtime_error("Duplicate asset ID " + Header.Id + ": " + PathToUtf8(It->second) +
					                         " and " + PathToUtf8(Entry.Path));
				}
				Result.Entries.push_back({Entry.Path, std::move(Header)});
			}
		}
	}
	std::sort(Result.Entries.begin(), Result.Entries.end(),
	          [](const auto& InA, const auto& InB)
	          {
		          return InA.Path < InB.Path;
	          });
	return Result;
}

std::vector<FAssetRef> BuildAssetIndex(std::span<const FAssetRegistryEntry> InEntries)
{
	std::vector<FAssetRef> Result;
	for (const auto& Entry : InEntries)
	{
		Result.push_back({Entry.Header.Id, PathToUtf8(Entry.Path), Entry.Header.TypeId, {}});
	}
	return Result;
}

void IndexDiscoveredAssets(FAssetService& InAssets, const std::filesystem::path& InLocalRoot)
{
	auto& Files = *InAssets.FileSystem();
	std::vector<std::filesystem::path> Roots;
	const auto LocalRoot = Files.Normalize(InLocalRoot);
	if (const auto* Mounted = dynamic_cast<FMountedFileSystem*>(&Files))
	{
		for (const auto& Mount : Mounted->GetMounts())
		{
			Roots.push_back(Mount.Root);
		}
		if (!IsPackagePath(LocalRoot))
		{
			Roots.push_back(LocalRoot);
		}
	}
	else
	{
		Roots.push_back(LocalRoot);
	}
	for (const auto& Root : Roots)
	{
		const auto Found = DiscoverAssets(Files, Root);
		InAssets.AddAssetIndex(BuildAssetIndex(Found.Entries), Root);
	}
}
} // namespace Hyperion
