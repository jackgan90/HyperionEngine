#include "Hyperion/Assets/AssetRegistry.h"
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Core/ContentHash.h"
#include "Hyperion/IO/Path.h"
#include "Support/TestSupport.h"
#include <iostream>

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

class FRangeFiles final : public IFileSystem
{
public:
	FLocalFileSystem Local;
	std::size_t RangeBytes{};

	FBytes Read(const std::filesystem::path&, std::size_t) override
	{
		throw std::runtime_error("Discovery must not read the bulk payload");
	}

	FBytes ReadRange(const std::filesystem::path& InPath, std::size_t InOffset, std::size_t InSize) override
	{
		RangeBytes += InSize;
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
	Files.WriteAtomic(Root / "Bulk.hasset", Encoded.Bytes);
	Files.WriteAtomic(Root / ".cache/Duplicate.hasset", Encoded.Bytes);
	Files.WriteAtomic(Root / ".git/Duplicate.hasset", Encoded.Bytes);
	const auto Found = DiscoverAssets(Files, Root);
	HYP_CHECK(Found.Errors.empty() && Found.Entries.size() == 1);
	HYP_CHECK(Serialize(Found.Entries.front().Header) == Serialize(Encoded.Header));
	HYP_CHECK(Files.RangeBytes < 16u * 1024u);
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
	std::filesystem::remove_all(Root);
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
	Assets.SetCatalog(BuildAssetCatalog(Discovery.Entries), Root);
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
		CheckMetadata();
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
