#include "Hyperion/Reflection/Wire.h"
#include "Hyperion/Serialization/Archive.h"
#include "Support/TestSupport.h"
#include <limits>

namespace Hyperion
{
struct FEnvelopeFixture
{
	std::uint32_t Count{};
	std::string Name = "untitled";
};

template<> const FRecordDescriptor& RecordType<FEnvelopeFixture>()
{
	static const auto Type = []
	{
		auto Result = MakeRecord<FEnvelopeFixture>(
		    "test.envelope", {Member("count", &FEnvelopeFixture::Count), Member("name", &FEnvelopeFixture::Name)}, 2,
		    [](const FEnvelopeFixture& InValue)
		    {
			    if (!InValue.Count)
			    {
				    throw std::invalid_argument("count must be positive");
			    }
		    });
		Result.Migrations.emplace(1,
		                          [](FArchiveNode::FObject& InFields)
		                          {
			                          InFields["count"] = std::move(InFields.at("oldCount"));
			                          InFields.erase("oldCount");
		                          });
		return Result;
	}();
	return Type;
}
} // namespace Hyperion

namespace
{
using namespace Hyperion;

std::string Failure(const std::function<void()>& InWork)
{
	try
	{
		InWork();
	}
	catch (const std::exception& Error)
	{
		return Error.what();
	}
	throw std::runtime_error("Expected envelope failure");
}

void CheckRepresentationAndReferences()
{
	const auto Storage = std::make_shared<const std::vector<std::byte>>(8, std::byte{42});
	const FArchiveNode::FObject Fields{{"name", WriteValue(std::string("business name"))},
	                                   {"type", WriteValue(false)},
	                                   {"version", WriteValue(std::int64_t{-1})},
	                                   {"fields", FArchiveNode(FBulkData{EBulkElement::U8, {}, Storage, 2, 4})}};
	const FArchiveNode Expected(FArchiveNode::FObject{{"type", FArchiveNode(std::string("unknown.type"))},
	                                                  {"version", FArchiveNode(std::uint64_t{4294967295})},
	                                                  {"fields", FArchiveNode(Fields)}});
	auto Node = MakeRecordEnvelope("unknown.type", std::numeric_limits<std::uint32_t>::max(), Fields);
	HYP_CHECK(EncodeArchive(Node) == EncodeArchive(Expected));
	HYP_CHECK(HashArchive(Node) == HashArchive(Expected));
	const auto& ConstNode = Node;
	auto& Object = std::get<FArchiveNode::FObject>(Node.Value);
	HYP_CHECK(&RecordFields(Node) == &std::get<FArchiveNode::FObject>(Object.at("fields").Value));
	HYP_CHECK(&RecordFields(ConstNode) == &RecordFields(Node));
	HYP_CHECK(&RecordTypeId(ConstNode) == &std::get<std::string>(Object.at("type").Value));
	HYP_CHECK(RecordVersion(ConstNode) == 4294967295u);
	const auto& Bulk = std::get<FBulkData>(RecordFields(Node).at("fields").Value);
	HYP_CHECK(Bulk.Storage == Storage && Bulk.Offset == 2 && Bulk.Size == 4 && Bulk.Bytes.empty());
	RecordFields(Node).at("name") = WriteValue(std::string("edited"));
	HYP_CHECK(ReadValue<std::string>(RecordFields(ConstNode).at("name")) == "edited");
	HYP_CHECK(RecordTypeId(Node) == "unknown.type");
	HYP_CHECK(std::string_view(RecordFieldsKey) == "fields");
}

void CheckIndependentAccessAndShape()
{
	FArchiveNode Node(FArchiveNode::FObject{{"type", WriteValue(std::string("unknown.type"))},
	                                        {"version", WriteValue(std::int64_t{2})}});
	HYP_CHECK(RecordTypeId(Node) == "unknown.type" && RecordVersion(Node) == 2);
	const auto Before = EncodeArchive(Node);
	(void)Failure(
	    [&]
	    {
		    (void)RecordFields(Node);
	    });
	const auto& ConstNode = Node;
	(void)Failure(
	    [&]
	    {
		    (void)RecordFields(ConstNode);
	    });
	HYP_CHECK(EncodeArchive(Node) == Before && !HasRecordEnvelopeShape(Node));
	auto& Object = std::get<FArchiveNode::FObject>(Node.Value);
	Object["fields"] = WriteValue(false);
	HYP_CHECK(!HasRecordEnvelopeShape(Node));
	(void)Failure(
	    [&]
	    {
		    (void)RecordFields(Node);
	    });
	Object["fields"] = FArchiveNode(FArchiveNode::FObject{});
	for (const auto& Version : {WriteValue(std::int64_t{-1}), WriteValue(std::uint64_t{4294967296}), WriteValue(2.0),
	                            WriteValue(false), FArchiveNode(std::monostate{}), WriteValue(std::string("opaque"))})
	{
		Object["version"] = Version;
		HYP_CHECK(HasRecordEnvelopeShape(Node) && RecordFields(Node).empty());
		(void)Failure(
		    [&]
		    {
			    (void)RecordVersion(Node);
		    });
	}
	Object.erase("version");
	HYP_CHECK(!HasRecordEnvelopeShape(Node) && RecordFields(Node).empty());
	(void)Failure(
	    [&]
	    {
		    (void)RecordVersion(Node);
	    });
	Object["version"] = WriteValue(2u);
	Object["type"] = WriteValue(false);
	HYP_CHECK(!HasRecordEnvelopeShape(Node) && RecordVersion(Node) == 2);
	(void)Failure(
	    [&]
	    {
		    (void)RecordTypeId(Node);
	    });
	HYP_CHECK(!HasRecordEnvelopeShape(FArchiveNode(true)));
	HYP_CHECK(!HasRecordEnvelopeShape(FArchiveNode(FArchiveNode::FArray{})));
}

void CheckValidationAndMigration()
{
	const auto& Type = RecordType<FEnvelopeFixture>();
	FArchiveNode Node(FArchiveNode::FObject{
	    {"type", WriteValue(std::string("wrong.type"))}, {"version", WriteValue(2u)}, {"fields", WriteValue(false)}});
	const auto Error = Failure(
	    [&]
	    {
		    (void)ReadRecord(Type, Node, {"asset.child"});
	    });
	HYP_CHECK(Error == "asset.child (test.envelope): type or schema version mismatch (file 2, current 2)");
	auto& Object = std::get<FArchiveNode::FObject>(Node.Value);
	Object["version"] = WriteValue(std::int64_t{-1});
	HYP_CHECK(Failure(
	              [&]
	              {
		              (void)ReadRecord(Type, Node, {"asset.child"});
	              }) ==
	          "asset.child (test.envelope): " + Failure(
	                                                [&]
	                                                {
		                                                (void)ReadValue<std::uint32_t>(Object.at("version"));
	                                                }));
	const auto Default = RecordValueShape<FEnvelopeFixture>().DefaultValue();
	HYP_CHECK(RecordTypeId(Default) == Type.Id && RecordVersion(Default) == 2);
	HYP_CHECK(ReadValue<std::uint32_t>(RecordFields(Default).at("count")) == 0);
	FEnvelopeFixture Destination{42, "preserved"};
	HYP_CHECK(Failure(
	              [&]
	              {
		              ReadRecordFields(Type, &Destination, Default);
	              })
	              .find("count must be positive") != std::string::npos);
	HYP_CHECK(Destination.Count == 42 && Destination.Name == "preserved");
	const FArchiveNode Legacy(
	    FArchiveNode::FObject{{"type", WriteValue(Type.Id)},
	                          {"version", WriteValue(std::int64_t{1})},
	                          {"fields", FArchiveNode(FArchiveNode::FObject{{"oldCount", WriteValue(7u)}})}});
	const auto Before = EncodeArchive(Legacy);
	HYP_CHECK(ReadValue<FEnvelopeFixture>(Legacy).Count == 7);
	HYP_CHECK(EncodeArchive(Legacy) == Before && RecordVersion(Legacy) == 1);
}

void CheckRecordAndWireProducers()
{
	const FEnvelopeFixture Value{9, "named"};
	const FArchiveNode Fields(FArchiveNode::FObject{{"count", WriteValue(9u)}, {"name", WriteValue(Value.Name)}});
	const FArchiveNode Expected(FArchiveNode::FObject{
	    {"type", WriteValue(std::string("test.envelope"))}, {"version", WriteValue(2u)}, {"fields", Fields}});
	HYP_CHECK(EncodeArchive(WriteValue(Value)) == EncodeArchive(Expected));
	HYP_CHECK(HashArchive(WriteValue(Value)) == HashArchive(Expected));
	HYP_CHECK(EncodeArchive(WriteRecordWire(RecordType<FEnvelopeFixture>(), &Value)) == EncodeArchive(Fields));
	const auto Restored = ReadValueWire(RecordValueShape<FEnvelopeFixture>(), Fields);
	HYP_CHECK(EncodeArchive(Restored) == EncodeArchive(Expected));
	HYP_CHECK(ReadValue<FEnvelopeFixture>(Restored).Name == Value.Name);
}
} // namespace

void CheckRecordEnvelope()
{
	CheckRepresentationAndReferences();
	CheckIndependentAccessAndShape();
	CheckValidationAndMigration();
	CheckRecordAndWireProducers();
}
