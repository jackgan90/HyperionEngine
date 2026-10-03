#include "Hyperion/Automation/Endpoint.h"
#include "Support/TestSupport.h"
#include <array>

namespace
{
using namespace Hyperion;

void CheckValidResponses()
{
	const auto Raw = ParseJson(R"({"session":"s","operations":[]})");
	const auto RawView = ReadAutomationResponse(Raw);
	HYP_CHECK(!RawView.Status && RawView.Result == &Raw && !RawView.IsFailed());
	const auto Completed = AutomationCompleted(ParseJson(R"({"status":"user-defined","job":17})"));
	const auto View = ReadAutomationResponse(Completed);
	HYP_CHECK(View.Status == EAutomationStatus::Completed && !View.Job && !View.Error);
	HYP_CHECK(&View.CompletedResult() == &std::get<FArchiveNode::FObject>(Completed.Value).at("result"));
	HYP_CHECK(WriteJson(Completed) == R"({"result":{"job":17,"status":"user-defined"},"status":"completed"})");
	const auto Failure = AutomationFailure("specific", "message", "/a~1b", ParseJson(R"([null,{"extra":42}])"));
	const auto Error = ReadAutomationResponse(Failure);
	HYP_CHECK(Error.IsFailed() && Error.Error->Code == "specific" && Error.Error->Message == "message");
	HYP_CHECK(Error.Error->Path == "/a~1b" && WriteJson(*Error.Error->Details) == R"([null,{"extra":42}])");
	const auto Added = ParseJson(R"({"status":"completed","result":null,"future":{"status":false}})");
	HYP_CHECK(ReadAutomationResponse(Added).Status == EAutomationStatus::Completed);
	const auto EmptyError =
	    ParseJson(R"({"status":"failed","error":{"code":"","message":"","path":"","details":null,"future":true}})");
	HYP_CHECK(ReadAutomationResponse(EmptyError).IsFailed());
	const auto Running = AutomationJobResponse(EAutomationStatus::Running, "j", "op", true);
	const auto Job = ReadAutomationResponse(Running);
	HYP_CHECK(Job.Job->Id == "j" && Job.Job->Operation == "op" && Job.Job->bCancellable);
	HYP_CHECK(Job.Job->PollAfterMs == 20u && !Job.Job->Outcome);
	const auto Done = AutomationJobResponse(EAutomationStatus::Completed, "j", "op", true, Completed);
	HYP_CHECK(!ReadAutomationResponse(Done).Job->bCancellable);
	const auto Failed = AutomationJobResponse(EAutomationStatus::Failed, "j", "op", false, Failure);
	HYP_CHECK(ReadAutomationResponse(Failed).IsFailed());
	const auto Cancelled = AutomationJobResponse(EAutomationStatus::Cancelled, "j", "op", false,
	                                             AutomationFailure("cancelled", "cancelled"));
	const auto CancelView = ReadAutomationResponse(Cancelled);
	HYP_CHECK(!CancelView.IsFailed() && ReadAutomationResponse(*CancelView.Job->Outcome).IsFailed());
	for (const auto* NonDirect : {&Raw, &Running, &Done, &Failure})
	{
		bool bRejected{};
		try
		{
			(void)ReadAutomationResponse(*NonDirect).CompletedResult();
		}
		catch (const FAutomationError& Exception)
		{
			bRejected = Exception.Code == "protocol_error";
		}
		HYP_CHECK(bRejected);
	}
}

void CheckMalformedResponses()
{
	constexpr std::array Invalid{
	    R"(null)",
	    R"([])",
	    R"(true)",
	    R"({"result":{}})",
	    R"({"status":null})",
	    R"({"status":"future"})",
	    R"({"status":"completed"})",
	    R"({"status":"failed","error":null})",
	    R"({"status":"completed","result":{},"error":{}})",
	    R"({"status":"running"})",
	    R"({"status":"failed","error":{"code":1,"message":"","path":"","details":{}}})",
	    R"({"status":"failed","error":{"code":"x","message":"","details":{}}})",
	    R"({"status":"failed","error":{"code":"x","message":"","path":""}})",
	    R"({"status":"failed","error":{"code":"x","message":false,"path":"","details":{}}})",
	    R"({"status":"running","job":"","operation":"op","cancellable":false,"pollAfterMs":20})",
	    R"({"status":"running","job":"j","operation":"","cancellable":false,"pollAfterMs":20})",
	    R"({"status":"running","job":"j","operation":"op","cancellable":0,"pollAfterMs":20})",
	    R"({"status":"running","job":"j","operation":"op","cancellable":false,"pollAfterMs":0})",
	    R"({"status":"running","job":"j","operation":"op","cancellable":false,"pollAfterMs":-1})",
	    R"({"status":"running","job":"j","operation":"op","cancellable":false,"pollAfterMs":4294967296})",
	    R"({"status":"running","job":"j","operation":"op","cancellable":false,"pollAfterMs":1.5})",
	    R"({"status":"running","job":"j","operation":"op","cancellable":false,"pollAfterMs":20,"outcome":{}})",
	    R"({"status":"completed","job":"j","operation":"op","cancellable":true,"outcome":{"status":"completed","result":{}}})",
	    R"({"status":"completed","job":"j","operation":"op","cancellable":false,"pollAfterMs":20,"outcome":{"status":"completed","result":{}}})",
	    R"({"status":"failed","job":"j","operation":"op","cancellable":false,"outcome":{"status":"completed","result":{}}})",
	    R"({"status":"cancelled","job":"j","operation":"op","cancellable":false,"outcome":{"status":"failed","error":{"code":"other","message":"","path":"","details":{}}}})",
	    R"({"status":"completed","job":"j","operation":"op","cancellable":false,"outcome":{"status":"completed","job":"nested","operation":"op","cancellable":false,"outcome":{"status":"completed","result":{}}}})"};
	for (const auto Text : Invalid)
	{
		const auto Response = ParseJson(Text);
		bool bRejected{};
		try
		{
			(void)ReadAutomationResponse(Response);
		}
		catch (const FAutomationError& Error)
		{
			bRejected = Error.Code == "protocol_error" && !Error.Path.empty();
		}
		HYP_CHECK(bRejected);
	}
	bool bRejected{};
	try
	{
		(void)AutomationJobResponse(static_cast<EAutomationStatus>(999), "j", "op", false);
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}
} // namespace

void CheckResponseContracts()
{
	CheckValidResponses();
	CheckMalformedResponses();
}
