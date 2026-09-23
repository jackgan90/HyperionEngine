#include "ConnectionInternal.h"

namespace Hyperion
{
FAutomationRouter::FAutomationRouter(IAutomationEndpoint& InLocal, FConnectionManager& InConnections)
    : Local(InLocal), Connections(InConnections)
{
}

void FAutomationRouter::SetDefaultConnection(std::string InConnection)
{
	DefaultConnection = std::move(InConnection);
}

FEndpointRequest FAutomationRouter::Begin(std::string_view InMethod, const FArchiveNode& InParameters)
{
	try
	{
		auto Fields = ConnectionFields(InParameters);
		if (InMethod == "targets.list")
		{
			CheckConnectionKeys(Fields, {});
			return ReadyAutomationRequest(Connections.List());
		}
		if (InMethod == "targets.connect")
		{
			return Connections.Connect(InParameters);
		}
		if (InMethod == "targets.disconnect")
		{
			CheckConnectionKeys(Fields, {"connection"});
			return ReadyAutomationRequest(Connections.Disconnect(ReadValue<std::string>(Fields.at("connection"))));
		}
		const auto Found = Fields.find("connection");
		const auto Connection = Found == Fields.end() ? DefaultConnection : ReadValue<std::string>(Found->second);
		if (Found != Fields.end() && Connection.empty())
		{
			throw FAutomationError("invalid_arguments", "Explicit connection cannot be empty");
		}
		Fields.erase("connection");
		const FArchiveNode Parameters(std::move(Fields));
		return Connection.empty() ? Local.Begin(InMethod, Parameters)
		                          : Connections.Request(Connection, InMethod, Parameters);
	}
	catch (...)
	{
		return ReadyAutomationRequest(CurrentConnectionFailure());
	}
}

FArchiveNode FAutomationRouter::Tools() const
{
	auto Result = Local.Tools();
	auto& Tools = std::get<FArchiveNode::FArray>(std::get<FArchiveNode::FObject>(Result.Value).at("tools").Value);
	for (auto& Tool : Tools)
	{
		auto& Schema =
		    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Tool.Value).at("inputSchema").Value);
		if (!Schema.contains("properties"))
		{
			Schema.emplace("properties", FArchiveNode(FArchiveNode::FObject{}));
		}
		std::get<FArchiveNode::FObject>(Schema.at("properties").Value)
		    .emplace(
		        "connection",
		        ParseJson(
		            R"({"type":"string","description":"Connection ID from targets.connect. Omit for the configured default target or standalone session."})"));
	}
	const auto Add = [&](const char* InName, const char* InDescription, FArchiveNode InSchema, bool bInReadOnly)
	{
		Tools.emplace_back(FArchiveNode::FObject{
		    {"name", WriteValue(std::string(InName))},
		    {"description", WriteValue(std::string(InDescription))},
		    {"inputSchema", std::move(InSchema)},
		    {"annotations", FArchiveNode(FArchiveNode::FObject{{"readOnlyHint", WriteValue(bInReadOnly)},
		                                                       {"openWorldHint", WriteValue(false)}})}});
	};
	Add("targets.list", "Discover candidate engine instances. Entries may be stale; connecting verifies identity.",
	    ParseJson(R"({"type":"object","additionalProperties":false})"), true);
	auto ConnectSchema = ParseJson(
	    R"({"type":"object","properties":{"instance":{"type":"string","description":"Boot-specific target identity; pins the handshake when supplied."}},"additionalProperties":false,"anyOf":[{"required":["instance"]},{"required":["address"]}]})");
	std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(ConnectSchema.Value).at("properties").Value)
	    .emplace("address", RecordWireSchema(RecordType<FTransportAddress>()));
	Add("targets.connect",
	    "Connect by discovered instance or explicit transport address. Returns a connection ID; does not change the "
	    "default target.",
	    std::move(ConnectSchema), false);
	Add("targets.disconnect",
	    "Disconnect without closing the application or discarding its documents. Admitted work can still complete.",
	    ParseJson(
	        R"({"type":"object","properties":{"connection":{"type":"string"}},"required":["connection"],"additionalProperties":false})"),
	    false);
	return Result;
}
} // namespace Hyperion
