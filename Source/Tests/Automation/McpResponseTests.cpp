#include "Hyperion/Automation/Endpoint.h"
#include "Support/TestSupport.h"
#include <utility>

namespace
{
using namespace Hyperion;

class FResponseEndpoint final : public IAutomationEndpoint
{
public:
	FArchiveNode Response;
	bool bDeferred{};
	unsigned Invocations{};

	FEndpointRequest Begin(std::string_view, const FArchiveNode&) override
	{
		++Invocations;
		return {[Value = std::optional(Response), bWait = bDeferred]() mutable -> std::optional<FArchiveNode>
		        {
			        if (std::exchange(bWait, false))
			        {
				        return {};
			        }
			        return std::exchange(Value, {});
		        }};
	}

	FArchiveNode Tools() const override
	{
		return AutomationTools();
	}
};

const FArchiveNode::FObject& Fields(const FArchiveNode& InNode)
{
	return std::get<FArchiveNode::FObject>(InNode.Value);
}

FArchiveNode InvokeMcp(FResponseEndpoint& InEndpoint)
{
	FMcpConnection Mcp(InEndpoint);
	HYP_CHECK(Mcp.Receive(
	    R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"test","version":"1"}}})"));
	HYP_CHECK(!Mcp.Receive(R"({"jsonrpc":"2.0","method":"notifications/initialized"})"));
	auto Result =
	    Mcp.Receive(R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"engine.info","arguments":{}}})");
	if (InEndpoint.bDeferred)
	{
		HYP_CHECK(!Result && Mcp.HasPending());
		const auto Replies = Mcp.Poll();
		HYP_CHECK(Replies.size() == 1);
		Result = Replies.front();
	}
	HYP_CHECK(Result && !Mcp.HasPending() && Mcp.Poll().empty() && InEndpoint.Invocations == 1);
	return ParseJson(*Result, AutomationResponseLimits);
}

void CheckMcpResults(bool bInDeferred)
{
	struct FCase
	{
		FArchiveNode Response;
		bool bFailed;
	};

	const auto Failure = AutomationFailure("specific", "message", "/path", ParseJson(R"({"detail":42})"));
	const auto Success = AutomationCompleted(ParseJson(R"({"value":42})"));
	const FCase Cases[] = {
	    {ParseJson(R"({"session":"s"})"), false},
	    {Success, false},
	    {Failure, true},
	    {AutomationJobResponse(EAutomationStatus::Running, "j", "op", false), false},
	    {AutomationJobResponse(EAutomationStatus::Completed, "j", "op", false, Success), false},
	    {AutomationJobResponse(EAutomationStatus::Failed, "j", "op", false, Failure), true},
	    {AutomationJobResponse(EAutomationStatus::Cancelled, "j", "op", false, AutomationFailure("cancelled", "stop")),
	     false}};
	for (const auto& Case : Cases)
	{
		FResponseEndpoint Endpoint;
		Endpoint.Response = Case.Response;
		Endpoint.bDeferred = bInDeferred;
		const auto Response = InvokeMcp(Endpoint);
		const auto& Result = Fields(Fields(Response).at("result"));
		HYP_CHECK(ReadValue<bool>(Result.at("isError")) == Case.bFailed);
		HYP_CHECK(WriteJson(Result.at("structuredContent")) == WriteJson(Case.Response));
		const auto& Content = std::get<FArchiveNode::FArray>(Result.at("content").Value);
		HYP_CHECK(Content.size() == 1 &&
		          ReadValue<std::string>(Fields(Content[0]).at("text")) == WriteJson(Case.Response));
	}
	for (const auto Invalid : {R"({"status":"unknown"})", R"({"status":"completed"})", "null"})
	{
		FResponseEndpoint Endpoint;
		Endpoint.Response = ParseJson(Invalid);
		Endpoint.bDeferred = bInDeferred;
		const auto Response = InvokeMcp(Endpoint);
		HYP_CHECK(ReadValue<int>(Fields(Fields(Response).at("error")).at("code")) == -32603);
	}
}
} // namespace

void CheckMcpResponseContracts()
{
	CheckMcpResults(false);
	CheckMcpResults(true);
}
