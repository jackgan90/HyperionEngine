#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Core/ContentHash.h"
#include "Support/TestSupport.h"
#include <iostream>
#include <thread>

namespace Hyperion
{
struct FNativeFixture
{
	std::string Name;
	std::vector<FAssetRef> References;
	std::vector<std::uint8_t> Data;
};

template<> const FRecordDescriptor& RecordType<FNativeFixture>()
{
	static const auto Type =
	    MakeRecord<FNativeFixture>("test.native.fixture", {Member("name", &FNativeFixture::Name, {true}),
	                                                       Member("references", &FNativeFixture::References),
	                                                       Member("data", &FNativeFixture::Data)});
	return Type;
}

struct FDeepNativeFixture
{
	std::vector<FDeepNativeFixture> Children;
	bool operator==(const FDeepNativeFixture&) const = default;
};

template<> const FRecordDescriptor& RecordType<FDeepNativeFixture>()
{
	static const auto Type =
	    MakeRecord<FDeepNativeFixture>("test.native.deep", {Member("children", &FDeepNativeFixture::Children)});
	return Type;
}
} // namespace Hyperion

namespace
{
using namespace Hyperion;

template<class F> void Reject(F InWork, std::string_view InExpected)
{
	bool bFailed = false;
	try
	{
		InWork();
	}
	catch (const std::exception& Error)
	{
		bFailed = std::string_view(Error.what()).find(InExpected) != std::string_view::npos;
		if (!bFailed)
		{
			std::cerr << "Unexpected error: " << Error.what() << '\n';
		}
	}
	HYP_CHECK(bFailed);
}

FAssetHeader Store(FMemoryFileSystem& InFiles, const std::filesystem::path& InPath, const FNativeFixture& InValue,
                   FAssetHeader InHeader = {})
{
	auto Encoded = EncodeAsset(RecordType<FNativeFixture>(), &InValue, std::move(InHeader));
	InFiles.WriteAtomic(InPath, Encoded.Bytes);
	return Encoded.Header;
}

std::vector<std::byte> TamperedEnvelope(const FEncodedAsset& InAsset, unsigned InCase)
{
	auto Node = DecodeArchive(std::span(InAsset.Bytes).subspan(80));
	auto& Fields = std::get<FArchiveNode::FObject>(Node.Value);
	auto Header = ReadValue<FAssetHeader>(Fields.at("header"));
	if (InCase == 0)
	{
		Header.TypeId = "forged.type";
	}
	if (InCase == 1)
	{
		Header.Revision = std::string(64, '0');
	}
	if (InCase == 2)
	{
		Header.Dependencies = {{"forged", {"", "missing.hasset", "test.native.fixture", ""}}};
	}
	Fields["header"] = WriteValue(Header);
	const auto Payload = EncodeArchive(Node);
	const auto Digest = ContentHash(Payload);
	std::vector<std::byte> Bytes(InAsset.Bytes.begin(), InAsset.Bytes.begin() + 80);
	for (unsigned Index = 0; Index < 8; ++Index)
	{
		Bytes[8 + Index] = std::byte((Payload.size() >> (Index * 8)) & 255);
	}
	for (std::size_t Index = 0; Index < Digest.size(); ++Index)
	{
		Bytes[16 + Index] = std::byte(Digest[Index]);
	}
	Bytes.insert(Bytes.end(), Payload.begin(), Payload.end());
	return Bytes;
}

void CheckContainer()
{
	HYP_CHECK(ContentHash({}) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	FNativeFixture Value{"binary", {}, {1, 2, 3, 4}};
	auto Encoded = EncodeAsset(RecordType<FNativeFixture>(), &Value);
	HYP_CHECK(IsAssetIdentifier(Encoded.Header.Id) && IsAssetRevision(Encoded.Header.Revision));
	const auto Copy = EncodeAsset(RecordType<FNativeFixture>(), &Value, Encoded.Header);
	HYP_CHECK(Encoded.Bytes == Copy.Bytes);
	auto Decoded = DecodeAsset(Encoded.Bytes);
	HYP_CHECK(!Decoded.bLegacy && ReadValue<FNativeFixture>(Decoded.Object).Data == Value.Data);
	auto Bad = Encoded.Bytes;
	Bad.back() ^= std::byte{1};
	Reject(
	    [&]
	    {
		    DecodeAsset(Bad);
	    },
	    "integrity");
	Bad = Encoded.Bytes;
	Bad[4] = std::byte{99};
	Reject(
	    [&]
	    {
		    DecodeAsset(Bad);
	    },
	    "container header");
	Bad.resize(40);
	Reject(
	    [&]
	    {
		    DecodeAsset(Bad);
	    },
	    "container header");
	Reject(
	    [&]
	    {
		    DecodeAsset(TamperedEnvelope(Encoded, 0));
	    },
	    "type or schema mismatch");
	Reject(
	    [&]
	    {
		    DecodeAsset(TamperedEnvelope(Encoded, 1));
	    },
	    "revision mismatch");
	Reject(
	    [&]
	    {
		    DecodeAsset(TamperedEnvelope(Encoded, 2));
	    },
	    "dependency table mismatch");
	HYP_CHECK(HashArchive(WriteValue(Value)) == ContentHash(Serialize(Value)));
	FArchiveLimits Tiny;
	Tiny.MaxBytes = 1;
	Reject(
	    [&]
	    {
		    DecodeAsset(Encoded.Bytes, Tiny);
	    },
	    "size");
	auto Legacy = DecodeAsset(Serialize(Value));
	HYP_CHECK(Legacy.bLegacy && ReadValue<FNativeFixture>(Legacy.Object).Name == Value.Name);
	auto Upgraded = EncodeAsset(RecordType<FNativeFixture>(), &Value, Legacy.Header);
	HYP_CHECK(!DecodeAsset(Upgraded.Bytes).bLegacy && Upgraded.Header.Id == Legacy.Header.Id);
}

void CheckNativeSizeBudget()
{
	const FNativeFixture Value{"bounded", {}, {1, 2, 3}};
	const auto Encoded = EncodeAsset(RecordType<FNativeFixture>(), &Value);
	for (const auto Limit : {std::size_t{0}, std::size_t{79}, Encoded.Bytes.size() - 1})
	{
		FArchiveLimits Limits;
		Limits.MaxBytes = Limit;
		Reject(
		    [&]
		    {
			    EncodeAsset(RecordType<FNativeFixture>(), &Value, Encoded.Header, Limits);
		    },
		    "limit");
	}
	FArchiveLimits Limits;
	Limits.MaxBytes = Encoded.Bytes.size();
	const auto Exact = EncodeAsset(RecordType<FNativeFixture>(), &Value, Encoded.Header, Limits);
	HYP_CHECK(Exact.Bytes == Encoded.Bytes &&
	          Serialize(DecodeAsset(Exact.Bytes, Limits).Header) == Serialize(Encoded.Header));
}

void CheckCustomArchiveLimits()
{
	FDeepNativeFixture Value;
	auto* Current = &Value;
	for (unsigned Index = 0; Index < 24; ++Index)
	{
		Current->Children.emplace_back();
		Current = &Current->Children.front();
	}
	FArchiveLimits Limits;
	Limits.MaxDepth = 256;
	const auto Encoded = EncodeAsset(RecordType<FDeepNativeFixture>(), &Value, {}, Limits);
	const auto LegacyBytes = EncodeArchive(WriteValue(Value), Limits);
	for (const auto& Bytes : {Encoded.Bytes, LegacyBytes})
	{
		Reject(
		    [&]
		    {
			    DecodeAsset(Bytes);
		    },
		    "complexity limit");
		const auto Decoded = DecodeAsset(Bytes, Limits);
		HYP_CHECK(ReadValue<FDeepNativeFixture>(Decoded.Object) == Value);
		HYP_CHECK(Decoded.Header.Revision == Encoded.Header.Revision);
	}
	HYP_CHECK(!DecodeAsset(Encoded.Bytes, Limits).bLegacy && DecodeAsset(LegacyBytes, Limits).bLegacy);
}

void CheckGraphDrain()
{
	FTaskSystem Tasks(1, 1);
	auto Files = std::make_shared<FMemoryFileSystem>();
	FIOService IO(Tasks, Files);
	const auto Directory = std::filesystem::absolute("graph-drain");
	const auto Child = Store(*Files, Directory / "child.hasset", {"child"});
	Store(*Files, Directory / "root.hasset",
	      {"root", {{Child.Id, "child.hasset", RecordType<FNativeFixture>().Id, Child.Revision}}});
	std::atomic<bool> bStarted{};
	std::atomic<bool> bRelease{};
	const auto Blocker = Tasks.Dispatch({EDomain::Worker},
	                                    [&]
	                                    {
		                                    bStarted = true;
		                                    bStarted.notify_one();
		                                    bRelease.wait(false);
	                                    });
	bStarted.wait(false);
	FAssetService Assets(IO);
	Assets.Types().Register<FNativeFixture>();
	const auto Request = Assets.LoadGraphAsync(Directory / "root.hasset");
	std::jthread Releaser(
	    [&]
	    {
		    for (;;)
		    {
			    try
			    {
				    Assets.LoadAsync(Directory / "root.hasset");
			    }
			    catch (const std::logic_error&)
			    {
				    break;
			    }
			    std::this_thread::yield();
		    }
		    bRelease = true;
		    bRelease.notify_one();
	    });
	Assets.Drain();
	const auto Graph = Request.Get(Tasks);
	HYP_CHECK(Graph->Failures.empty() && Graph->Assets.size() == 2);
	HYP_CHECK(Assets.Statistics().InFlight == 0 && Assets.Statistics().Entries == 0);
	Reject(
	    [&]
	    {
		    Assets.LoadAsync(Directory / "child.hasset");
	    },
	    "closing");
	Tasks.Wait(Blocker);
}

void CheckService(FTaskSystem& InTasks, FIOService& InIO, FMemoryFileSystem& InFiles,
                  const std::filesystem::path& InRoot)
{
	FAssetService Assets(InIO);
	auto Snapshot = std::make_shared<FNativeFixture>(FNativeFixture{"original", {}, {1, 2}});
	Assets.SaveAsync<FNativeFixture>(InRoot / "saved.hasset", Snapshot).Get(InTasks);
	auto Old = Assets.LoadAsync<FNativeFixture>(InRoot / "saved.hasset").Get(InTasks);
	const auto FirstHeader = Assets.LoadAsync(InRoot / "saved.hasset").Get(InTasks)->Header;
	Snapshot->Name = "first";
	auto FirstSave = Assets.SaveAsync<FNativeFixture>(InRoot / "saved.hasset", Snapshot);
	auto FirstLoad = Assets.LoadAsync<FNativeFixture>(InRoot / "saved.hasset");
	Snapshot->Name = "second";
	auto SecondSave = Assets.SaveAsync<FNativeFixture>(InRoot / "saved.hasset", Snapshot);
	auto SecondLoad = Assets.LoadAsync<FNativeFixture>(InRoot / "saved.hasset");
	Snapshot->Name = "caller edits after admission";
	HYP_CHECK(FirstLoad.Get(InTasks)->Name == "first");
	HYP_CHECK(SecondLoad.Get(InTasks)->Name == "second" && Old->Name == "original");
	FirstSave.Get(InTasks);
	SecondSave.Get(InTasks);
	HYP_CHECK(Assets.LoadAsync(InRoot / "saved.hasset").Get(InTasks)->Header.Id == FirstHeader.Id);
	Assets.ClearCache();
	const auto Reads = InIO.Statistics().Reads.load();
	auto Cancelled = Assets.LoadAsync<FNativeFixture>(InRoot / "saved.hasset");
	auto Shared = Assets.LoadAsync<FNativeFixture>(InRoot / "saved.hasset");
	Cancelled.Cancel();
	HYP_CHECK(Shared.Get(InTasks)->Name == "second");
	Reject(
	    [&]
	    {
		    Cancelled.Get(InTasks);
	    },
	    "canceled");
	HYP_CHECK(InIO.Statistics().Reads == Reads + 1);
	auto Untyped = Assets.LoadAsync(InRoot / "saved.hasset").Get(InTasks);
	HYP_CHECK(Untyped->As<FNativeFixture>()->Name == "second");
	Reject(
	    [&]
	    {
		    Untyped->As<int>();
	    },
	    "type mismatch");
	Reject(
	    [&]
	    {
		    Assets.LoadAsync<FNativeFixture>(InRoot / "source.gltf").Get(InTasks);
	    },
	    "import");
	auto Failed = Assets.LoadAsync<FNativeFixture>(InRoot / "missing.hasset");
	Reject(
	    [&]
	    {
		    Failed.Get(InTasks);
	    },
	    "File not found");
	Store(InFiles, InRoot / "missing.hasset", {"repaired"});
	HYP_CHECK(Assets.LoadAsync<FNativeFixture>(InRoot / "missing.hasset").Get(InTasks)->Name == "repaired");
	Reject(
	    [&]
	    {
		    Failed.Get(InTasks);
	    },
	    "File not found");
	Assets.Drain();
}

void CheckGraph(FTaskSystem& InTasks, FIOService& InIO, FMemoryFileSystem& InFiles, const std::filesystem::path& InRoot)
{
	const auto Type = RecordType<FNativeFixture>().Id;
	const auto Child = Store(InFiles, InRoot / "child.hasset", {"child"});
	FAssetRef Reference{Child.Id, "child.hasset", Type, Child.Revision};
	FNativeFixture Root{"root", {Reference, Reference, {{}, "absent.hasset", Type, {}}}};
	Store(InFiles, InRoot / "root.hasset", Root);
	FAssetService Assets(InIO);
	Assets.Types().Register<FNativeFixture>();
	auto Graph = Assets.LoadGraphAsync(InRoot / "root.hasset").Get(InTasks);
	HYP_CHECK(Graph->Assets.size() == 2 && Graph->Failures.size() == 1);
	HYP_CHECK(Graph->Failures[0].Field == "references[2]");
	Reference.TypeId = "wrong.type";
	Reject(
	    [&]
	    {
		    Assets.LoadReferenceAsync(Reference, InRoot / "root.hasset").Get(InTasks);
	    },
	    "mismatch");
	Reference.TypeId = Type;
	Reference.Revision = std::string(64, '0');
	Reject(
	    [&]
	    {
		    Assets.LoadReferenceAsync(Reference, InRoot / "root.hasset").Get(InTasks);
	    },
	    "mismatch");
	Store(InFiles, InRoot / "a.hasset", {"a", {{{}, "b.hasset", Type, {}}}});
	Store(InFiles, InRoot / "b.hasset", {"b", {{{}, "a.hasset", Type, {}}}});
	auto Cycle = Assets.LoadGraphAsync(InRoot / "a.hasset").Get(InTasks);
	HYP_CHECK(Cycle->Failures.size() == 1 && Cycle->Failures[0].Error.find("cycle") != std::string::npos);
	const auto Moved = InRoot / "moved";
	InFiles.WriteAtomic(Moved / "child.hasset", InFiles.Read(InRoot / "child.hasset", 100000));
	InFiles.WriteAtomic(Moved / "root.hasset", InFiles.Read(InRoot / "root.hasset", 100000));
	HYP_CHECK(Assets.LoadGraphAsync(Moved / "root.hasset").Get(InTasks)->Assets.size() == 2);
	Reference = {Child.Id, "child.hasset", Type, Child.Revision};
	Assets.SetCatalog({{Reference}}, Moved);
	HYP_CHECK(Assets.LoadByIdAsync(Child.Id).Get(InTasks)->Path == Moved / "child.hasset");
	Reject(
	    [&]
	    {
		    Assets.SetCatalog({{Reference, Reference}}, Moved);
	    },
	    "Duplicate");
	Assets.Drain();
}

void CheckCache(FTaskSystem& InTasks, FIOService& InIO, FMemoryFileSystem& InFiles, const std::filesystem::path& InRoot)
{
	FAssetService Assets(InIO, {2, 100000, 32, 32});
	std::shared_ptr<const FNativeFixture> Retained;
	for (unsigned Index = 0; Index < 5; ++Index)
	{
		const auto Path = InRoot / (std::to_string(Index) + ".hasset");
		Store(InFiles, Path, {std::to_string(Index), {}, std::vector<std::uint8_t>(1000)});
		auto Value = Assets.LoadAsync<FNativeFixture>(Path).Get(InTasks);
		if (Index == 0)
		{
			Retained = Value;
		}
		HYP_CHECK(Assets.Statistics().Entries <= 2);
	}
	HYP_CHECK(Retained->Name == "0" && Retained->Data.size() == 1000);
	Assets.Invalidate(InRoot / "4.hasset");
	Store(InFiles, InRoot / "4.hasset", {"replacement"});
	HYP_CHECK(Assets.LoadAsync<FNativeFixture>(InRoot / "4.hasset").Get(InTasks)->Name == "replacement");
	Assets.Drain();
}
} // namespace

int main()
{
	try
	{
		CheckContainer();
		CheckNativeSizeBudget();
		CheckCustomArchiveLimits();
		CheckGraphDrain();
		FTaskSystem Tasks(1, 1);
		auto Files = std::make_shared<FMemoryFileSystem>();
		FIOService IO(Tasks, Files);
		const auto Root = std::filesystem::absolute("native-assets").lexically_normal();
		CheckService(Tasks, IO, *Files, Root);
		CheckGraph(Tasks, IO, *Files, Root);
		CheckCache(Tasks, IO, *Files, Root);
		Tasks.Shutdown();
		std::cout << "Native container, typed/untyped loading, references, cycles, relocation, ordered saves and cache "
		             "passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
