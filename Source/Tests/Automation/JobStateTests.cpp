#include "Hyperion/Automation/Session.h"
#include "Support/TestSupport.h"

namespace Hyperion
{
struct FJobStateTestValue
{
	bool bCancellable{};
};

template<> const FRecordDescriptor& RecordType<FJobStateTestValue>()
{
	static const auto Type =
	    MakeRecord<FJobStateTestValue>("test.job-state", {Member("cancellable", &FJobStateTestValue::bCancellable)});
	return Type;
}
} // namespace Hyperion

namespace
{
using namespace Hyperion;

const FArchiveNode& Field(const FArchiveNode& InValue, const char* InKey)
{
	return std::get<FArchiveNode::FObject>(InValue.Value).at(InKey);
}

std::string Text(const FArchiveNode& InValue, const char* InKey)
{
	return ReadValue<std::string>(Field(InValue, InKey));
}

enum class ECompletion
{
	Pending,
	Success,
	Failure
};

struct FJobFixture
{
	FOperationCatalog Catalog;
	ECompletion Completion = ECompletion::Pending;
	unsigned Polls{};
	unsigned Cancellations{};

	FJobFixture()
	{
		FOperationInfo Info{"test.job-state", "Job state", "Controlled completion",
		                    "test",           "No writes", "Complete after test signal",
		                    ParseJson("{}")};
		Catalog.Register(MakeAsyncOperation<FJobStateTestValue, FJobStateTestValue>(
		    Info,
		    [&](const FJobStateTestValue& InRequest)
		    {
			    TPendingOperation<FJobStateTestValue> Pending;
			    Pending.Poll = [&]() -> std::optional<FJobStateTestValue>
			    {
				    ++Polls;
				    if (Completion == ECompletion::Failure)
				    {
					    throw FAutomationError("test_failure", "Controlled failure");
				    }
				    return Completion == ECompletion::Success ? std::optional(FJobStateTestValue{}) : std::nullopt;
			    };
			    if (InRequest.bCancellable)
			    {
				    Pending.Cancel = [&]
				    {
					    ++Cancellations;
				    };
			    }
			    return Pending;
		    }));
		Catalog.Seal();
	}

	FArchiveNode Start(FAutomationSession& InSession, bool bInCancellable = false)
	{
		const FJobStateTestValue Request{bInCancellable};
		return InSession.Call("test.job-state", WriteRecordWire(RecordType<FJobStateTestValue>(), &Request));
	}
};

void CheckTerminalStates()
{
	FJobFixture Fixture;
	FAutomationSession Session(Fixture.Catalog, {1, 2});
	const auto Running = Fixture.Start(Session);
	const auto First = Text(Running, "job");
	HYP_CHECK(Text(Running, "status") == "running" && Session.PendingCount() == 1);
	HYP_CHECK(!ReadValue<bool>(Field(Running, "cancellable")));
	HYP_CHECK(ReadInteger<unsigned>(Field(Running, "pollAfterMs")) == 20);
	Fixture.Completion = ECompletion::Success;
	const auto Completed = Session.GetJob(First);
	HYP_CHECK(Text(Completed, "status") == "completed" && Session.PendingCount() == 0);
	HYP_CHECK(Text(Field(Completed, "outcome"), "status") == "completed");
	HYP_CHECK(!std::get<FArchiveNode::FObject>(Completed.Value).contains("pollAfterMs"));
	const auto Polls = Fixture.Polls;
	HYP_CHECK(Text(Session.CancelJob(First), "status") == "completed" && Fixture.Polls == Polls);
	const auto Second = Text(Fixture.Start(Session), "job");
	Fixture.Completion = ECompletion::Failure;
	const auto Failed = Session.GetJob(Second);
	HYP_CHECK(Text(Failed, "status") == "failed" && Session.PendingCount() == 0);
	HYP_CHECK(Text(Field(Field(Failed, "outcome"), "error"), "code") == "test_failure");
	Fixture.Completion = ECompletion::Pending;
	const auto Third = Text(Fixture.Start(Session, true), "job");
	HYP_CHECK(Text(Session.GetJob(Third), "status") == "running");
	bool bExpired{};
	try
	{
		(void)Session.GetJob(First);
	}
	catch (const FAutomationError& Error)
	{
		bExpired = Error.Code == "not_found";
	}
	HYP_CHECK(bExpired && Text(Session.GetJob(Second), "status") == "failed");
	const auto Cancelled = Session.CancelJob(Third);
	HYP_CHECK(Text(Cancelled, "status") == "cancelled" && Fixture.Cancellations == 1 && Session.PendingCount() == 0);
	HYP_CHECK(!ReadValue<bool>(Field(Cancelled, "cancellable")));
	HYP_CHECK(Text(Field(Field(Cancelled, "outcome"), "error"), "code") == "cancelled");
	const auto CancelPolls = Fixture.Polls;
	HYP_CHECK(Text(Session.CancelJob(Third), "status") == "cancelled");
	HYP_CHECK(Fixture.Polls == CancelPolls && Fixture.Cancellations == 1);
}

void CheckStoppedAdmission()
{
	FJobFixture Fixture;
	FAutomationSession Session(Fixture.Catalog);
	const auto Job = Text(Fixture.Start(Session), "job");
	bool bRejected{};
	try
	{
		(void)Session.CancelJob(Job);
	}
	catch (const FAutomationError& Error)
	{
		bRejected = Error.Code == "not_cancellable";
	}
	HYP_CHECK(bRejected && Session.PendingCount() == 1);
	Session.StopAdmission();
	HYP_CHECK(Text(Field(Fixture.Start(Session), "error"), "code") == "session_closed");
	Fixture.Completion = ECompletion::Success;
	HYP_CHECK(Text(Session.GetJob(Job), "status") == "completed" && Session.PendingCount() == 0);
}

void CheckSnapshot(FArchiveNode InValue, std::string_view InExpected)
{
	// Only the ephemeral job identity is normalized; every other field is a fixed compatibility expectation.
	auto& Fields = std::get<FArchiveNode::FObject>(InValue.Value);
	if (Fields.contains("job"))
	{
		Fields.at("job") = WriteValue(std::string("<job>"));
	}
	HYP_CHECK(WriteJson(InValue) == WriteJson(ParseJson(InExpected)));
}

void CheckResponseSnapshots()
{
	CheckSnapshot(
	    AutomationFailure("test_failure", "Controlled failure", "field", ParseJson(R"({"cause":1})")),
	    R"({"status":"failed","error":{"code":"test_failure","message":"Controlled failure","path":"field","details":{"cause":1}}})");
	FJobFixture Fixture;
	FAutomationSession Session(Fixture.Catalog);
	const auto Started = Fixture.Start(Session);
	const auto Job = Text(Started, "job");
	CheckSnapshot(
	    Started,
	    R"({"status":"running","job":"<job>","operation":"test.job-state","cancellable":false,"pollAfterMs":20})");
	Fixture.Completion = ECompletion::Success;
	const auto Completed = Session.GetJob(Job);
	CheckSnapshot(Field(Completed, "outcome"), R"({"status":"completed","result":{"cancellable":false}})");
	CheckSnapshot(
	    Completed,
	    R"({"status":"completed","job":"<job>","operation":"test.job-state","cancellable":false,"outcome":{"status":"completed","result":{"cancellable":false}}})");
	const auto FailedJob = Text(Fixture.Start(Session), "job");
	Fixture.Completion = ECompletion::Failure;
	CheckSnapshot(
	    Session.GetJob(FailedJob),
	    R"({"status":"failed","job":"<job>","operation":"test.job-state","cancellable":false,"outcome":{"status":"failed","error":{"code":"test_failure","message":"Controlled failure","path":"","details":{}}}})");
	Fixture.Completion = ECompletion::Pending;
	const auto Cancellable = Fixture.Start(Session, true);
	CheckSnapshot(
	    Cancellable,
	    R"({"status":"running","job":"<job>","operation":"test.job-state","cancellable":true,"pollAfterMs":20})");
	CheckSnapshot(
	    Session.CancelJob(Text(Cancellable, "job")),
	    R"({"status":"cancelled","job":"<job>","operation":"test.job-state","cancellable":false,"outcome":{"status":"failed","error":{"code":"cancelled","message":"Operation cancelled; provider cleanup is still drained at shutdown","path":"","details":{}}}})");
}
} // namespace

void CheckJobStates()
{
	CheckTerminalStates();
	CheckStoppedAdmission();
	CheckResponseSnapshots();
}
