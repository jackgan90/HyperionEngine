#include "Hyperion/Application/ApplicationHost.h"
#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include "StdioReader.h"
#include <chrono>
#include <iostream>
#include <thread>

namespace Hyperion
{
namespace
{
bool IsFailed(const FArchiveNode& InResult)
{
	const auto& Fields = std::get<FArchiveNode::FObject>(InResult.Value);
	const auto It = Fields.find("status");
	return It != Fields.end() && ReadValue<std::string>(It->second) == "failed";
}

class FAutomationStdioPlugin final : public FPlugin
{
public:
	explicit FAutomationStdioPlugin(FAutomationStreamOptions InOptions) : Options(std::move(InOptions))
	{
	}

	void Start(FPluginContext& InContext) override
	{
		Session = &InContext.Require<FAutomationSession>();
		Endpoint = &InContext.Require<FAutomationEndpoint>();
		Control = &InContext.Require<FApplicationControl>();
		if (Options.Transport != EAutomationTransport::Once)
		{
			Reader = std::make_unique<FStdioReader>();
		}
		if (Options.Transport == EAutomationTransport::Mcp)
		{
			Mcp = std::make_unique<FMcpConnection>(*Endpoint);
		}
		InContext.Provide(Status);
	}

	void Update(const FPluginUpdate&) override;

	void Quiesce() noexcept override
	{
		if (Session)
		{
			Session->StopAdmission();
		}
	}

private:
	void Write(std::string_view InMessage);
	void Receive(std::string_view InMessage);
	void ReceiveJsonLine(std::string_view InMessage);
	void Once();
	std::size_t Consume();
	FAutomationStreamOptions Options;
	FAutomationStreamStatus Status;
	FAutomationSession* Session{};
	FAutomationEndpoint* Endpoint{};
	FApplicationControl* Control{};
	std::unique_ptr<FStdioReader> Reader;
	std::unique_ptr<FMcpConnection> Mcp;
	std::string Buffer;
	std::string WaitingJob;
};

void FAutomationStdioPlugin::Write(std::string_view InMessage)
{
	std::cout << InMessage << '\n' << std::flush;
	if (!std::cout)
	{
		throw std::runtime_error("Automation stdout disconnected");
	}
}

void FAutomationStdioPlugin::ReceiveJsonLine(std::string_view InMessage)
{
	FArchiveNode Id;
	FArchiveNode Result;
	try
	{
		const auto Request = ParseJson(InMessage);
		const auto* Fields = std::get_if<FArchiveNode::FObject>(&Request.Value);
		if (!Fields || !Fields->contains("method"))
		{
			throw FAutomationError("invalid_request", "Expected {id?, method, params?}");
		}
		for (const auto& [Key, Value] : *Fields)
		{
			if (Key != "id" && Key != "method" && Key != "params")
			{
				throw FAutomationError("invalid_request", "Unknown request field", Key);
			}
		}
		if (const auto It = Fields->find("id"); It != Fields->end())
		{
			if (!std::holds_alternative<std::string>(It->second.Value) &&
			    !std::holds_alternative<std::uint64_t>(It->second.Value) &&
			    !std::holds_alternative<std::int64_t>(It->second.Value))
			{
				throw FAutomationError("invalid_request", "id must be a string or integer", "id");
			}
			Id = It->second;
		}
		Result = Endpoint->Execute(ReadValue<std::string>(Fields->at("method")),
		                           Fields->contains("params") ? Fields->at("params")
		                                                      : FArchiveNode(FArchiveNode::FObject{}));
	}
	catch (...)
	{
		Result = CurrentAutomationFailure();
	}
	Status.bFailed |= IsFailed(Result);
	std::string Response;
	try
	{
		Response =
		    WriteAutomationResponse(FArchiveNode(FArchiveNode::FObject{{"id", Id}, {"result", std::move(Result)}}));
	}
	catch (const std::exception& Error)
	{
		Status.bFailed = true;
		Response = WriteAutomationResponse(FArchiveNode(FArchiveNode::FObject{
		    {"id", Id},
		    {"result",
		     AutomationFailure("result_unavailable",
		                       std::string("Response encoding failed after execution; inspect current state before any "
		                                   "retry: ") +
		                           Error.what())}}));
	}
	Write(Response);
}

void FAutomationStdioPlugin::Receive(std::string_view InMessage)
{
	if (Mcp)
	{
		if (const auto Response = Mcp->Receive(InMessage))
		{
			Write(*Response);
		}
	}
	else
	{
		ReceiveJsonLine(InMessage);
	}
}

std::size_t FAutomationStdioPlugin::Consume()
{
	std::size_t Count{};
	while (Count < 16)
	{
		const auto End = Buffer.find('\n');
		if ((End == std::string::npos ? Buffer.size() : End) > FJsonLimits{}.MaxBytes)
		{
			throw std::invalid_argument("Input line exceeds the 1 MiB protocol limit");
		}
		if (End == std::string::npos)
		{
			break;
		}
		Receive(std::string_view(Buffer).substr(0, End));
		Buffer.erase(0, End + 1);
		++Count;
	}
	return Count;
}

void FAutomationStdioPlugin::Once()
{
	FArchiveNode Result;
	if (WaitingJob.empty())
	{
		Result = Endpoint->Execute(Options.Method, Options.Parameters);
		const auto& Fields = std::get<FArchiveNode::FObject>(Result.Value);
		if (const auto It = Fields.find("job");
		    It != Fields.end() && ReadValue<std::string>(Fields.at("status")) == "running")
		{
			WaitingJob = ReadValue<std::string>(It->second);
			return;
		}
	}
	else
	{
		const auto Job = Session->GetJob(WaitingJob);
		const auto& Fields = std::get<FArchiveNode::FObject>(Job.Value);
		if (ReadValue<std::string>(Fields.at("status")) == "running")
		{
			return;
		}
		Result = Fields.at("outcome");
	}
	Status.bFailed |= IsFailed(Result);
	Write(WriteAutomationResponse(Result));
	Control->RequestExit();
}

void FAutomationStdioPlugin::Update(const FPluginUpdate&)
{
	if (Options.Transport == EAutomationTransport::Once)
	{
		Once();
	}
	else
	{
		if (Consume() < 16)
		{
			Buffer += Reader->Read();
			Consume();
		}
		if (Reader->IsEof() && Buffer.find('\n') == std::string::npos)
		{
			if (!Buffer.empty())
			{
				Receive(Buffer);
				Buffer.clear();
			}
			Session->StopAdmission();
			Control->RequestExit();
		}
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
} // namespace

void RegisterAutomationStdio(FPluginRegistry& InRegistry, FAutomationStreamOptions InOptions)
{
	FPluginDescriptor Descriptor;
	Descriptor.Id = "automation-stdio";
	Descriptor.Dependencies = {"automation-session"};
	Descriptor.Requires = {typeid(FAutomationSession), typeid(FAutomationEndpoint), typeid(FApplicationControl)};
	Descriptor.Provides = {typeid(FAutomationStreamStatus)};
	Descriptor.Create = [Options = std::move(InOptions)]
	{
		return std::make_unique<FAutomationStdioPlugin>(Options);
	};
	InRegistry.Add(std::move(Descriptor));
}
} // namespace Hyperion
