#include "Hyperion/Serialization/Archive.h"
#include "Support/TestSupport.h"
#include <array>

namespace Hyperion
{
enum class EArchiveMode
{
	First = 3,
	Second = 8
};

struct FSchemaChild
{
	std::string Name = "default";
	std::uint32_t Count = 7;
};

struct FSchemaObject
{
	std::vector<FSchemaChild> Children;
	std::optional<FSchemaChild> Optional;
	std::map<std::string, FSchemaChild> Named;
	std::uint64_t Large = std::numeric_limits<std::uint64_t>::max();
	std::vector<bool> Flags{true, false};
	EArchiveMode Mode = EArchiveMode::First;
	std::string Runtime = "transient";
};

template<> std::span<const EArchiveMode> RecordEnumValues<EArchiveMode>()
{
	static constexpr std::array Values{EArchiveMode::First, EArchiveMode::Second};
	return Values;
}

template<> const FRecordDescriptor& RecordType<FSchemaChild>()
{
	static const auto Type = []
	{
		auto Result = MakeRecord<FSchemaChild>(
		    "test.schema.child",
		    {Member("name", &FSchemaChild::Name, {true, true, {"label"}}), Member("count", &FSchemaChild::Count)}, 2);
		Result.Migrations.emplace(1,
		                          [](FArchiveNode::FObject& InFields)
		                          {
			                          const auto It = InFields.find("oldName");
			                          if (It != InFields.end())
			                          {
				                          if (InFields.contains("name"))
				                          {
					                          throw std::runtime_error("Conflicting old name");
				                          }
				                          InFields.emplace("name", std::move(It->second));
				                          InFields.erase(It);
			                          }
		                          });
		return Result;
	}();
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSchemaObject>()
{
	static const auto Type = MakeRecord<FSchemaObject>(
	    "test.schema.object",
	    {Member("children", &FSchemaObject::Children), Member("optional", &FSchemaObject::Optional),
	     Member("named", &FSchemaObject::Named), Member("large", &FSchemaObject::Large),
	     Member("flags", &FSchemaObject::Flags), Member("mode", &FSchemaObject::Mode),
	     Member("runtime", &FSchemaObject::Runtime, {false, false})});
	return Type;
}
} // namespace Hyperion

namespace
{
using namespace Hyperion;

FArchiveNode::FObject& Fields(FArchiveNode& InNode)
{
	return std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(InNode.Value).at("fields").Value);
}

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
	}
	HYP_CHECK(bFailed);
}

void CheckMigration()
{
	auto Node = WriteValue(FSchemaChild{"before", 42});
	std::get<FArchiveNode::FObject>(Node.Value)["version"] = WriteValue(1u);
	auto& Values = Fields(Node);
	Values.emplace("oldName", std::move(Values.at("name")));
	Values.erase("name");
	Values.erase("count");
	Values.emplace("unknown", FArchiveNode(true));
	std::vector<std::string> Diagnostics;
	const auto Restored = ReadValue<FSchemaChild>(Node, {"asset.Children[2]", &Diagnostics});
	HYP_CHECK(Restored.Name == "before" && Restored.Count == 7);
	HYP_CHECK(Diagnostics.size() == 2 && Diagnostics[1].find("Children[2].unknown") != std::string::npos);
	HYP_CHECK(ReadValue<unsigned>(std::get<FArchiveNode::FObject>(WriteValue(Restored).Value).at("version")) == 2);
	auto Missing = RecordType<FSchemaChild>();
	Missing.Migrations.clear();
	Reject(
	    [&]
	    {
		    ReadRecord(Missing, Node);
	    },
	    "missing schema migration");
	std::get<FArchiveNode::FObject>(Node.Value)["version"] = WriteValue(3u);
	Reject(
	    [&]
	    {
		    ReadValue<FSchemaChild>(Node);
	    },
	    "schema version");
}

void CheckMismatch()
{
	auto Node = WriteValue(FSchemaChild{"kept", 42});
	auto& Values = Fields(Node);
	Values.emplace("label", FArchiveNode(std::string("duplicate")));
	FSchemaChild Destination{"original", 100};
	Reject(
	    [&]
	    {
		    ReadRecordFields(RecordType<FSchemaChild>(), &Destination, Node);
	    },
	    "conflicting field alias");
	HYP_CHECK(Destination.Name == "original" && Destination.Count == 100);
	Values.erase("name");
	HYP_CHECK(ReadValue<FSchemaChild>(Node).Name == "duplicate");
	Values.erase("label");
	Reject(
	    [&]
	    {
		    ReadValue<FSchemaChild>(Node);
	    },
	    "missing required field");
	Values.emplace("name", FArchiveNode(std::string("name")));
	Values["count"] = WriteValue(std::string("wrong"));
	Reject(
	    [&]
	    {
		    ReadValue<FSchemaChild>(Node, {"Scene.Children[3]"});
	    },
	    "Scene.Children[3].count");
	Reject(
	    []
	    {
		    ReadValue<unsigned>(WriteValue(std::int64_t{-1}));
	    },
	    "out of range");
	Reject(
	    []
	    {
		    ReadValue<std::int64_t>(WriteValue(std::numeric_limits<std::uint64_t>::max()));
	    },
	    "out of range");
	Reject(
	    []
	    {
		    ReadValue<EArchiveMode>(WriteValue(4));
	    },
	    "enum");
	Reject(
	    []
	    {
		    WriteValue(static_cast<EArchiveMode>(4));
	    },
	    "enum");
}

void CheckRegistry()
{
	FRecordRegistry Registry;
	Registry.Register<FSchemaChild>();
	Registry.Register<FSchemaChild>();
	HYP_CHECK(Registry.Find("test.schema.child")->CppType == typeid(FSchemaChild));
	auto Conflict = RecordType<FSchemaObject>();
	Conflict.Id = "test.schema.child";
	Reject(
	    [&]
	    {
		    Registry.Register(Conflict);
	    },
	    "Conflicting");
	auto ValidatorConflict = RecordType<FSchemaChild>();
	ValidatorConflict.Validate = [](const void*)
	{
	};
	Reject(
	    [&]
	    {
		    Registry.Register(ValidatorConflict);
	    },
	    "Conflicting");
	auto Independent = MakeRecord<FSchemaChild>("test.schema.child", RecordType<FSchemaChild>().Members, 2);
	Independent.Migrations = RecordType<FSchemaChild>().Migrations;
	Reject(
	    [&]
	    {
		    Registry.Register(Independent);
	    },
	    "Conflicting");
	auto Duplicate = RecordType<FSchemaChild>();
	Duplicate.Members.push_back(Duplicate.Members.front());
	Reject(
	    [&]
	    {
		    ValidateRecordDescriptor(Duplicate);
	    },
	    "duplicate");
	Reject(
	    [&]
	    {
		    Registry.Find("absent");
	    },
	    "Unregistered");
}

void CheckBindingIdentity()
{
	struct FBindingFixture
	{
		int First{};
		int Second{};
	};

	auto Type = MakeRecord<FBindingFixture>("test.binding", {Member("value", &FBindingFixture::First)}, 4);
	const auto Migration = [](int InValue)
	{
		return [InValue](FArchiveNode::FObject& InFields)
		{
			InFields["value"] = WriteValue(InValue);
		};
	};
	Type.Migrations.emplace(1, Migration(10));
	Type.Migrations.emplace(2, Migration(20));
	FRecordRegistry Registry;
	Registry.Register(Type);
	const auto Copy = Type;
	Registry.Register(Copy);
	const FBindingFixture Empty;
	HYP_CHECK(ReadRecord(*Registry.Find(Type.Id), WriteRecord(Copy, &Empty)) != nullptr);
	const auto Other = Member("value", &FBindingFixture::Second);
	for (unsigned Case = 0; Case < 6; ++Case)
	{
		auto Conflict = Type;
		switch (Case)
		{
			case 0:
				Conflict.Members[0] = Other;
				break;
			case 1:
				Conflict.Members[0].Read = Other.Read;
				break;
			case 2:
				Conflict.Members[0].Write = Other.Write;
				break;
			case 3:
				Conflict.Members[0].Visit = Other.Visit;
				break;
			case 4:
				Conflict.Migrations[1] = Migration(99);
				break;
			case 5:
				Conflict.Migrations.erase(1);
				Conflict.Migrations.emplace(3, Migration(30));
				break;
		}
		Reject(
		    [&]
		    {
			    Registry.Register(Conflict);
		    },
		    "Conflicting");
	}
	const FBindingFixture Original{10, 20};
	const auto Restored =
	    std::static_pointer_cast<FBindingFixture>(ReadRecord(*Registry.Find(Type.Id), WriteRecord(Type, &Original)));
	HYP_CHECK(Restored->First == 10 && Restored->Second == 0);
}

void CheckWire()
{
	const auto Bytes = Serialize(std::uint64_t{0x0807060504030201});
	HYP_CHECK(Bytes.size() == 33 && Bytes[4] == std::byte{2} && Bytes[24] == std::byte{8});
	for (unsigned Index = 0; Index < 8; ++Index)
	{
		HYP_CHECK(Bytes[25 + Index] == std::byte(Index + 1));
	}
	const std::array Legacy{std::byte{'H'}, std::byte{'Y'}, std::byte{'P'}, std::byte{'A'}, std::byte{1},
	                        std::byte{0},   std::byte{0},   std::byte{0},   std::byte{0},   std::byte{1}};
	HYP_CHECK(ReadValue<bool>(DecodeArchive(Legacy)));
	auto Bulk = Serialize(std::vector<std::uint32_t>{1, 2});
	HYP_CHECK(Bulk.size() == 60 && Bulk[24] == std::byte{52});
	auto Owner = std::make_shared<const std::vector<std::byte>>(Bulk);
	auto Node = DecodeArchive(Owner);
	HYP_CHECK(std::get<FBulkData>(Node.Value).Storage == Owner);
	Owner.reset();
	HYP_CHECK(ReadValue<std::vector<std::uint32_t>>(Node) == std::vector<std::uint32_t>({1, 2}));
	FArchiveLimits Limits;
	Limits.MaxAllocatedBytes = 1;
	Reject(
	    [&]
	    {
		    DecodeArchive(Bulk, Limits);
	    },
	    "allocation budget");
	Bulk[24] = std::byte{0};
	Reject(
	    [&]
	    {
		    DecodeArchive(Bulk);
	    },
	    "bulk range");
	auto Invalid = Bytes;
	Invalid[24] = std::byte{255};
	Reject(
	    [&]
	    {
		    DecodeArchive(Invalid);
	    },
	    "tag");
	Invalid = Bytes;
	Invalid.push_back(std::byte{0});
	Reject(
	    [&]
	    {
		    DecodeArchive(Invalid);
	    },
	    "Trailing");
}
} // namespace

void CheckRecordEvolution()
{
	FSchemaObject Original;
	Original.Children.push_back({"nested", 11});
	Original.Optional = FSchemaChild{"optional", 12};
	Original.Named.emplace("key", FSchemaChild{"mapped", 13});
	Original.Runtime = "not persisted";
	const auto Restored = Deserialize<FSchemaObject>(Serialize(Original));
	HYP_CHECK(Restored.Children[0].Name == "nested" && Restored.Optional->Count == 12);
	HYP_CHECK(Restored.Named.at("key").Count == 13 && Restored.Large == Original.Large);
	HYP_CHECK(Restored.Flags == Original.Flags && Restored.Runtime == "transient");
	Original.Optional.reset();
	HYP_CHECK(!Deserialize<FSchemaObject>(Serialize(Original)).Optional);
	std::vector<std::string> Paths;
	VisitValue(
	    Restored,
	    [&](const FRecordDescriptor& InType, const void*, std::string_view InPath)
	    {
		    if (InType.CppType == typeid(FSchemaChild))
		    {
			    Paths.emplace_back(InPath);
		    }
	    },
	    "Root");
	HYP_CHECK(Paths == std::vector<std::string>({"Root.children[0]", "Root.optional", "Root.named[key]"}));
	CheckMigration();
	CheckMismatch();
	CheckRegistry();
	CheckBindingIdentity();
	CheckWire();
}
