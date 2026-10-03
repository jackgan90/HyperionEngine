#include "Hyperion/Automation/Connections.h"
#include "Hyperion/Transport/MemoryTransport.h"
#include "Support/TestSupport.h"

namespace Hyperion
{
namespace
{
class FEmptyDiscovery final : public ITargetDiscovery
{
public:
	FTargetDiscoverySnapshot List() override
	{
		return {};
	}

	std::optional<FAutomationTarget> Find(const std::string&) override
	{
		return {};
	}
};

struct FReplyFixture
{
	FTransportRegistry Transports;
	FEmptyDiscovery Discovery;
	FCurrentUserAccessPolicy Access;
	std::unique_ptr<ITransportListener> Listener;
	std::unique_ptr<FConnectionManager> Manager;
	std::unique_ptr<FAutomationChannel> Peer;
	FEndpointRequest Hello;

	explicit FReplyFixture(std::string_view InExpectedInstance = {})
	{
		Transports.Register(MakeMemoryTransport(128, 3));
		Listener = Transports.Listen({"memory", "response-peer"});
		Manager = std::make_unique<FConnectionManager>(Transports, Discovery, Access);
		auto Parameters = ParseJson(R"({"address":{"scheme":"memory","address":"response-peer"}})");
		if (!InExpectedInstance.empty())
		{
			std::get<FArchiveNode::FObject>(Parameters.Value)
			    .emplace("instance", WriteValue(std::string(InExpectedInstance)));
		}
		Hello = Manager->Connect(Parameters);
		Peer = std::make_unique<FAutomationChannel>(Listener->Accept());
	}

	FArchiveNode Receive()
	{
		for (unsigned Step = 0; Step < 20000; ++Step)
		{
			Manager->Poll();
			Peer->Poll();
			if (const auto Message = Peer->Receive())
			{
				return ParseJson(*Message);
			}
		}
		throw std::runtime_error("Expected a peer request");
	}

	FArchiveNode Await(FEndpointRequest& InRequest)
	{
		for (unsigned Step = 0; Step < 20000; ++Step)
		{
			Peer->Poll();
			Manager->Poll();
			if (auto Reply = InRequest.Poll())
			{
				return *Reply;
			}
		}
		throw std::runtime_error("Expected a connection response");
	}

	void Reply(const FArchiveNode& InRequest, FArchiveNode InValue)
	{
		Peer->Send(WriteAutomationResponse(FArchiveNode(FArchiveNode::FObject{
		    {"id", std::get<FArchiveNode::FObject>(InRequest.Value).at("id")}, {"result", std::move(InValue)}})));
	}

	static FArchiveNode ValidHello()
	{
		const FAutomationTarget Target{"test-instance", "Test", "test-build", {"memory", "response-peer"}};
		return AutomationCompleted(FArchiveNode(FArchiveNode::FObject{
		    {"protocol", WriteValue(std::uint32_t(1))},
		    {"maxFrame", WriteValue(static_cast<std::uint32_t>(AutomationResponseLimits.MaxBytes))},
		    {"target", WriteRecordWire(RecordType<FAutomationTarget>(), &Target)}}));
	}

	std::string Connect()
	{
		const auto Request = Receive();
		Reply(Request, ValidHello());
		const auto Response = Await(Hello);
		const auto& Payload = ReadAutomationResponse(Response).CompletedResult();
		return ReadValue<std::string>(std::get<FArchiveNode::FObject>(Payload.Value).at("connection"));
	}
};

void CheckRejectedHello(FArchiveNode InReply, std::string_view InCode, std::string_view InExpectedInstance = {})
{
	FReplyFixture Fixture(InExpectedInstance);
	const auto Request = Fixture.Receive();
	Fixture.Reply(Request, std::move(InReply));
	const auto Response = Fixture.Await(Fixture.Hello);
	const auto View = ReadAutomationResponse(Response);
	HYP_CHECK(View.IsFailed() && View.Error && View.Error->Code == InCode);
	Fixture.Peer->Poll();
	HYP_CHECK(Fixture.Peer->State() == ETransportState::Closed && !Fixture.Listener->Accept());
}

void CheckBadHandshakes()
{
	for (const auto Invalid :
	     {R"({"status":"completed"})", R"({"protocol":1,"maxFrame":1024})",
	      R"({"status":"running","job":"j","operation":"op","cancellable":false,"pollAfterMs":20})",
	      R"({"status":"completed","result":null})", R"({"status":"completed","result":[]})",
	      R"({"status":"completed","result":{}})",
	      R"({"status":"completed","result":{"protocol":"x","maxFrame":1024}})",
	      R"({"status":"completed","result":{"protocol":4294967296,"maxFrame":1024}})",
	      R"({"status":"completed","result":{"protocol":1,"maxFrame":false}})",
	      R"({"status":"completed","result":{"protocol":1,"maxFrame":-1}})",
	      R"({"status":"completed","result":{"protocol":1}})",
	      R"({"status":"completed","result":{"protocol":1,"maxFrame":1024}})",
	      R"({"status":"completed","result":{"protocol":1,"maxFrame":1024,"target":null}})",
	      R"({"status":"completed","result":{"protocol":1,"maxFrame":1024,"target":{"instance":42}}})"})
	{
		CheckRejectedHello(ParseJson(Invalid), "protocol_error");
	}
	FReplyFixture Fixture;
	const auto Request = Fixture.Receive();
	const auto Failure = AutomationFailure("access_denied", "peer reason", "/peer", ParseJson(R"({"fact":true})"));
	Fixture.Reply(Request, Failure);
	HYP_CHECK(WriteJson(Fixture.Await(Fixture.Hello)) == WriteJson(Failure));
}

void CheckHandshakePolicy()
{
	auto VersionMismatch = FReplyFixture::ValidHello();
	std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(VersionMismatch.Value).at("result").Value)
	    .at("protocol") = WriteValue(std::uint32_t(2));
	CheckRejectedHello(std::move(VersionMismatch), "protocol_mismatch");
	auto BudgetMismatch = FReplyFixture::ValidHello();
	std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(BudgetMismatch.Value).at("result").Value)
	    .at("maxFrame") = WriteValue(static_cast<std::uint32_t>(AutomationResponseLimits.MaxBytes + 1));
	CheckRejectedHello(std::move(BudgetMismatch), "protocol_mismatch");
	CheckRejectedHello(FReplyFixture::ValidHello(), "stale_target", "old-instance");
}

void CheckBadRoutedReply()
{
	FReplyFixture Fixture;
	const auto Connection = Fixture.Connect();
	auto First = Fixture.Manager->Request(Connection, "api.call", ParseJson(R"({"operation":"mutation.one"})"));
	auto Second = Fixture.Manager->Request(Connection, "api.call", ParseJson(R"({"operation":"mutation.two"})"));
	const auto FirstWire = Fixture.Receive();
	const auto SecondWire = Fixture.Receive();
	HYP_CHECK(WriteJson(FirstWire) != WriteJson(SecondWire));
	Fixture.Reply(FirstWire, ParseJson(R"({"status":"failed","error":{"code":"missing-other-fields"}})"));
	const auto FirstResult = Fixture.Await(First);
	const auto SecondResult = Fixture.Await(Second);
	HYP_CHECK(ReadAutomationResponse(FirstResult).Error->Code == "protocol_error");
	HYP_CHECK(WriteJson(FirstResult) == WriteJson(SecondResult));
	HYP_CHECK(!First.Poll() && !Second.Poll());
	bool bRejected{};
	try
	{
		(void)Fixture.Manager->Request(Connection, "api.call", ParseJson("{}"));
	}
	catch (const FAutomationError& Error)
	{
		bRejected = Error.Code == "disconnected";
	}
	HYP_CHECK(bRejected && !Fixture.Peer->Receive() && !Fixture.Listener->Accept());
}
} // namespace

void RunResponseConnectionTests()
{
	CheckBadHandshakes();
	CheckHandshakePolicy();
	CheckBadRoutedReply();
}
} // namespace Hyperion
