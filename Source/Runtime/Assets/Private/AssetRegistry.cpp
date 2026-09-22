#include "Hyperion/Assets/AssetRegistry.h"
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/IO/MountedFileSystem.h"
#include "Hyperion/IO/Path.h"
#include <set>

namespace Hyperion
{
namespace
{
std::uint64_t Integer(std::span<const std::byte> InBytes, std::size_t InOffset, unsigned InSize)
{
	std::uint64_t Value{};
	for (unsigned Index = 0; Index < InSize; ++Index)
	{
		Value |= std::uint64_t(std::to_integer<unsigned>(InBytes[InOffset + Index])) << (Index * 8);
	}
	return Value;
}
} // namespace

FAssetHeader ReadAssetHeader(IFileSystem& InFiles, const std::filesystem::path& InPath)
{
	constexpr std::size_t PrefixSize = 80;
	constexpr std::size_t MetadataLimit = 32u * 1024u * 1024u;
	const auto Prefix = InFiles.ReadRange(InPath, 0, PrefixSize + 24);
	const std::string Magic(reinterpret_cast<const char*>(Prefix.data()), 4);
	if (Magic != "HAST" || Integer(Prefix, 4, 4) != 1 || Integer(Prefix, PrefixSize + 4, 4) != 2)
	{
		throw std::runtime_error("Discovery requires a current native container: " + PathToUtf8(InPath));
	}
	const auto Total = Integer(Prefix, 8, 8);
	const auto Metadata = Integer(Prefix, PrefixSize + 8, 8);
	const auto Blocks = Integer(Prefix, PrefixSize + 16, 4);
	if (Total > FArchiveLimits{}.MaxBytes - PrefixSize || Metadata > MetadataLimit ||
	    Blocks > FArchiveLimits{}.MaxNodes)
	{
		throw std::runtime_error("Native metadata exceeds discovery limits");
	}
	const auto Size = 24 + Blocks * 16 + Metadata;
	if (Size > Total || Size > MetadataLimit)
	{
		throw std::runtime_error("Invalid native metadata size");
	}
	const auto Bytes = InFiles.ReadRange(InPath, PrefixSize, static_cast<std::size_t>(Size));
	const auto Envelope = DecodeArchiveMetadata(Bytes, static_cast<std::size_t>(Total));
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
			if (Name == ".git" || Name == ".cache" || Name.starts_with(".publish-"))
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
