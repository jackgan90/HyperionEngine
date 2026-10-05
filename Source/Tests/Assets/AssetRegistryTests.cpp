#include "Hyperion/Assets/AssetEntryNames.h"
#include "Hyperion/Assets/AssetRegistry.h"
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Core/ContentHash.h"
#include "Hyperion/IO/Path.h"
#include "Support/BinaryFixtures.h"
#include "Support/TestSupport.h"
#include <array>
#include <iostream>
#include <limits>

namespace Hyperion
{
struct FRegistryFixture
{
	std::string Name;
	std::vector<FAssetRef> References;
	std::vector<std::uint8_t> Data;
};

template<> const FRecordDescriptor& RecordType<FRegistryFixture>()
{
	static const auto Type = MakeRecord<FRegistryFixture>(
	    "test.registry", {Member("name", &FRegistryFixture::Name), Member("references", &FRegistryFixture::References),
	                      Member("data", &FRegistryFixture::Data)});
	return Type;
}
} // namespace Hyperion

namespace
{
using namespace Hyperion;
using namespace Hyperion::Test;

template<class T> void RejectRegistry(T InWork)
{
	bool bRejected{};
	try
	{
		InWork();
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

std::size_t FixtureMetadataEnd(std::span<const std::byte> InBytes)
{
	return 80 + 24 + ReadFixtureInteger(InBytes, 96, 4) * 16 + ReadFixtureInteger(InBytes, 88, 8);
}

class FRangeFiles final : public IFileSystem
{
public:
	FLocalFileSystem Local;
	std::size_t RangeBytes{};
	std::size_t RangeCalls{};
	std::size_t RangeLimit{};

	FBytes Read(const std::filesystem::path&, std::size_t) override
	{
		throw std::runtime_error("Discovery must not read the bulk payload");
	}

	FBytes ReadRange(const std::filesystem::path& InPath, std::size_t InOffset, std::size_t InSize) override
	{
		HYP_CHECK(InOffset <= RangeLimit && InSize <= RangeLimit - InOffset);
		RangeBytes += InSize;
		++RangeCalls;
		return Local.ReadRange(InPath, InOffset, InSize);
	}

	void WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes) override
	{
		Local.WriteAtomic(InPath, InBytes);
	}

	std::vector<FDirectoryEntry> ListDirectory(const std::filesystem::path& InPath) override
	{
		return Local.ListDirectory(InPath);
	}
};

void CheckMetadata()
{
	const auto Root = std::filesystem::absolute("registry-metadata") / CreateIdentifier();
	FRegistryFixture Texture{"bulk", {}, std::vector<std::uint8_t>(32u * 1024u * 1024u, 127)};
	auto Encoded = EncodeAsset(RecordType<FRegistryFixture>(), &Texture);
	FRangeFiles Files;
	Files.RangeLimit = FixtureMetadataEnd(Encoded.Bytes);
	Files.WriteAtomic(Root / "Bulk.hasset", Encoded.Bytes);
	Files.WriteAtomic(Root / ".cache/Duplicate.hasset", Encoded.Bytes);
	Files.WriteAtomic(Root / ".git/Duplicate.hasset", Encoded.Bytes);
	Files.WriteAtomic(Root / ".publish-fixture/Duplicate.hasset", Encoded.Bytes);
	const auto Found = DiscoverAssets(Files, Root);
	HYP_CHECK(Found.Errors.empty() && Found.Entries.size() == 1);
	HYP_CHECK(Serialize(Found.Entries.front().Header) == Serialize(Encoded.Header));
	HYP_CHECK(Files.RangeBytes < 16u * 1024u);
	HYP_CHECK(Files.RangeCalls == 2);
	Encoded.Bytes.back() ^= std::byte{1};
	Files.WriteAtomic(Root / "Bulk.hasset", Encoded.Bytes);
	HYP_CHECK(DiscoverAssets(Files, Root).Entries.size() == 1);
	bool bRejected{};
	try
	{
		DecodeAsset(Encoded.Bytes);
	}
	catch (const std::runtime_error&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
	Encoded.Bytes.resize(Files.RangeLimit);
	Files.WriteAtomic(Root / "Bulk.hasset", Encoded.Bytes);
	const auto Truncated = DiscoverAssets(Files, Root);
	HYP_CHECK(Truncated.Errors.empty() && Truncated.Entries.size() == 1);
	HYP_CHECK(Serialize(Truncated.Entries.front().Header) == Serialize(Encoded.Header));
	RejectRegistry(
	    [&]
	    {
		    DecodeAsset(Encoded.Bytes);
	    });
	std::filesystem::remove_all(Root);
}

void CheckReservedEntryNames()
{
	for (const auto* Name : {".git", ".cache", ".publish-", ".publish-library", ".publish-arbitrary"})
	{
		HYP_CHECK(IsReservedAssetEntry(Name));
	}
	for (const auto* Name : {"", ".GIT", ".Cache", ".PUBLISH-library", ".assets", "Catalog.hasset",
	                         ".asset-library.hasset", ".publish", "Public.publish-lock"})
	{
		HYP_CHECK(!IsReservedAssetEntry(Name));
	}
	HYP_CHECK(AssetPublicationLeaseName() == ".publish-library");
	HYP_CHECK(IsReservedAssetEntry(AssetPublicationLeaseName()));
}

void CheckDiscoveryEntryPolicy()
{
	FMemoryFileSystem Files;
	const auto Root = std::filesystem::absolute("registry-entry-policy");
	// Memory paths preserve case independently of the host filesystem.
	for (const auto* Name : {".git", ".cache", ".publish-work", ".GIT", ".Cache", ".PUBLISH-work", ".assets"})
	{
		const FRegistryFixture Fixture{Name};
		Files.WriteAtomic(Root / Name / "Entry.hasset", EncodeAsset(RecordType<FRegistryFixture>(), &Fixture).Bytes);
	}
	const auto Found = DiscoverAssets(Files, Root);
	HYP_CHECK(Found.Errors.empty() && Found.Entries.size() == 4);
	for (const auto& Entry : Found.Entries)
	{
		HYP_CHECK(!IsReservedAssetEntry(Entry.Path.parent_path().filename().string()));
	}
}

class FFixtureFiles final : public IFileSystem
{
public:
	FBytes Bytes;
	std::size_t LargestRead{};
	std::size_t RangeCalls{};
	std::size_t ReadLimit = 64u * 1024u;
	std::optional<std::size_t> ShortOffset;
	std::optional<std::size_t> ShortSize;
	std::size_t RemoveBytes = 1;

	FBytes Read(const std::filesystem::path&, std::size_t) override
	{
		throw std::runtime_error("Unexpected complete file read");
	}

	FBytes ReadRange(const std::filesystem::path&, std::size_t InOffset, std::size_t InSize) override
	{
		LargestRead = std::max(LargestRead, InSize);
		++RangeCalls;
		HYP_CHECK(InSize <= ReadLimit);
		if (InOffset > Bytes.size() || InSize > Bytes.size() - InOffset)
		{
			throw std::runtime_error("Truncated fixture range");
		}
		if (ShortOffset == InOffset && (!ShortSize || ShortSize == InSize))
		{
			InSize -= std::min(RemoveBytes, InSize);
		}
		return {Bytes.begin() + InOffset, Bytes.begin() + InOffset + InSize};
	}

	void WriteAtomic(const std::filesystem::path&, std::span<const std::byte>) override
	{
		throw std::runtime_error("Read-only fixture");
	}

	std::vector<FDirectoryEntry> ListDirectory(const std::filesystem::path& InPath) override
	{
		return {{InPath / "Test.hasset", false, {}}};
	}
};

void CheckDiscoveryMutation(const FBytes& InBytes, std::size_t InMaximumRead = 64u * 1024u)
{
	FFixtureFiles Files;
	Files.Bytes = InBytes;
	const auto Result = DiscoverAssets(Files, "registry-malformed");
	HYP_CHECK(Result.Entries.empty() && Result.Errors.size() == 1);
	HYP_CHECK(Files.LargestRead <= InMaximumRead);
}

void CheckMalformedDiscovery()
{
	const FRegistryFixture Value{"metadata", {}, {1, 2, 3, 4}};
	const auto Good = EncodeAsset(RecordType<FRegistryFixture>(), &Value).Bytes;
	const auto End = FixtureMetadataEnd(Good);

	struct FMutation
	{
		std::size_t Offset;
		unsigned Size;
		std::uint64_t Value;
		std::size_t MaximumRead = 64u * 1024u;
	};

	const std::array Mutations{FMutation{0, 1, 0},
	                           FMutation{4, 4, 2},
	                           FMutation{80, 1, 0},
	                           FMutation{84, 4, 1},
	                           FMutation{84, 4, 3},
	                           FMutation{100, 4, 1},
	                           FMutation{8, 8, 0, 104},
	                           FMutation{8, 8, std::numeric_limits<std::uint64_t>::max(), 104},
	                           FMutation{8, 8, FArchiveLimits{}.MaxBytes - 79, 104},
	                           FMutation{88, 8, std::numeric_limits<std::uint64_t>::max(), 104},
	                           FMutation{88, 8, 32u * 1024u * 1024u, 104},
	                           FMutation{96, 4, 1000001, 104},
	                           FMutation{96, 4, std::numeric_limits<std::uint32_t>::max(), 104},
	                           FMutation{104, 8, End - 81},
	                           FMutation{104, 8, End - 79},
	                           FMutation{112, 8, std::numeric_limits<std::uint64_t>::max()}};
	for (const auto Mutation : Mutations)
	{
		auto Bad = Good;
		WriteFixtureInteger(Bad, Mutation.Offset, Mutation.Size, Mutation.Value);
		CheckDiscoveryMutation(Bad, Mutation.MaximumRead);
	}
	for (std::size_t Size = 0; Size < 104; ++Size)
	{
		CheckDiscoveryMutation(FBytes(Good.begin(), Good.begin() + Size));
	}
	for (const auto Size : {std::size_t{119}, End - 1})
	{
		CheckDiscoveryMutation(FBytes(Good.begin(), Good.begin() + Size));
	}
}

void CheckShortDiscoveryReads()
{
	const FRegistryFixture Value{"short", {}, {1, 2, 3, 4}};
	const auto Bytes = EncodeAsset(RecordType<FRegistryFixture>(), &Value).Bytes;
	for (const auto Range : {std::pair{std::size_t{0}, GetNativeAssetMetadataPrefixSize()},
	                         std::pair{std::size_t{80}, FixtureMetadataEnd(Bytes) - 80}})
	{
		for (const auto Removed : {std::size_t{1}, Range.second})
		{
			FFixtureFiles Files;
			Files.Bytes = Bytes;
			Files.ShortOffset = Range.first;
			Files.ShortSize = Range.second;
			Files.RemoveBytes = Removed;
			const auto Found = DiscoverAssets(Files, "registry-short-read");
			HYP_CHECK(Found.Entries.empty() && Found.Errors.size() == 1);
			HYP_CHECK(Found.Errors.begin()->second.find("Incomplete native metadata range") != std::string::npos);
		}
	}
}

void CheckDiscoveryRangeBudget()
{
	const FRegistryFixture Value{"budget", {}, {1, 2, 3, 4}};
	const auto Good = EncodeAsset(RecordType<FRegistryFixture>(), &Value).Bytes;
	constexpr std::size_t MetadataLimit = 32u * 1024u * 1024u;
	HYP_CHECK(ReadFixtureInteger(Good, 96, 4) == 1);
	for (const auto Extent : {MetadataLimit, MetadataLimit + 1})
	{
		FFixtureFiles Files;
		Files.Bytes = Good;
		Files.ReadLimit = MetadataLimit;
		WriteFixtureInteger(Files.Bytes, 8, 8, Extent + 4);
		WriteFixtureInteger(Files.Bytes, 88, 8, Extent - 24 - 16);
		const auto Found = DiscoverAssets(Files, "registry-metadata-budget");
		HYP_CHECK(Found.Entries.empty() && Found.Errors.size() == 1);
		// The exact boundary reaches the unavailable range; one extra byte is rejected before requesting it.
		HYP_CHECK(Files.LargestRead == (Extent == MetadataLimit ? MetadataLimit : GetNativeAssetMetadataPrefixSize()));
		HYP_CHECK(Files.RangeCalls == (Extent == MetadataLimit ? 2u : 1u));
	}
}

FBytes NativeLegacyPayload(const FArchiveNode& InObject)
{
	const FAssetHeader Header{"0123456789abcdef0123456789abcdef", "test.legacy.metadata", 1, HashArchive(InObject)};
	auto StoredHeader = WriteValue(Header);
	auto& Record = std::get<FArchiveNode::FObject>(StoredHeader.Value);
	Record.at("version") = FArchiveNode(std::int64_t{1});
	auto& Fields = std::get<FArchiveNode::FObject>(Record.at("fields").Value);
	Fields.at("schema") = FArchiveNode(std::int64_t{1});
	Fields.erase("import");
	const auto Current =
	    EncodeArchive(FArchiveNode(FArchiveNode::FObject{{"header", std::move(StoredHeader)}, {"object", InObject}}));
	HYP_CHECK(ReadFixtureInteger(Current, 16, 4) == 0);
	auto Legacy = FixtureBytes("4859504101000000");
	Legacy.insert(Legacy.end(), Current.begin() + 24, Current.end());
	auto Native = FixtureBytes("48415354010000000000000000000000");
	WriteFixtureInteger(Native, 8, 8, Legacy.size());
	for (const auto Character : ContentHash(Legacy))
	{
		Native.push_back(std::byte(Character));
	}
	Native.insert(Native.end(), Legacy.begin(), Legacy.end());
	return Native;
}

void CheckLegacyDiscovery()
{
	const auto Object = FArchiveNode(FArchiveNode::FObject{{"type", FArchiveNode(std::string("test.legacy.metadata"))},
	                                                       {"version", FArchiveNode(std::int64_t{1})},
	                                                       {"fields", FArchiveNode(FArchiveNode::FObject{})}});
	const auto Current = EncodeArchive(Object);
	HYP_CHECK(ReadFixtureInteger(Current, 16, 4) == 0);
	auto Legacy = FixtureBytes("4859504101000000");
	Legacy.insert(Legacy.end(), Current.begin() + 24, Current.end());
	for (const auto& Bytes : {Current, Legacy})
	{
		const auto Loaded = DecodeAsset(Bytes);
		HYP_CHECK(Loaded.bLegacy && Loaded.Header.TypeId == "test.legacy.metadata");
		CheckDiscoveryMutation(Bytes);
	}
	const auto Native = NativeLegacyPayload(Object);
	const auto Loaded = DecodeAsset(Native);
	HYP_CHECK(!Loaded.bLegacy && Loaded.Header.TypeId == "test.legacy.metadata");
	CheckDiscoveryMutation(Native);
}

void CheckRelocation()
{
	FTaskSystem Tasks(2, 1);
	auto Files = std::make_shared<FMemoryFileSystem>();
	FIOService IO(Tasks, Files);
	FAssetService Assets(IO);
	const auto Root = std::filesystem::absolute("registry-relocation");
	const auto OldPath = Root / "Child.hasset";
	const auto NewPath = Root / "Moved/Child.hasset";
	Assets.SaveAsync(OldPath, std::make_shared<const FRegistryFixture>(FRegistryFixture{"old"})).Get(Tasks);
	const auto Held = Assets.LoadAsync<FRegistryFixture>(OldPath).GetAsset(Tasks);
	const FAssetRef Pinned{Held->Header.Id, "Child.hasset", Held->Header.TypeId, Held->Header.Revision};
	Assets
	    .SaveAsync(Root / "Parent.hasset",
	               std::make_shared<const FRegistryFixture>(FRegistryFixture{"parent", {Pinned}}))
	    .Get(Tasks);
	Files->WriteAtomic(NewPath, Files->Read(OldPath, 1024 * 1024));
	Files->Remove(OldPath);
	const auto Discovery = DiscoverAssets(*Files, Root);
	HYP_CHECK(Discovery.Errors.empty() && Discovery.Entries.size() == 2);
	Assets.SetAssetIndex(BuildAssetIndex(Discovery.Entries), Root);
	Assets.ClearCache();
	const auto Graph = Assets.LoadGraphAsync(Root / "Parent.hasset").Get(Tasks);
	HYP_CHECK(Graph->Failures.empty() && Graph->Assets.contains(NewPath));
	HYP_CHECK(Graph->Root->Header.Dependencies.front().Reference.Revision.empty());
	Assets.SaveAsync(NewPath, std::make_shared<const FRegistryFixture>(FRegistryFixture{"new"})).Get(Tasks);
	HYP_CHECK(Assets.LoadByIdAsync(Held->Header.Id).Get(Tasks)->As<FRegistryFixture>()->Name == "new");
	HYP_CHECK(Held->As<FRegistryFixture>()->Name == "old");
	Files->WriteAtomic(Root / "Duplicate.hasset", Files->Read(NewPath, 1024 * 1024));
	std::string Error;
	try
	{
		DiscoverAssets(*Files, Root);
	}
	catch (const std::runtime_error& Failure)
	{
		Error = Failure.what();
	}
	HYP_CHECK(Error.find("Duplicate asset ID") != std::string::npos &&
	          Error.find("Moved/Child.hasset") != std::string::npos);
}
} // namespace

int main()
{
	try
	{
		CheckReservedEntryNames();
		CheckDiscoveryEntryPolicy();
		CheckMetadata();
		CheckMalformedDiscovery();
		CheckShortDiscoveryReads();
		CheckDiscoveryRangeBudget();
		CheckLegacyDiscovery();
		CheckRelocation();
		std::cout
		    << "Metadata-only discovery, integrity, relocation, current references and duplicate diagnostics passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
