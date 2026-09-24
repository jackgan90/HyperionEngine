#include "Hyperion/Automation/Endpoint.h"
#include "Support/TestSupport.h"
#include <array>
#include <iostream>
#include <thread>

namespace Hyperion
{
enum class ETestMode : std::uint8_t
{
	First = 1,
	Second = 3
};

template<> std::span<const TRecordEnumEntry<ETestMode>> RecordEnumEntries<ETestMode>()
{
	static constexpr TRecordEnumEntry<ETestMode> Values[] = {{ETestMode::First, "First", "First mode"},
	                                                         {ETestMode::Second, "Second", "Second mode"}};
	return Values;
}

struct FWireChild
{
	std::int32_t Count{};
};

enum class EPartialMode : std::uint64_t
{
	First = 1,
	Alias = First,
	Unlabelled = UINT64_MAX
};

template<> std::span<const EPartialMode> RecordEnumValues<EPartialMode>()
{
	static constexpr EPartialMode Values[] = {EPartialMode::First, EPartialMode::Alias, EPartialMode::Unlabelled};
	return Values;
}

template<> std::span<const TRecordEnumEntry<EPartialMode>> RecordEnumEntries<EPartialMode>()
{
	static constexpr TRecordEnumEntry<EPartialMode> Entries[] = {{EPartialMode::First, "First", "Primary name"},
	                                                             {EPartialMode::Alias, "Alias", "Alternate name"}};
	return Entries;
}

struct FPartialEnum
{
	EPartialMode Mode = EPartialMode::Unlabelled;
};

struct FWireRequest
{
	std::string Name;
	std::uint64_t Revision{};
	std::array<float, 3> Position{};
	std::optional<FWireChild> Child;
	std::map<std::string, FWireChild> Named;
	std::vector<ETestMode> Modes{ETestMode::First};
	bool bEnabled = true;
};

struct FWireResult
{
	std::string Name;
};

struct FRecursiveWire
{
	std::vector<FRecursiveWire> Children;
};

struct FRecursiveEnvelope
{
	FRecursiveWire Value;
};

template<> const FRecordDescriptor& RecordType<FRecursiveWire>()
{
	static const auto Type =
	    MakeRecord<FRecursiveWire>("test.wire.recursive", {Member("children", &FRecursiveWire::Children)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FRecursiveEnvelope>()
{
	static const auto Type =
	    MakeRecord<FRecursiveEnvelope>("test.wire.recursive-envelope", {Member("value/~", &FRecursiveEnvelope::Value)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FWireChild>()
{
	static const auto Type = MakeRecord<FWireChild>("test.wire.child", {Member("count", &FWireChild::Count)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FWireRequest>()
{
	static const auto Type = MakeRecord<FWireRequest>(
	    "test.wire.request", {Member("name", &FWireRequest::Name, {.bRequired = true, .Description = "Name to echo."}),
	                          Member("revision", &FWireRequest::Revision), Member("position", &FWireRequest::Position),
	                          Member("child", &FWireRequest::Child), Member("named", &FWireRequest::Named),
	                          Member("modes", &FWireRequest::Modes),
	                          Member("enabled", &FWireRequest::bEnabled,
	                                 {.Description = "Newly registered field appears without transport edits."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FWireResult>()
{
	static const auto Type = MakeRecord<FWireResult>("test.wire.result", {Member("name", &FWireResult::Name)});
	return Type;
}
} // namespace Hyperion

namespace
{
using namespace Hyperion;

const FArchiveNode& Field(const FArchiveNode& InValue, const std::string& InName)
{
	return std::get<FArchiveNode::FObject>(InValue.Value).at(InName);
}

std::string Text(const FArchiveNode& InValue, const std::string& InName)
{
	return ReadValue<std::string>(Field(InValue, InName));
}

template<class T> void Reject(T InWork)
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

FOperationInfo Info(std::string InId)
{
	return {std::move(InId),
	        "Echo name",
	        "Exercise typed registration.",
	        "test",
	        "No document writes.",
	        "Completes with the accepted name.",
	        ParseJson(R"({"name":"example"})")};
}

void CheckRecursiveSchema()
{
	FOperationCatalog Catalog;
	Catalog.RegisterType(RecordType<FRecursiveEnvelope>());
	HYP_CHECK(Text(Catalog.DescribeType("test.wire.recursive"), "x-hyperion-type") == "test.wire.recursive");
	const auto Schema = RecordWireSchema(RecordType<FRecursiveEnvelope>());
	const auto& Recursive = Field(Field(Schema, "properties"), "value/~");
	const auto& Children = Field(Field(Recursive, "properties"), "children");
	HYP_CHECK(Text(Field(Children, "items"), "$ref") == "#/properties/value~1~0");
	FRecursiveEnvelope Value;
	Value.Value.Children.resize(1);
	const auto Wire = WriteRecordWire(RecordType<FRecursiveEnvelope>(), &Value);
	const auto Decoded =
	    std::static_pointer_cast<FRecursiveEnvelope>(ReadRecordWire(RecordType<FRecursiveEnvelope>(), Wire));
	HYP_CHECK(Decoded->Value.Children.size() == 1);
	auto* Tail = &Value.Value;
	for (unsigned Depth = 0; Depth < 40; ++Depth)
	{
		Tail->Children.resize(1);
		Tail = &Tail->Children.front();
	}
	Reject(
	    [&]
	    {
		    WriteRecordWire(RecordType<FRecursiveEnvelope>(), &Value);
	    });
}

void CheckWire()
{
	const auto Input = ParseJson(
	    R"({"name":"test","revision":"18446744073709551615","position":[1,2,3],"child":{"count":9},"named":{"x":{"count":-3}},"modes":[1,3],"enabled":false})");
	const auto Value = std::static_pointer_cast<FWireRequest>(ReadRecordWire(RecordType<FWireRequest>(), Input));
	HYP_CHECK(Value->Revision == UINT64_MAX && Value->Position[2] == 3 && Value->Child->Count == 9 && !Value->bEnabled);
	const auto Encoded = WriteRecordWire(RecordType<FWireRequest>(), Value.get());
	const auto Restored = ReadRecordWire(RecordType<FWireRequest>(), Encoded);
	HYP_CHECK(WriteJson(WriteRecordWire(RecordType<FWireRequest>(), Restored.get())) == WriteJson(Encoded));
	const auto Schema = RecordWireSchema(RecordType<FWireRequest>());
	const auto& ModeSchema = Field(Field(Field(Schema, "properties"), "modes"), "items");
	HYP_CHECK(Text(ModeSchema, "description").find("3=Second") != std::string::npos);
	HYP_CHECK(std::get<FArchiveNode::FArray>(Field(ModeSchema, "oneOf").Value).size() == 2);
	HYP_CHECK(Text(Field(Field(Schema, "properties"), "revision"), "type") == "string");
	HYP_CHECK(ReadInteger<int>(Field(Field(Field(Schema, "properties"), "position"), "minItems")) == 3);
	HYP_CHECK(Text(Field(Field(Schema, "properties"), "enabled"), "description").find("without transport") !=
	          std::string::npos);
	for (const auto* Bad : {R"({})", R"({"name":"x","typo":1})", R"({"name":"x","position":[1,2]})",
	                        R"({"name":"x","revision":18446744073709551615})", R"({"name":"x","revision":"01"})",
	                        R"({"name":"x","revision":"18446744073709551616"})", R"({"name":"x","modes":[2]})",
	                        R"({"name":"x","child":{"count":2147483648}})", R"({"name":"x","child":{"typo":1}})"})
	{
		Reject(
		    [&]
		    {
			    (void)ReadRecordWire(RecordType<FWireRequest>(), ParseJson(Bad));
		    });
	}
	Reject(
	    []
	    {
		    ParseJson(R"({"x":1,"x":2})");
	    });
	Reject(
	    []
	    {
		    ParseJson("[[[[0]]]]", {100, 100, 2});
	    });
	Reject(
	    []
	    {
		    ParseJson("123456", {4, 100, 2});
	    });
	Reject(
	    []
	    {
		    WriteJson(FArchiveNode(std::numeric_limits<double>::infinity()));
	    });
	const auto Integral =
	    ReadRecordWire(RecordType<FWireRequest>(), ParseJson(R"({"name":"x","child":{"count":1.0},"modes":[1.0]})"));
	HYP_CHECK(std::static_pointer_cast<FWireRequest>(Integral)->Child->Count == 1);
}

void CheckPartialEnumMetadata()
{
	const auto Type = MakeRecord<FPartialEnum>(
	    "test.partial.enum", {Member("mode", &FPartialEnum::Mode, {.Description = "Partial enum metadata."})});
	const auto Schema = RecordWireSchema(Type);
	const auto& Property = Field(Field(Schema, "properties"), "mode");
	const auto& Allowed = std::get<FArchiveNode::FArray>(Field(Property, "enum").Value);
	const auto& Branches = std::get<FArchiveNode::FArray>(Field(Property, "oneOf").Value);
	HYP_CHECK(Allowed.size() == 2 && Branches.size() == 2);
	HYP_CHECK(Text(Branches.front(), "title") == "First / Alias");
	HYP_CHECK(Text(Property, "description").find("Partial enum metadata.") == 0);
	HYP_CHECK(Text(Property, "description").find("Alternate name") != std::string::npos);
	for (const auto& Value : Allowed)
	{
		std::size_t Matches{};
		for (const auto& Branch : Branches)
		{
			Matches += WriteJson(Field(Branch, "const")) == WriteJson(Value) ? 1u : 0u;
		}
		HYP_CHECK(Matches == 1);
		const auto Decoded = ReadRecordWire(Type, FArchiveNode(FArchiveNode::FObject{{"mode", Value}}));
		HYP_CHECK(WriteJson(Field(WriteRecordWire(Type, Decoded.get()), "mode")) == WriteJson(Value));
	}
	HYP_CHECK(Text(Property, "default") == "18446744073709551615");
	HYP_CHECK(Text(Branches.back(), "const") == Text(Property, "default"));
	Reject(
	    [&]
	    {
		    ReadRecordWire(Type, ParseJson(R"({"mode":"2"})"));
	    });
}

void CheckCatalogAndJobs()
{
	FOperationCatalog Catalog;
	int Invoked{};
	Catalog.Register(MakeOperation<FWireRequest, FWireResult>(Info("test.echo"),
	                                                          [&](const FWireRequest& InRequest)
	                                                          {
		                                                          ++Invoked;
		                                                          return FWireResult{InRequest.Name};
	                                                          }));
	bool bReady{};
	HYP_CHECK(Text(Catalog.DescribeType("test.wire.child"), "x-hyperion-type") == "test.wire.child");
	HYP_CHECK(ReadInteger<unsigned>(Field(Catalog.Search("typed registration", 0, 50), "total")) == 1);
	bool bCancelled{};
	Catalog.Register(MakeAsyncOperation<FWireRequest, FWireResult>(
	    Info("test.deferred"),
	    [&](const FWireRequest& InRequest)
	    {
		    return TPendingOperation<FWireResult>{[&, Name = InRequest.Name]() -> std::optional<FWireResult>
		                                          {
			                                          return bReady ? std::optional(FWireResult{Name}) : std::nullopt;
		                                          },
		                                          [&]
		                                          {
			                                          bCancelled = true;
		                                          }};
	    }));
	auto Missing = Info("test.missing");
	Missing.Unavailable = "Provider disabled";
	Catalog.Register(MakeOperation<FWireRequest, FWireResult>(Missing,
	                                                          [](const FWireRequest&) -> FWireResult
	                                                          {
		                                                          throw std::logic_error("must not invoke");
	                                                          }));
	Reject(
	    [&]
	    {
		    Catalog.Register(MakeOperation<FWireRequest, FWireResult>(Info("test.echo"),
		                                                              [](const FWireRequest&)
		                                                              {
			                                                              return FWireResult{};
		                                                              }));
	    });
	Catalog.Seal();
	Reject(
	    [&]
	    {
		    Catalog.RegisterType(RecordType<FWireRequest>());
	    });
	bool bWrongThread{};
	std::thread Other(
	    [&]
	    {
		    try
		    {
			    Catalog.Search("");
		    }
		    catch (const std::logic_error&)
		    {
			    bWrongThread = true;
		    }
	    });
	Other.join();
	HYP_CHECK(bWrongThread);
	FAutomationSession Session(Catalog, {1, 2});
	FAutomationEndpoint Endpoint(Session);
	const auto Args = ParseJson(R"({"name":"one","enabled":false})");
	HYP_CHECK(Text(Session.Call("test.echo", Args), "status") == "completed" && Invoked == 1);
	HYP_CHECK(Text(Session.Call("test.echo", ParseJson(R"({"name":"one","unknown":0})")), "status") == "failed" &&
	          Invoked == 1);
	HYP_CHECK(Text(Field(Session.Call("test.missing", Args), "error"), "code") == "unavailable");
	const auto Started = Session.Call("test.deferred", Args);
	const auto Job = Text(Started, "job");
	HYP_CHECK(Text(Field(Session.Call("test.deferred", Args), "error"), "code") == "busy");
	HYP_CHECK(Text(Session.GetJob(Job), "status") == "running");
	bReady = true;
	HYP_CHECK(Text(Field(Field(Session.GetJob(Job), "outcome"), "result"), "name") == "one");
	bReady = false;
	const auto Cancel = Text(Session.Call("test.deferred", Args), "job");
	HYP_CHECK(Text(Session.CancelJob(Cancel), "status") == "cancelled" && bCancelled);
	HYP_CHECK(Text(Endpoint.Execute("types.describe", ParseJson(R"({"type":"absent"})")), "status") == "failed");
	HYP_CHECK(std::get<FArchiveNode::FArray>(Field(Catalog.Search("echo", 0, 1), "items").Value).size() == 1);
	Session.StopAdmission();
	HYP_CHECK(Text(Field(Session.Call("test.echo", Args), "error"), "code") == "session_closed");
}

void CheckResultLimits()
{
	FOperationCatalog Catalog;
	int Invoked{};
	const FWireResult Oversized{std::string(FJsonLimits{}.MaxBytes, 'x')};
	Catalog.Register(MakeOperation<FWireRequest, FWireResult>(Info("test.large"),
	                                                          [&](const FWireRequest&)
	                                                          {
		                                                          ++Invoked;
		                                                          return Oversized;
	                                                          }));
	Catalog.Register(MakeAsyncOperation<FWireRequest, FWireResult>(Info("test.large_async"),
	                                                               [&](const FWireRequest&)
	                                                               {
		                                                               return TPendingOperation<FWireResult>{
		                                                                   [&]() -> std::optional<FWireResult>
		                                                                   {
			                                                                   ++Invoked;
			                                                                   return Oversized;
		                                                                   }};
	                                                               }));
	Catalog.Seal();
	FAutomationSession Session(Catalog);
	const auto Args = ParseJson(R"({"name":"accepted"})");
	const auto Result = Session.Call("test.large", Args);
	HYP_CHECK(Text(Field(Result, "error"), "code") == "result_unavailable" && Invoked == 1);
	const auto Job = Text(Session.Call("test.large_async", Args), "job");
	const auto Outcome = Field(Session.GetJob(Job), "outcome");
	HYP_CHECK(Text(Field(Outcome, "error"), "code") == "result_unavailable" && Invoked == 2);
}

void CheckMcp()
{
	FOperationCatalog Catalog;
	Catalog.Register(MakeOperation<FWireRequest, FWireResult>(Info("test.echo"),
	                                                          [](const FWireRequest& InRequest)
	                                                          {
		                                                          return FWireResult{InRequest.Name};
	                                                          }));
	Catalog.Seal();
	FAutomationSession Session(Catalog);
	FAutomationEndpoint Endpoint(Session);
	FMcpConnection Mcp(Endpoint);
	HYP_CHECK(ReadInteger<int>(Field(Field(ParseJson(*Mcp.Receive("{")), "error"), "code")) == -32700);
	HYP_CHECK(ReadInteger<int>(Field(Field(ParseJson(*Mcp.Receive(R"({"jsonrpc":2,"method":"ping","id":1})")), "error"),
	                                 "code")) == -32600);
	HYP_CHECK(Mcp.Receive(
	    R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"test","version":"1"}}})"));
	HYP_CHECK(!Mcp.Receive(R"({"jsonrpc":"2.0","method":"notifications/initialized"})"));
	const auto Before = *Mcp.Receive(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})");
	const auto Result = ParseJson(*Mcp.Receive(
	    R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"api.call","arguments":{"operation":"test.echo","arguments":{"name":"test"}}}})"));
	HYP_CHECK(Text(Field(Field(Field(Result, "result"), "structuredContent"), "result"), "name") == "test");
	HYP_CHECK(*Mcp.Receive(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})") == Before);
	HYP_CHECK(!Mcp.Receive(R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"api.call"}})"));
}
} // namespace

int main()
{
	try
	{
		CheckWire();
		CheckPartialEnumMetadata();
		CheckRecursiveSchema();
		CheckCatalogAndJobs();
		CheckResultLimits();
		CheckMcp();
		std::cout << "Automation contracts passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
