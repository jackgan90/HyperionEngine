#include "ConnectionInternal.h"
#include <utility>

namespace Hyperion
{
namespace
{
struct FRemoteRequest
{
	std::optional<FArchiveNode> Result;
	FConnectionClock::time_point Deadline = FConnectionClock::now() + ConnectionTimeout;
};

struct FRemoteConnection
{
	std::string Id = CreateAutomationIdentity();
	FAutomationChannel Channel;
	std::string ExpectedInstance;
	std::map<std::string, std::shared_ptr<FRemoteRequest>> Requests;
	std::uint64_t NextRequest{};
	bool bReady{};
	bool bClosed{};
	bool bAdmitted{};
	const IAutomationAccessPolicy& Access;

	FRemoteConnection(std::unique_ptr<ITransportConnection> InConnection, const IAutomationAccessPolicy& InAccess)
	    : Channel(std::move(InConnection)), Access(InAccess)
	{
	}

	FEndpointRequest Begin(std::string_view InMethod, const FArchiveNode& InParameters);
	void Poll();
	void Close(FArchiveNode InFailure);
};

FEndpointRequest FRemoteConnection::Begin(std::string_view InMethod, const FArchiveNode& InParameters)
{
	if (bClosed)
	{
		throw FAutomationError("disconnected", "Target connection is closed; no fallback or retry was performed");
	}
	if (Requests.size() >= 32)
	{
		throw FAutomationError("busy", "Connection request limit reached");
	}
	const auto RequestId = std::to_string(++NextRequest);
	const auto Message = WriteJson(FArchiveNode(FArchiveNode::FObject{
	    {"id", WriteValue(RequestId)}, {"method", WriteValue(std::string(InMethod))}, {"params", InParameters}}));
	auto Request = std::make_shared<FRemoteRequest>();
	Requests.emplace(RequestId, Request);
	try
	{
		Channel.Send(Message);
	}
	catch (...)
	{
		Requests.erase(RequestId);
		throw;
	}
	return {[Request]() mutable
	        {
		        return std::exchange(Request->Result, {});
	        }};
}

void FRemoteConnection::Close(FArchiveNode InFailure)
{
	bClosed = true;
	Channel.Close();
	for (auto& [RequestId, Request] : Requests)
	{
		Request->Result = InFailure;
	}
	Requests.clear();
}

void FRemoteConnection::Poll()
{
	if (bClosed)
	{
		return;
	}
	try
	{
		Channel.Poll(bAdmitted);
		if (!bAdmitted && Channel.State() == ETransportState::Connected)
		{
			if (!Access.Admit(Channel.Peer()))
			{
				throw FAutomationError("access_denied", "Transport peer failed the configured admission policy");
			}
			bAdmitted = true;
		}
		// Drain up to the entire outstanding-request budget before interpreting a terminal stream state.
		for (unsigned Count = 0; bAdmitted && Count < 32; ++Count)
		{
			const auto Message = Channel.Receive();
			if (!Message)
			{
				break;
			}
			const auto Value = ParseJson(*Message, AutomationResponseLimits);
			const auto& Fields = ConnectionFields(Value);
			CheckConnectionKeys(Fields, {"id", "result"});
			const auto Found = Requests.find(ReadValue<std::string>(Fields.at("id")));
			if (Found == Requests.end())
			{
				throw FAutomationError("protocol_error", "Unexpected response correlation ID");
			}
			Found->second->Result = Fields.at("result");
			Requests.erase(Found);
		}
		if (Channel.State() == ETransportState::Closed || Channel.State() == ETransportState::Failed)
		{
			throw FAutomationError("disconnected", "Target disconnected; transmitted mutations may have completed");
		}
		const auto Now = FConnectionClock::now();
		for (const auto& [RequestId, Request] : Requests)
		{
			if (Now > Request->Deadline)
			{
				throw FAutomationError("timeout",
				                       "Reply timed out; outcome may be unknown. Re-query, do not replay mutations");
			}
		}
	}
	catch (...)
	{
		Close(CurrentConnectionFailure());
	}
}

FArchiveNode FinishConnect(FRemoteConnection& InClient, const FArchiveNode& InHello)
{
	const auto& Fields = ConnectionFields(InHello);
	if (Fields.contains("status") && ReadValue<std::string>(Fields.at("status")) == "failed")
	{
		InClient.Close(InHello);
		return InHello;
	}
	const auto& Result = ConnectionFields(Fields.at("result"));
	if (ReadValue<std::uint32_t>(Result.at("protocol")) != AutomationProtocolVersion ||
	    ReadValue<std::uint32_t>(Result.at("maxFrame")) > AutomationResponseLimits.MaxBytes)
	{
		throw FAutomationError("protocol_mismatch", "Target selected an unsupported communication contract");
	}
	const auto Target = ReadRecordWire(RecordType<FAutomationTarget>(), Result.at("target"));
	const auto& Info = *static_cast<const FAutomationTarget*>(Target.get());
	if (!InClient.ExpectedInstance.empty() && InClient.ExpectedInstance != Info.Instance)
	{
		throw FAutomationError("stale_target", "Connected instance does not match the selected target");
	}
	InClient.bReady = true;
	auto Response = Result;
	Response.emplace("connection", WriteValue(InClient.Id));
	return CompletedConnectionResult(FArchiveNode(std::move(Response)));
}
} // namespace

struct FConnectionManager::FImpl
{
	FTransportRegistry& Transports;
	ITargetDiscovery& Discovery;
	const IAutomationAccessPolicy& Access;
	std::map<std::string, std::shared_ptr<FRemoteConnection>> Clients;
	FTransportAddress Resolve(const FArchiveNode::FObject& InFields);
};

FTransportAddress FConnectionManager::FImpl::Resolve(const FArchiveNode::FObject& InFields)
{
	if (InFields.contains("address"))
	{
		const auto Address = ReadRecordWire(RecordType<FTransportAddress>(), InFields.at("address"));
		return *static_cast<const FTransportAddress*>(Address.get());
	}
	if (!InFields.contains("instance"))
	{
		throw FAutomationError("invalid_arguments", "Specify an instance or a transport address");
	}
	const auto Instance = ReadValue<std::string>(InFields.at("instance"));
	for (const auto& Target : Discovery.List())
	{
		if (Target.Instance == Instance)
		{
			return Target.Address;
		}
	}
	throw FAutomationError("not_found", "Target was not discovered; an explicit address can also be supplied");
}

FConnectionManager::FConnectionManager(FTransportRegistry& InTransports, ITargetDiscovery& InDiscovery,
                                       const IAutomationAccessPolicy& InAccess)
    : Impl(std::make_unique<FImpl>(InTransports, InDiscovery, InAccess))
{
}

FConnectionManager::~FConnectionManager()
{
	Close();
}

FArchiveNode FConnectionManager::List()
{
	FArchiveNode::FArray Targets;
	for (const auto& Target : Impl->Discovery.List())
	{
		Targets.push_back(WriteRecordWire(RecordType<FAutomationTarget>(), &Target));
	}
	return CompletedConnectionResult(FArchiveNode(FArchiveNode::FObject{
	    {"targets", FArchiveNode(std::move(Targets))},
	    {"contract", WriteValue(std::string(
	                     "Candidates may be stale; connect verifies identity. Domain paths belong to the target."))}}));
}

FEndpointRequest FConnectionManager::Connect(const FArchiveNode& InParameters)
{
	return BeginConnection(InParameters, false, 10000);
}

FEndpointRequest FConnectionManager::Probe(const FArchiveNode& InParameters)
{
	auto Fields = ConnectionFields(InParameters);
	CheckConnectionKeys(Fields, {"instance", "address", "timeoutMs"});
	const auto Timeout = Fields.contains("timeoutMs") ? ReadValue<std::uint32_t>(Fields.at("timeoutMs")) : 1000;
	if (Timeout < 50 || Timeout > 5000)
	{
		throw FAutomationError("invalid_arguments", "Probe timeoutMs must be 50-5000");
	}
	Fields.erase("timeoutMs");
	return BeginConnection(FArchiveNode(std::move(Fields)), true, Timeout);
}

FEndpointRequest FConnectionManager::BeginConnection(const FArchiveNode& InParameters, bool bInProbe,
                                                     std::uint32_t InTimeoutMs)
{
	const auto& Fields = ConnectionFields(InParameters);
	CheckConnectionKeys(Fields, {"instance", "address"});
	if (Impl->Clients.size() >= 16)
	{
		throw FAutomationError("busy", "Disconnect an existing target before opening more connections");
	}
	const auto Address = Impl->Resolve(Fields);
	auto Connection = Impl->Transports.Connect(Address);
	auto Client = std::make_shared<FRemoteConnection>(std::move(Connection), Impl->Access);
	Client->ExpectedInstance = Fields.contains("instance") ? ReadValue<std::string>(Fields.at("instance")) : "";
	auto Hello = Client->Begin(
	    "hello", FArchiveNode(FArchiveNode::FObject{
	                 {"protocol", WriteValue(AutomationProtocolVersion)},
	                 {"instance", WriteValue(Client->ExpectedInstance)},
	                 {"maxFrame", WriteValue(static_cast<std::uint32_t>(AutomationResponseLimits.MaxBytes))}}));
	Impl->Clients.emplace(Client->Id, Client);
	for (auto& [Id, Request] : Client->Requests)
	{
		Request->Deadline = FConnectionClock::now() + std::chrono::milliseconds(InTimeoutMs);
	}
	return {[Client, bInProbe, Hello = std::move(Hello)]() mutable -> std::optional<FArchiveNode>
	        {
		        if (const auto Result = Hello.Poll())
		        {
			        try
			        {
				        auto Response = FinishConnect(*Client, *Result);
				        if (bInProbe)
				        {
					        auto& Fields = std::get<FArchiveNode::FObject>(Response.Value);
					        if (Fields.contains("result"))
					        {
						        auto& Info = std::get<FArchiveNode::FObject>(Fields.at("result").Value);
						        const auto Target = Info.at("target");
						        Info = {{"target", Target}};
						        const auto CheckedAt = std::chrono::duration_cast<std::chrono::milliseconds>(
						                                   std::chrono::system_clock::now().time_since_epoch())
						                                   .count();
						        Info.emplace("checkedAtUnixMs", WriteValue(std::to_string(CheckedAt)));
						        Info.emplace("reachable", WriteValue(true));
						        Info.emplace("contract",
						                     WriteValue(std::string(
						                         "Advisory handshake snapshot only. Connect validates identity again; "
						                         "timeout/failure is not proof that the process is dead.")));
					        }
					        Client->bReady = false;
					        Client->Close(AutomationFailure("disconnected", "Probe completed"));
				        }
				        return Response;
			        }
			        catch (...)
			        {
				        auto Failure = CurrentConnectionFailure();
				        Client->Close(Failure);
				        return Failure;
			        }
		        }
		        return {};
	        }};
}

FArchiveNode FConnectionManager::Disconnect(const std::string& InConnection)
{
	const auto Found = Impl->Clients.find(InConnection);
	if (Found == Impl->Clients.end())
	{
		throw FAutomationError("not_found", "Unknown connection");
	}
	Found->second->Close(
	    AutomationFailure("disconnected", "Connection explicitly closed; admitted work may finish on the target"));
	Impl->Clients.erase(Found);
	return CompletedConnectionResult(FArchiveNode(FArchiveNode::FObject{{"disconnected", WriteValue(true)}}));
}

FEndpointRequest FConnectionManager::Request(const std::string& InConnection, std::string_view InMethod,
                                             const FArchiveNode& InParameters)
{
	const auto Found = Impl->Clients.find(InConnection);
	if (Found == Impl->Clients.end())
	{
		throw FAutomationError("disconnected", "Unknown connection; no target fallback was performed");
	}
	if (!Found->second->bReady && !Found->second->bClosed)
	{
		throw FAutomationError("busy", "Target handshake has not completed");
	}
	return Found->second->Begin(InMethod, InParameters);
}

void FConnectionManager::Poll()
{
	for (auto& [Id, Client] : Impl->Clients)
	{
		Client->Poll();
	}
	// Failed handshakes never returned a usable connection ID; release their slots automatically.
	std::erase_if(Impl->Clients,
	              [](const auto& InEntry)
	              {
		              return InEntry.second->bClosed && !InEntry.second->bReady;
	              });
}

void FConnectionManager::Close()
{
	for (auto& [Id, Client] : Impl->Clients)
	{
		Client->Close(AutomationFailure("disconnected", "Frontend is shutting down; admitted target work may finish"));
	}
	Impl->Clients.clear();
}
} // namespace Hyperion
