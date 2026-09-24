#include "Hyperion/Automation/Endpoint.h"
#include <utility>

namespace Hyperion
{
namespace
{
struct FBootstrap
{
	const char* Name;
	const char* Description;
	const char* Schema;
	bool bReadOnly;
};

const FBootstrap Bootstrap[] = {
    {"engine.info", "Inspect this session and its execution/retention contract.",
     R"({"type":"object","additionalProperties":false})", true},
    {"api.search",
     "Search IDs, summaries, descriptions and keywords (all query words must match). Query at most 256 bytes; offset "
     ">= 0; limit 1-50, default 12. Use api.describe before calling an unfamiliar operation.",
     R"({"type":"object","properties":{"query":{"type":"string","default":""},"offset":{"type":"integer","minimum":0,"default":0},"limit":{"type":"integer","minimum":1,"maximum":50,"default":12}},"additionalProperties":false})",
     true},
    {"api.describe", "Get full input/output schemas, example, effects and completion semantics for an operation.",
     R"({"type":"object","properties":{"operation":{"type":"string"}},"required":["operation"],"additionalProperties":false})",
     true},
    {"types.describe", "Get a reflected type schema by stable type ID, without loading an asset.",
     R"({"type":"object","properties":{"type":{"type":"string"}},"required":["type"],"additionalProperties":false})",
     true},
    {"api.call",
     "Invoke a described operation. A running result contains a job ID for jobs.get. Mutations must not be "
     "automatically retried.",
     R"({"type":"object","properties":{"operation":{"type":"string"},"arguments":{"type":"object"}},"required":["operation","arguments"],"additionalProperties":false})",
     false},
    {"jobs.get", "Poll a session-owned job. Completed/failed outcomes describe the actual operation result.",
     R"({"type":"object","properties":{"job":{"type":"string"}},"required":["job"],"additionalProperties":false})",
     true},
    {"jobs.cancel",
     "Cancel a cancellable job. Non-cancellable operations return an error; cancellation is not rollback.",
     R"({"type":"object","properties":{"job":{"type":"string"}},"required":["job"],"additionalProperties":false})",
     false}};

const FArchiveNode::FObject& CheckParameters(std::string_view InMethod, const FArchiveNode& InParameters)
{
	const auto It = std::find_if(std::begin(Bootstrap), std::end(Bootstrap),
	                             [&](const FBootstrap& InItem)
	                             {
		                             return InMethod == InItem.Name;
	                             });
	if (It == std::end(Bootstrap))
	{
		throw FAutomationError("not_found", "Unknown bootstrap method: " + std::string(InMethod));
	}
	const auto* Fields = std::get_if<FArchiveNode::FObject>(&InParameters.Value);
	if (!Fields)
	{
		throw FAutomationError("invalid_arguments", "Parameters must be an object");
	}
	const auto Schema = ParseJson(It->Schema);
	const auto& Definition = std::get<FArchiveNode::FObject>(Schema.Value);
	const auto Properties = Definition.contains("properties")
	                            ? std::get<FArchiveNode::FObject>(Definition.at("properties").Value)
	                            : FArchiveNode::FObject{};
	for (const auto& [Key, Value] : *Fields)
	{
		if (!Properties.contains(Key))
		{
			throw FAutomationError("invalid_arguments", "Unknown parameter", Key);
		}
		const auto& Property = std::get<FArchiveNode::FObject>(Properties.at(Key).Value);
		const auto Type = ReadValue<std::string>(Property.at("type"));
		const bool bValid = Type == "string"   ? std::holds_alternative<std::string>(Value.Value)
		                    : Type == "object" ? std::holds_alternative<FArchiveNode::FObject>(Value.Value)
		                                       : std::holds_alternative<std::uint64_t>(Value.Value) ||
		                                             std::holds_alternative<std::int64_t>(Value.Value);
		if (!bValid)
		{
			throw FAutomationError("invalid_arguments", "Incorrect parameter type", Key);
		}
	}
	if (Definition.contains("required"))
	{
		for (const auto& Value : std::get<FArchiveNode::FArray>(Definition.at("required").Value))
		{
			const auto Key = ReadValue<std::string>(Value);
			if (!Fields->contains(Key))
			{
				throw FAutomationError("invalid_arguments", "Missing required parameter", Key);
			}
		}
	}
	return *Fields;
}
} // namespace

std::string WriteAutomationResponse(const FArchiveNode& InValue)
{
	return WriteJson(InValue, AutomationResponseLimits);
}

FAutomationEndpoint::FAutomationEndpoint(FAutomationSession& InSession) : Session(InSession)
{
}

FArchiveNode FAutomationEndpoint::Execute(std::string_view InMethod, const FArchiveNode& InParameters)
{
	try
	{
		const auto& Values = CheckParameters(InMethod, InParameters);
		const auto& Catalog = Session.GetCatalog();
		if (InMethod == "engine.info")
		{
			return FArchiveNode(FArchiveNode::FObject{
			    {"session", WriteValue(Session.GetId())},
			    {"apiVersion", WriteValue(std::uint32_t(1))},
			    {"operations", WriteValue(Catalog.Size())},
			    {"contract",
			     WriteValue(std::string("Main-owned session; wide integers are decimal strings; describe before call; "
			                            "save is explicit; jobs expire by bounded retention; no automatic retry."))}});
		}
		if (InMethod == "api.search")
		{
			return Catalog.Search(Values.contains("query") ? ReadValue<std::string>(Values.at("query")) : "",
			                      Values.contains("offset") ? ReadInteger<std::uint32_t>(Values.at("offset")) : 0,
			                      Values.contains("limit") ? ReadInteger<std::uint32_t>(Values.at("limit")) : 12);
		}
		if (InMethod == "api.describe")
		{
			return Catalog.Describe(ReadValue<std::string>(Values.at("operation")));
		}
		if (InMethod == "types.describe")
		{
			return Catalog.DescribeType(ReadValue<std::string>(Values.at("type")));
		}
		if (InMethod == "api.call")
		{
			return Session.Call(ReadValue<std::string>(Values.at("operation")), Values.at("arguments"));
		}
		if (InMethod == "jobs.get")
		{
			return Session.GetJob(ReadValue<std::string>(Values.at("job")));
		}
		return Session.CancelJob(ReadValue<std::string>(Values.at("job")));
	}
	catch (...)
	{
		return CurrentAutomationFailure();
	}
}

FArchiveNode AutomationTools()
{
	FArchiveNode::FArray Result;
	for (const auto& Tool : Bootstrap)
	{
		Result.emplace_back(FArchiveNode::FObject{
		    {"name", WriteValue(std::string(Tool.Name))},
		    {"description", WriteValue(std::string(Tool.Description))},
		    {"inputSchema", ParseJson(Tool.Schema)},
		    {"annotations", FArchiveNode(FArchiveNode::FObject{{"readOnlyHint", WriteValue(Tool.bReadOnly)},
		                                                       {"openWorldHint", WriteValue(false)}})}});
	}
	return FArchiveNode(FArchiveNode::FObject{{"tools", FArchiveNode(std::move(Result))}});
}

FArchiveNode FAutomationEndpoint::Tools() const
{
	return AutomationTools();
}

FEndpointRequest ReadyAutomationRequest(FArchiveNode InValue)
{
	return {[Value = std::optional(std::move(InValue))]() mutable
	        {
		        return std::exchange(Value, {});
	        }};
}

FEndpointRequest FAutomationEndpoint::Begin(std::string_view InMethod, const FArchiveNode& InParameters)
{
	return ReadyAutomationRequest(Execute(InMethod, InParameters));
}
} // namespace Hyperion
