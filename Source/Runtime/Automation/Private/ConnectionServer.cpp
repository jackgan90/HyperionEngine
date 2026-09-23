#include "ConnectionInternal.h"

namespace Hyperion
{
struct FAutomationServer::FImpl
{
	struct FPeer
	{
		FAutomationChannel Channel;
		FAutomationSession Session;
		FAutomationEndpoint Endpoint;
		FConnectionClock::time_point Deadline = FConnectionClock::now() + ConnectionTimeout;
		bool bHello{};
		bool bClosing{};

		FPeer(std::unique_ptr<ITransportConnection> InConnection, const FOperationCatalog& InCatalog)
		    : Channel(std::move(InConnection)), Session(InCatalog, {8, 32}), Endpoint(Session)
		{
		}
	};

	const FOperationCatalog& Catalog;
	FAutomationTarget Info;
	std::unique_ptr<ITransportListener> Listener;
	const IAutomationAccessPolicy& Access;
	std::vector<std::unique_ptr<FPeer>> Peers;
	bool bStopped{};
	FConnectionClock::time_point DrainDeadline{};

	void Accept();
	void PollPeer(FPeer& InPeer);
	void Message(FPeer& InPeer, const std::string& InMessage);
	FArchiveNode Hello(FPeer& InPeer, const FArchiveNode& InParameters);
};

FAutomationServer::FAutomationServer(const FOperationCatalog& InCatalog, FAutomationTarget InTarget,
                                     std::unique_ptr<ITransportListener> InListener,
                                     const IAutomationAccessPolicy& InAccess)
    : Impl(std::make_unique<FImpl>(InCatalog, std::move(InTarget), std::move(InListener), InAccess))
{
	if (!InCatalog.IsSealed() || !Impl->Listener)
	{
		throw std::invalid_argument("Automation server requires a sealed catalog and listener");
	}
}

FAutomationServer::~FAutomationServer() = default;

void FAutomationServer::FImpl::Accept()
{
	if (bStopped || Peers.size() >= 32)
	{
		return;
	}
	if (auto Connection = Listener->Accept())
	{
		if (!Access.Admit(Connection->Peer()))
		{
			Connection->Close();
			return;
		}
		Peers.push_back(std::make_unique<FPeer>(std::move(Connection), Catalog));
	}
}

FArchiveNode FAutomationServer::FImpl::Hello(FPeer& InPeer, const FArchiveNode& InParameters)
{
	const auto& Fields = ConnectionFields(InParameters);
	CheckConnectionKeys(Fields, {"protocol", "instance", "maxFrame"});
	if (ReadValue<std::uint32_t>(Fields.at("protocol")) != AutomationProtocolVersion)
	{
		throw FAutomationError("protocol_mismatch", "Unsupported automation communication version");
	}
	const auto Expected = ReadValue<std::string>(Fields.at("instance"));
	if (!Expected.empty() && Expected != Info.Instance)
	{
		throw FAutomationError("stale_target", "Target instance changed; discover and select it explicitly");
	}
	if (ReadValue<std::uint32_t>(Fields.at("maxFrame")) < AutomationResponseLimits.MaxBytes)
	{
		throw FAutomationError("protocol_mismatch", "Peer response budget is too small");
	}
	InPeer.bHello = true;
	return CompletedConnectionResult(FArchiveNode(
	    FArchiveNode::FObject{{"protocol", WriteValue(AutomationProtocolVersion)},
	                          {"target", WriteRecordWire(RecordType<FAutomationTarget>(), &Info)},
	                          {"session", WriteValue(InPeer.Session.GetId())},
	                          {"catalog", WriteValue(Info.Instance)},
	                          {"maxFrame", WriteValue(static_cast<std::uint32_t>(AutomationResponseLimits.MaxBytes))},
	                          {"maxRequest", WriteValue(static_cast<std::uint32_t>(FJsonLimits{}.MaxBytes))},
	                          {"resume", WriteValue(false)}}));
}

void FAutomationServer::FImpl::Message(FPeer& InPeer, const std::string& InMessage)
{
	std::string Id;
	FArchiveNode Result;
	try
	{
		const auto Request = ParseJson(InMessage);
		const auto& Fields = ConnectionFields(Request);
		CheckConnectionKeys(Fields, {"id", "method", "params"});
		Id = ReadValue<std::string>(Fields.at("id"));
		if (Id.empty() || Id.size() > 64)
		{
			throw FAutomationError("protocol_error", "Invalid request correlation ID");
		}
		const auto Method = ReadValue<std::string>(Fields.at("method"));
		if (!InPeer.bHello)
		{
			if (Method != "hello" || InMessage.size() > 8192)
			{
				throw FAutomationError("protocol_error", "A bounded hello must precede requests");
			}
			Result = Hello(InPeer, Fields.at("params"));
		}
		else
		{
			Result = InPeer.Endpoint.Execute(Method, Fields.at("params"));
			if (Method == "engine.info")
			{
				std::get<FArchiveNode::FObject>(Result.Value)
				    .emplace("target", WriteRecordWire(RecordType<FAutomationTarget>(), &Info));
			}
		}
	}
	catch (...)
	{
		Result = CurrentConnectionFailure();
		InPeer.bClosing = !InPeer.bHello;
	}
	InPeer.Channel.Send(WriteAutomationResponse(
	    FArchiveNode(FArchiveNode::FObject{{"id", WriteValue(Id)}, {"result", std::move(Result)}})));
}

void FAutomationServer::FImpl::PollPeer(FPeer& InPeer)
{
	InPeer.Session.Poll();
	try
	{
		InPeer.Channel.Poll();
		if (!InPeer.bHello && FConnectionClock::now() > InPeer.Deadline)
		{
			InPeer.Channel.Close();
		}
		for (unsigned Count = 0; Count < 4 && !InPeer.bClosing && !bStopped; ++Count)
		{
			const auto Request = InPeer.Channel.Receive();
			if (!Request)
			{
				break;
			}
			Message(InPeer, *Request);
		}
		if (InPeer.bClosing && (InPeer.Channel.IsDrained() || (bStopped && FConnectionClock::now() >= DrainDeadline)))
		{
			InPeer.Channel.Close();
		}
	}
	catch (const std::exception&)
	{
		InPeer.Channel.Close();
	}
	if (InPeer.Channel.State() == ETransportState::Closed || InPeer.Channel.State() == ETransportState::Failed)
	{
		InPeer.bClosing = true;
		InPeer.Session.StopAdmission();
	}
}

void FAutomationServer::Poll()
{
	Impl->Catalog.RequireOwner();
	Impl->Accept();
	for (auto& Peer : Impl->Peers)
	{
		Impl->PollPeer(*Peer);
	}
	std::erase_if(Impl->Peers,
	              [](const auto& InPeer)
	              {
		              return InPeer->bClosing && !InPeer->Session.PendingCount() &&
		                     (InPeer->Channel.State() == ETransportState::Closed ||
		                      InPeer->Channel.State() == ETransportState::Failed);
	              });
}

void FAutomationServer::StopAdmission()
{
	if (Impl->bStopped)
	{
		return;
	}
	Impl->bStopped = true;
	// Flush already queued replies before normal shutdown. A stalled peer must not prevent exit.
	Impl->DrainDeadline = FConnectionClock::now() + std::chrono::seconds(2);
	Impl->Listener->Close();
	for (auto& Peer : Impl->Peers)
	{
		Peer->Session.StopAdmission();
		Peer->bClosing = true;
	}
}

std::size_t FAutomationServer::PendingCount() const
{
	std::size_t Count{};
	for (const auto& Peer : Impl->Peers)
	{
		Count += Peer->Session.PendingCount();
		if (Impl->bStopped && Peer->Channel.State() != ETransportState::Closed &&
		    Peer->Channel.State() != ETransportState::Failed)
		{
			++Count;
		}
	}
	return Count;
}

const FAutomationTarget& FAutomationServer::Target() const
{
	return Impl->Info;
}
} // namespace Hyperion
