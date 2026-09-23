#include "Hyperion/Automation/Endpoint.h"

namespace Hyperion
{
namespace
{
class FRpcError : public std::runtime_error
{
public:
	FRpcError(int InCode, std::string InMessage) : std::runtime_error(std::move(InMessage)), Code(InCode)
	{
	}

	int Code;
};

FArchiveNode RpcFailure(const FArchiveNode& InId, int InCode, std::string InMessage)
{
	return FArchiveNode(FArchiveNode::FObject{
	    {"jsonrpc", WriteValue(std::string("2.0"))},
	    {"id", InId},
	    {"error",
	     FArchiveNode(FArchiveNode::FObject{{"code", WriteValue(InCode)}, {"message", WriteValue(InMessage)}})}});
}

FArchiveNode ToolResult(FArchiveNode InResult)
{
	const auto& Fields = std::get<FArchiveNode::FObject>(InResult.Value);
	const bool bFailed = Fields.contains("status") && ReadValue<std::string>(Fields.at("status")) == "failed";
	std::string Text;
	try
	{
		Text = WriteAutomationResponse(InResult);
	}
	catch (const std::exception& Error)
	{
		throw FRpcError(-32603, std::string("Result encoding failed after execution; inspect current state before any "
		                                    "retry: ") +
		                            Error.what());
	}
	FArchiveNode::FArray Content{
	    FArchiveNode(FArchiveNode::FObject{{"type", WriteValue(std::string("text"))}, {"text", WriteValue(Text)}})};
	return FArchiveNode(FArchiveNode::FObject{{"content", FArchiveNode(std::move(Content))},
	                                          {"structuredContent", std::move(InResult)},
	                                          {"isError", WriteValue(bFailed)}});
}

std::string RpcResponse(const FArchiveNode& InId, FArchiveNode InResult)
{
	try
	{
		return WriteAutomationResponse(FArchiveNode(FArchiveNode::FObject{
		    {"jsonrpc", WriteValue(std::string("2.0"))}, {"id", InId}, {"result", std::move(InResult)}}));
	}
	catch (const std::exception& Error)
	{
		return WriteAutomationResponse(RpcFailure(
		    InId, -32603,
		    std::string("Response encoding failed after execution; inspect current state before any retry: ") +
		        Error.what()));
	}
}
} // namespace

FMcpConnection::FMcpConnection(IAutomationEndpoint& InEndpoint) : Endpoint(InEndpoint)
{
}

FArchiveNode FMcpConnection::Dispatch(std::string_view InMethod, const FArchiveNode& InParameters)
{
	const auto& Params = std::get<FArchiveNode::FObject>(InParameters.Value);
	if (InMethod == "initialize")
	{
		if (bInitialized)
		{
			throw FRpcError(-32600, "Connection already initialized");
		}
		(void)ReadValue<std::string>(Params.at("protocolVersion"));
		(void)std::get<FArchiveNode::FObject>(Params.at("capabilities").Value);
		const auto& Client = std::get<FArchiveNode::FObject>(Params.at("clientInfo").Value);
		(void)ReadValue<std::string>(Client.at("name"));
		(void)ReadValue<std::string>(Client.at("version"));
		bInitialized = true;
		return ParseJson(
		    R"({"protocolVersion":"2025-11-25","capabilities":{"tools":{"listChanged":false}},"serverInfo":{"name":"hyperion","version":"1.0.0"},"instructions":"Use targets.list/connect to attach to a running application, then pass the returned connection to every routed call. Otherwise calls use the configured default or standalone session. Use api.search then api.describe before api.call. Poll jobs.get on the same connection for running calls. Jobs are session scoped; live documents belong to the application. No automatic retry or fallback on disconnect. Read effects and completion before invoking an operation."})");
	}
	if (InMethod == "ping")
	{
		return FArchiveNode(FArchiveNode::FObject{});
	}
	if (!bReady)
	{
		throw FRpcError(-32600, "Initialize and send notifications/initialized first");
	}
	if (InMethod == "tools/list")
	{
		if (Params.contains("cursor"))
		{
			throw FRpcError(-32602, "This fixed bootstrap catalog has no next page");
		}
		return Endpoint.Tools();
	}
	throw FRpcError(-32601, "Unsupported MCP method: " + std::string(InMethod));
}

FEndpointRequest FMcpConnection::BeginTool(const FArchiveNode& InParameters)
{
	if (!bReady)
	{
		throw FRpcError(-32600, "Initialize and send notifications/initialized first");
	}
	if (Pending.size() >= 32)
	{
		throw FRpcError(-32000, "Too many pending requests");
	}
	const auto& Params = std::get<FArchiveNode::FObject>(InParameters.Value);
	const auto Name = ReadValue<std::string>(Params.at("name"));
	const auto Catalog = Endpoint.Tools();
	const auto& Tools =
	    std::get<FArchiveNode::FArray>(std::get<FArchiveNode::FObject>(Catalog.Value).at("tools").Value);
	if (std::none_of(Tools.begin(), Tools.end(),
	                 [&](const FArchiveNode& InTool)
	                 {
		                 return ReadValue<std::string>(std::get<FArchiveNode::FObject>(InTool.Value).at("name")) ==
		                        Name;
	                 }))
	{
		throw FRpcError(-32602, "Unknown bootstrap tool: " + Name);
	}
	return Endpoint.Begin(Name, Params.contains("arguments") ? Params.at("arguments")
	                                                         : FArchiveNode(FArchiveNode::FObject{}));
}

std::vector<std::string> FMcpConnection::Poll()
{
	std::vector<std::string> Responses;
	for (auto It = Pending.begin(); It != Pending.end();)
	{
		try
		{
			if (auto Result = It->Request.Poll())
			{
				Responses.push_back(RpcResponse(It->Id, ToolResult(std::move(*Result))));
				It = Pending.erase(It);
			}
			else
			{
				++It;
			}
		}
		catch (const std::exception& Error)
		{
			Responses.push_back(WriteAutomationResponse(RpcFailure(It->Id, -32603, Error.what())));
			It = Pending.erase(It);
		}
	}
	return Responses;
}

bool FMcpConnection::HasPending() const
{
	return !Pending.empty();
}

std::optional<std::string> FMcpConnection::Receive(std::string_view InMessage)
{
	FArchiveNode Id(std::monostate{});
	FArchiveNode Request;
	try
	{
		Request = ParseJson(InMessage);
	}
	catch (const std::exception& Error)
	{
		return WriteAutomationResponse(RpcFailure(Id, -32700, Error.what()));
	}
	bool bNotification{};
	try
	{
		const auto* Fields = std::get_if<FArchiveNode::FObject>(&Request.Value);
		if (!Fields || !Fields->contains("jsonrpc") || !Fields->contains("method") ||
		    !std::holds_alternative<std::string>(Fields->at("jsonrpc").Value) ||
		    !std::holds_alternative<std::string>(Fields->at("method").Value))
		{
			throw FRpcError(-32600, "Invalid JSON-RPC request");
		}
		if (ReadValue<std::string>(Fields->at("jsonrpc")) != "2.0")
		{
			throw FRpcError(-32600, "Expected JSON-RPC 2.0");
		}
		const auto Method = ReadValue<std::string>(Fields->at("method"));
		bNotification = !Fields->contains("id");
		if (!bNotification)
		{
			const auto& Candidate = Fields->at("id");
			if (!std::holds_alternative<std::string>(Candidate.Value) &&
			    !std::holds_alternative<std::int64_t>(Candidate.Value) &&
			    !std::holds_alternative<std::uint64_t>(Candidate.Value))
			{
				throw FRpcError(-32600, "Invalid request ID");
			}
			Id = Candidate;
		}
		if (bNotification)
		{
			if (Method == "notifications/initialized" && bInitialized)
			{
				bReady = true;
			}
			return {};
		}
		const auto Parameters =
		    Fields->contains("params") ? Fields->at("params") : FArchiveNode(FArchiveNode::FObject{});
		if (Method == "tools/call")
		{
			auto RequestState = BeginTool(Parameters);
			if (auto Result = RequestState.Poll())
			{
				return RpcResponse(Id, ToolResult(std::move(*Result)));
			}
			Pending.push_back({std::move(Id), std::move(RequestState)});
			return {};
		}
		return RpcResponse(Id, Dispatch(Method, Parameters));
	}
	catch (const FRpcError& Error)
	{
		return bNotification ? std::nullopt
		                     : std::optional(WriteAutomationResponse(RpcFailure(Id, Error.Code, Error.what())));
	}
	catch (const std::exception& Error)
	{
		return bNotification ? std::nullopt
		                     : std::optional(WriteAutomationResponse(RpcFailure(Id, -32602, Error.what())));
	}
}
} // namespace Hyperion
