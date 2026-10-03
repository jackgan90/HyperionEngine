#include "Hyperion/Automation/Response.h"
#include "Hyperion/Automation/Operation.h"
#include <array>
#include <limits>

namespace Hyperion
{
namespace
{
struct FStatusName
{
	EAutomationStatus Status;
	std::string_view Name;
};

constexpr std::array StatusNames{
    FStatusName{EAutomationStatus::Running, "running"}, FStatusName{EAutomationStatus::Completed, "completed"},
    FStatusName{EAutomationStatus::Failed, "failed"}, FStatusName{EAutomationStatus::Cancelled, "cancelled"}};
constexpr std::array<std::string_view, 8> EnvelopeKeys{"status",    "result",      "error",       "job",
                                                       "operation", "cancellable", "pollAfterMs", "outcome"};

[[noreturn]] void InvalidResponse(std::string_view InPath)
{
	throw FAutomationError("protocol_error", "Invalid automation response envelope", std::string(InPath));
}

const FArchiveNode::FObject& Fields(const FArchiveNode& InValue)
{
	const auto* Object = std::get_if<FArchiveNode::FObject>(&InValue.Value);
	if (!Object)
	{
		InvalidResponse("response");
	}
	return *Object;
}

const FArchiveNode& Required(const FArchiveNode::FObject& InFields, std::string_view InKey)
{
	const auto It = InFields.find(std::string(InKey));
	if (It == InFields.end())
	{
		InvalidResponse(InKey);
	}
	return It->second;
}

std::string_view String(const FArchiveNode::FObject& InFields, std::string_view InKey)
{
	const auto* Value = std::get_if<std::string>(&Required(InFields, InKey).Value);
	if (!Value)
	{
		InvalidResponse(InKey);
	}
	return *Value;
}

void CheckShape(const FArchiveNode::FObject& InFields, std::initializer_list<std::string_view> InAllowed)
{
	for (const auto Key : EnvelopeKeys)
	{
		if (InFields.contains(std::string(Key)) &&
		    std::find(InAllowed.begin(), InAllowed.end(), Key) == InAllowed.end())
		{
			InvalidResponse(Key);
		}
	}
}

EAutomationStatus ReadStatus(const FArchiveNode::FObject& InFields)
{
	const auto Name = String(InFields, "status");
	for (const auto& Entry : StatusNames)
	{
		if (Entry.Name == Name)
		{
			return Entry.Status;
		}
	}
	InvalidResponse("status");
}

FArchiveNode WriteStatus(EAutomationStatus InStatus)
{
	for (const auto& Entry : StatusNames)
	{
		if (Entry.Status == InStatus)
		{
			return WriteValue(std::string(Entry.Name));
		}
	}
	throw std::invalid_argument("Invalid automation response status");
}

FAutomationResponseView ReadDirect(const FArchiveNode::FObject& InFields, EAutomationStatus InStatus)
{
	FAutomationResponseView View;
	View.Status = InStatus;
	if (InStatus == EAutomationStatus::Completed)
	{
		CheckShape(InFields, {"status", "result"});
		View.Result = &Required(InFields, "result");
	}
	else if (InStatus == EAutomationStatus::Failed)
	{
		CheckShape(InFields, {"status", "error"});
		const auto& Error = Fields(Required(InFields, "error"));
		View.Error = FAutomationFailureView{String(Error, "code"), String(Error, "message"), String(Error, "path"),
		                                    &Required(Error, "details")};
	}
	else
	{
		InvalidResponse("job");
	}
	return View;
}

std::uint32_t ReadPollDelay(const FArchiveNode::FObject& InFields)
{
	const auto& Node = Required(InFields, "pollAfterMs");
	std::uint64_t Value{};
	if (const auto* Unsigned = std::get_if<std::uint64_t>(&Node.Value))
	{
		Value = *Unsigned;
	}
	else if (const auto* Signed = std::get_if<std::int64_t>(&Node.Value); Signed && *Signed > 0)
	{
		Value = static_cast<std::uint64_t>(*Signed);
	}
	if (!Value || Value > std::numeric_limits<std::uint32_t>::max())
	{
		InvalidResponse("pollAfterMs");
	}
	return static_cast<std::uint32_t>(Value);
}

FAutomationResponseView ReadJob(const FArchiveNode::FObject& InFields, EAutomationStatus InStatus)
{
	const auto* bCancellable = std::get_if<bool>(&Required(InFields, "cancellable").Value);
	if (!bCancellable)
	{
		InvalidResponse("cancellable");
	}
	FAutomationJobView Job{String(InFields, "job"), String(InFields, "operation"), *bCancellable};
	if (Job.Id.empty() || Job.Operation.empty())
	{
		InvalidResponse(Job.Id.empty() ? "job" : "operation");
	}
	if (InStatus == EAutomationStatus::Running)
	{
		CheckShape(InFields, {"status", "job", "operation", "cancellable", "pollAfterMs"});
		Job.PollAfterMs = ReadPollDelay(InFields);
	}
	else
	{
		CheckShape(InFields, {"status", "job", "operation", "cancellable", "outcome"});
		if (Job.bCancellable)
		{
			InvalidResponse("cancellable");
		}
		Job.Outcome = &Required(InFields, "outcome");
		const auto& OutcomeFields = Fields(*Job.Outcome);
		const auto Outcome = ReadDirect(OutcomeFields, ReadStatus(OutcomeFields));
		const auto Expected =
		    InStatus == EAutomationStatus::Completed ? EAutomationStatus::Completed : EAutomationStatus::Failed;
		if (Outcome.Status != Expected ||
		    (InStatus == EAutomationStatus::Cancelled && Outcome.Error->Code != "cancelled"))
		{
			InvalidResponse("outcome");
		}
	}
	FAutomationResponseView View;
	View.Status = InStatus;
	View.Job = Job;
	return View;
}
} // namespace

bool FAutomationResponseView::IsFailed() const
{
	return Status == EAutomationStatus::Failed;
}

const FArchiveNode& FAutomationResponseView::CompletedResult() const
{
	if (Status != EAutomationStatus::Completed || Job || !Result)
	{
		InvalidResponse("result");
	}
	return *Result;
}

FAutomationResponseView ReadAutomationResponse(const FArchiveNode& InResponse)
{
	const auto& Object = Fields(InResponse);
	if (!Object.contains("status"))
	{
		CheckShape(Object, {});
		FAutomationResponseView View;
		View.Result = &InResponse;
		return View;
	}
	const auto Status = ReadStatus(Object);
	return Object.contains("job") ? ReadJob(Object, Status) : ReadDirect(Object, Status);
}

FArchiveNode AutomationCompleted(FArchiveNode InResult)
{
	return FArchiveNode(
	    FArchiveNode::FObject{{"status", WriteStatus(EAutomationStatus::Completed)}, {"result", std::move(InResult)}});
}

FArchiveNode AutomationFailure(std::string InCode, std::string InMessage, std::string InPath, FArchiveNode InDetails)
{
	return FArchiveNode(
	    FArchiveNode::FObject{{"status", WriteStatus(EAutomationStatus::Failed)},
	                          {"error", FArchiveNode(FArchiveNode::FObject{{"code", WriteValue(InCode)},
	                                                                       {"message", WriteValue(InMessage)},
	                                                                       {"path", WriteValue(InPath)},
	                                                                       {"details", std::move(InDetails)}})}});
}

FArchiveNode AutomationJobResponse(EAutomationStatus InStatus, std::string_view InJob, std::string_view InOperation,
                                   bool bInCancellable, const FArchiveNode& InOutcome)
{
	FArchiveNode::FObject Object{{"status", WriteStatus(InStatus)},
	                             {"job", WriteValue(std::string(InJob))},
	                             {"operation", WriteValue(std::string(InOperation))},
	                             {"cancellable", WriteValue(InStatus == EAutomationStatus::Running && bInCancellable)}};
	if (InStatus == EAutomationStatus::Running)
	{
		Object.emplace("pollAfterMs", WriteValue(std::uint32_t(20)));
	}
	else
	{
		Object.emplace("outcome", InOutcome);
	}
	FArchiveNode Response(std::move(Object));
	(void)ReadAutomationResponse(Response);
	return Response;
}
} // namespace Hyperion
