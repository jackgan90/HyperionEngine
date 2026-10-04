#pragma once
#include "Hyperion/Automation/AutomationErrors.h"
#include "Hyperion/Reflection/Wire.h"

namespace Hyperion
{
enum class EAutomationStatus
{
	Running,
	Completed,
	Failed,
	Cancelled
};

struct FAutomationFailureView
{
	std::string_view Code;
	std::string_view Message;
	std::string_view Path;
	const FArchiveNode* Details{};
};

struct FAutomationJobView
{
	std::string_view Id;
	std::string_view Operation;
	bool bCancellable{};
	std::optional<std::uint32_t> PollAfterMs;
	const FArchiveNode* Outcome{};
};

// Borrows the source node. Keep that node alive and unchanged while using this view.
// An absent Status denotes an unwrapped bootstrap query, whose Result is the source itself.
struct FAutomationResponseView
{
	std::optional<EAutomationStatus> Status;
	const FArchiveNode* Result{};
	std::optional<FAutomationFailureView> Error;
	std::optional<FAutomationJobView> Job;

	bool IsFailed() const;
	const FArchiveNode& CompletedResult() const;
};

FArchiveNode AutomationCompleted(FArchiveNode InResult);
FArchiveNode AutomationFailure(FErrorCode InCode, std::string InMessage, std::string InPath = {},
                               FArchiveNode InDetails = FArchiveNode(FArchiveNode::FObject{}));
FArchiveNode AutomationJobResponse(EAutomationStatus InStatus, std::string_view InJob, std::string_view InOperation,
                                   bool bInCancellable, const FArchiveNode& InOutcome = {});

// Validates only the shared envelope; operation payloads and additive fields stay opaque.
// Invalid envelopes throw FAutomationError with code protocol_error.
FAutomationResponseView ReadAutomationResponse(const FArchiveNode& InResponse);
FAutomationResponseView ReadAutomationResponse(FArchiveNode&&) = delete;
FAutomationResponseView ReadAutomationResponse(const FArchiveNode&&) = delete;
} // namespace Hyperion
