#include "Hyperion/Automation/Connections.h"
#include "Hyperion/Transport/MemoryTransport.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <iostream>
#include <thread>

namespace Hyperion
{
namespace
{
const FArchiveNode::FObject& Fields(const FArchiveNode& InValue)
{
	return std::get<FArchiveNode::FObject>(InValue.Value);
}

class FDiscovery final : public ITargetDiscovery
{
public:
	std::vector<FAutomationTarget> Targets;

	std::vector<FAutomationTarget> List() override
	{
		return Targets;
	}
};

FArchiveNode Await(FEndpointRequest InRequest, FConnectionManager& InManager, FAutomationServer& InServer)
{
	for (unsigned Step = 0; Step < 20000; ++Step)
	{
		InServer.Poll();
		InManager.Poll();
		if (auto Result = InRequest.Poll())
		{
			return *Result;
		}
	}
	throw std::runtime_error("Connection request did not finish");
}

void Connections()
{
	FTransportRegistry Transports;
	Transports.Register(MakeMemoryTransport(128, 3));
	FOperationCatalog Catalog;
	Catalog.Seal();
	FCurrentUserAccessPolicy Access;
	FDiscovery Discovery;
	FAutomationTarget Target{CreateAutomationIdentity(), "Test", "test-build", {"memory", "target"}};
	Discovery.Targets.push_back(Target);
	FAutomationServer Server(Catalog, Target, Transports.Listen(Target.Address), Access);
	FConnectionManager Manager(Transports, Discovery, Access);
	FAutomationSession LocalSession(Catalog);
	FAutomationEndpoint LocalEndpoint(LocalSession);
	FAutomationRouter Router(LocalEndpoint, Manager);
	for (unsigned Index = 0; Index < 20; ++Index)
	{
		const auto Probed = Await(
		    Router.Begin("targets.probe", ParseJson("{\"instance\":\"" + Target.Instance + "\"}")), Manager, Server);
		const auto& Probe = Fields(Fields(Probed).at("result"));
		HYP_CHECK(ReadValue<bool>(Probe.at("reachable")) && !Probe.contains("connection") &&
		          !Probe.contains("session") && Probe.contains("checkedAtUnixMs"));
		HYP_CHECK(ReadValue<std::string>(Fields(Probe.at("target")).at("instance")) == Target.Instance);
	}
	const auto InvalidProbe = Router.Begin("targets.probe", ParseJson(R"({"instance":"old","timeoutMs":0})")).Poll();
	HYP_CHECK(InvalidProbe &&
	          ReadValue<std::string>(Fields(Fields(*InvalidProbe).at("error")).at("code")) == "invalid_arguments");
	const auto StaleProbe =
	    Await(Router.Begin("targets.probe",
	                       ParseJson(R"({"instance":"old","address":{"scheme":"memory","address":"target"}})")),
	          Manager, Server);
	HYP_CHECK(ReadValue<std::string>(Fields(Fields(StaleProbe).at("error")).at("code")) == "stale_target");
	auto Waiting =
	    Router.Begin("targets.probe", ParseJson("{\"instance\":\"" + Target.Instance + "\",\"timeoutMs\":50}"));
	std::this_thread::sleep_for(std::chrono::milliseconds(60));
	Manager.Poll();
	const auto Timeout = Waiting.Poll();
	HYP_CHECK(Timeout && ReadValue<std::string>(Fields(Fields(*Timeout).at("error")).at("code")) == "timeout");
	const auto Connected = Await(
	    Router.Begin("targets.connect", ParseJson("{\"instance\":\"" + Target.Instance + "\"}")), Manager, Server);
	HYP_CHECK(ReadValue<std::string>(Fields(Connected).at("status")) == "completed");
	const auto Connection = ReadValue<std::string>(Fields(Fields(Connected).at("result")).at("connection"));
	const auto Info =
	    Await(Router.Begin("engine.info", ParseJson("{\"connection\":\"" + Connection + "\"}")), Manager, Server);
	HYP_CHECK(ReadValue<std::string>(Fields(Fields(Info).at("target")).at("instance")) == Target.Instance);
	HYP_CHECK(ReadValue<std::string>(Fields(Info).at("session")) != LocalSession.GetId());
	const auto Unknown =
	    Await(Router.Begin("targets.connect", ParseJson(R"({"address":{"scheme":"tls","address":"device"}})")), Manager,
	          Server);
	HYP_CHECK(ReadValue<std::string>(Fields(Fields(Unknown).at("error")).at("code")) == "unsupported_transport");
	const auto Stale =
	    Await(Router.Begin("targets.connect",
	                       ParseJson(R"({"instance":"old","address":{"scheme":"memory","address":"target"}})")),
	          Manager, Server);
	HYP_CHECK(ReadValue<std::string>(Fields(Fields(Stale).at("error")).at("code")) == "stale_target");
	Server.StopAdmission();
	const auto Lost =
	    Await(Router.Begin("engine.info", ParseJson("{\"connection\":\"" + Connection + "\"}")), Manager, Server);
	HYP_CHECK(ReadValue<std::string>(Fields(Lost).at("status")) == "failed");
	const auto Local = Router.Begin("engine.info", ParseJson("{}")).Poll();
	HYP_CHECK(Local && ReadValue<std::string>(Fields(*Local).at("session")) == LocalSession.GetId());
}

void Framing()
{
	FTransportRegistry Transports;
	Transports.Register(MakeMemoryTransport(8, 1));
	auto Listener = Transports.Listen({"memory", "framing"});
	FAutomationChannel Client(Transports.Connect({"memory", "framing"}), 32);
	FAutomationChannel Server(Listener->Accept(), 32);
	Client.Send("hello");
	Client.Send("second");
	std::vector<std::string> Messages;
	for (unsigned Step = 0; Step < 100; ++Step)
	{
		Client.Poll();
		Server.Poll();
		if (const auto Message = Server.Receive())
		{
			Messages.push_back(*Message);
		}
	}
	HYP_CHECK(Messages == (std::vector<std::string>{"hello", "second"}));
	auto Raw = Transports.Connect({"memory", "framing"});
	FAutomationChannel Invalid(Listener->Accept(), 32);
	HYP_CHECK(Raw->Send({std::byte{0x7f}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}}));
	bool bRejected{};
	try
	{
		for (unsigned Step = 0; Step < 8; ++Step)
		{
			Invalid.Poll();
		}
	}
	catch (const FTransportError&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

class FRejectAccess final : public IAutomationAccessPolicy
{
public:
	bool Admit(const FTransportPeer&) const override
	{
		return false;
	}
};

void Admission()
{
	FTransportRegistry Transports;
	Transports.Register(MakeMemoryTransport());
	FOperationCatalog Catalog;
	Catalog.Seal();
	FCurrentUserAccessPolicy Allowed;
	FRejectAccess Rejected;
	FDiscovery Discovery;
	FAutomationTarget Target{CreateAutomationIdentity(), "Test", "test", {"memory", "admission"}};
	FAutomationServer Server(Catalog, Target, Transports.Listen(Target.Address), Allowed);
	FConnectionManager Client(Transports, Discovery, Rejected);
	const auto Result =
	    Await(Client.Connect(ParseJson(R"({"address":{"scheme":"memory","address":"admission"}})")), Client, Server);
	HYP_CHECK(ReadValue<std::string>(Fields(Fields(Result).at("error")).at("code")) == "access_denied");
	Server.StopAdmission();
	FAutomationServer Closed(Catalog, Target, Transports.Listen(Target.Address), Rejected);
	FConnectionManager Peer(Transports, Discovery, Allowed);
	const auto Denied =
	    Await(Peer.Connect(ParseJson(R"({"address":{"scheme":"memory","address":"admission"}})")), Peer, Closed);
	HYP_CHECK(ReadValue<std::string>(Fields(Denied).at("status")) == "failed");
	HYP_CHECK(Closed.PendingCount() == 0);
}

void DrainShutdownReply()
{
	FTransportRegistry Transports;
	Transports.Register(MakeMemoryTransport(128, 3));
	FCurrentUserAccessPolicy Access;
	FDiscovery Discovery;
	FAutomationTarget Target{CreateAutomationIdentity(), "Test", "test", {"memory", "shutdown-reply"}};
	FOperationCatalog Catalog;
	bool bHandled{};
	FOperationInfo Info{"test.close",
	                    "Close",
	                    "Shutdown reply drain",
	                    "test",
	                    "Requests exit",
	                    "Accepted",
	                    WriteRecordWire(RecordType<FAutomationTarget>(), &Target)};
	Catalog.Register(MakeOperation<FAutomationTarget, FAutomationTarget>(std::move(Info),
	                                                                     [&](const auto& InRequest)
	                                                                     {
		                                                                     bHandled = true;
		                                                                     return InRequest;
	                                                                     }));
	Catalog.Seal();
	FAutomationServer Server(Catalog, Target, Transports.Listen(Target.Address), Access);
	FConnectionManager Client(Transports, Discovery, Access);
	const auto Hello = Await(Client.Connect(ParseJson(R"({"address":{"scheme":"memory","address":"shutdown-reply"}})")),
	                         Client, Server);
	const auto Connection = ReadValue<std::string>(Fields(Fields(Hello).at("result")).at("connection"));
	auto Reply = Client.Request(
	    Connection, "api.call",
	    FArchiveNode(FArchiveNode::FObject{{"operation", WriteValue(std::string("test.close"))},
	                                       {"arguments", WriteRecordWire(RecordType<FAutomationTarget>(), &Target)}}));
	for (unsigned Step = 0; !bHandled && Step < 20000; ++Step)
	{
		Client.Poll();
		Server.Poll();
	}
	HYP_CHECK(bHandled);
	Server.StopAdmission();
	HYP_CHECK(Server.PendingCount() > 0);
	const auto Result = Await(std::move(Reply), Client, Server);
	HYP_CHECK(ReadValue<std::string>(Fields(Result).at("status")) == "completed");
	for (unsigned Step = 0; Server.PendingCount() && Step < 100; ++Step)
	{
		Server.Poll();
		Client.Poll();
	}
	HYP_CHECK(Server.PendingCount() == 0);
}

void DrainDisconnectedWork()
{
	FTransportRegistry Transports;
	Transports.Register(MakeMemoryTransport());
	FCurrentUserAccessPolicy Access;
	FDiscovery Discovery;
	FAutomationTarget Target{CreateAutomationIdentity(), "Test", "test", {"memory", "drain"}};
	FOperationCatalog Catalog;
	bool bComplete{};
	unsigned Completions{};
	FOperationInfo Info{"test.pending",
	                    "Pending work",
	                    "Work retained after disconnect",
	                    "test",
	                    "Completes work",
	                    "Completion recorded",
	                    WriteRecordWire(RecordType<FAutomationTarget>(), &Target)};
	Catalog.Register(MakeAsyncOperation<FAutomationTarget, FAutomationTarget>(
	    std::move(Info),
	    [&](const FAutomationTarget& InRequest) -> TPendingOperation<FAutomationTarget>
	    {
		    return {[&, Request = InRequest]() -> std::optional<FAutomationTarget>
		            {
			            if (!bComplete)
			            {
				            return {};
			            }
			            ++Completions;
			            return Request;
		            }};
	    }));
	Catalog.Seal();
	FAutomationServer Server(Catalog, Target, Transports.Listen(Target.Address), Access);
	FConnectionManager Client(Transports, Discovery, Access);
	const auto Hello =
	    Await(Client.Connect(ParseJson(R"({"address":{"scheme":"memory","address":"drain"}})")), Client, Server);
	const auto Connection = ReadValue<std::string>(Fields(Fields(Hello).at("result")).at("connection"));
	const auto Job =
	    Await(Client.Request(Connection, "api.call",
	                         FArchiveNode(FArchiveNode::FObject{
	                             {"operation", WriteValue(std::string("test.pending"))},
	                             {"arguments", WriteRecordWire(RecordType<FAutomationTarget>(), &Target)}})),
	          Client, Server);
	HYP_CHECK(ReadValue<std::string>(Fields(Job).at("status")) == "running");
	Client.Disconnect(Connection);
	Server.Poll();
	HYP_CHECK(Server.PendingCount() == 1 && Completions == 0);
	Server.StopAdmission();
	bComplete = true;
	Server.Poll();
	Server.Poll();
	HYP_CHECK(Server.PendingCount() == 0 && Completions == 1);
}
} // namespace
} // namespace Hyperion

int main()
{
	try
	{
		Hyperion::Connections();
		Hyperion::Framing();
		Hyperion::Admission();
		Hyperion::DrainDisconnectedWork();
		Hyperion::DrainShutdownReply();
		std::cout << "Automation connection contracts passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
