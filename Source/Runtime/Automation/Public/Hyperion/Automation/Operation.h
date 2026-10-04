#pragma once
#include "Hyperion/Automation/AutomationErrors.h"
#include "Hyperion/Automation/Response.h"

namespace Hyperion
{
class FAutomationError : public FCodedError
{
public:
	FAutomationError(FErrorCode InCode, std::string InMessage, std::string InPath = {},
	                 FArchiveNode InDetails = FArchiveNode(FArchiveNode::FObject{}));
	std::string Path;
	FArchiveNode Details;
};

// The operation boundary keeps rich automation errors intact and forwards open domain codes.
template<class TFunction> decltype(auto) InvokeAutomation(TFunction&& InFunction)
{
	try
	{
		return std::forward<TFunction>(InFunction)();
	}
	catch (const FAutomationError&)
	{
		throw;
	}
	catch (const FCodedError& Error)
	{
		throw FAutomationError(Error.Code, Error.what());
	}
}

struct FOperationInfo
{
	std::string Id;
	std::string Summary;
	std::string Description;
	std::string Owner;
	std::string Effects;
	std::string Completion;
	FArchiveNode Example = FArchiveNode(FArchiveNode::FObject{});
	std::uint32_t Version = 1;
	bool bReadOnly{};
	std::vector<std::string> Keywords;
	// Unavailable providers remain discoverable without constructing their resources.
	std::string Unavailable;
};

template<class T> struct TPendingOperation
{
	// Main only. Captures own their request snapshots; provider state outlives pending jobs.
	std::function<std::optional<T>()> Poll;
	std::function<void()> Cancel;
};

struct FOperationTask
{
	std::optional<FArchiveNode> Completed;
	std::function<std::optional<FArchiveNode>()> Poll;
	std::function<void()> Cancel;
};

struct FOperationDescriptor
{
	FOperationInfo Info;
	const FRecordDescriptor* Request{};
	const FRecordDescriptor* Result{};
	bool bAsynchronous{};
	std::function<FOperationTask(const void*)> Invoke;
};

template<class TRequest, class TResult, class TFunction>
FOperationDescriptor MakeOperation(FOperationInfo InInfo, TFunction InFunction)
{
	return {std::move(InInfo), &RecordType<TRequest>(), &RecordType<TResult>(), false,
	        [Function = std::move(InFunction)](const void* InRequest)
	        {
		        const TResult Result = InvokeAutomation(
		            [&]
		            {
			            return Function(*static_cast<const TRequest*>(InRequest));
		            });
		        return FOperationTask{WriteRecordWire(RecordType<TResult>(), &Result)};
	        }};
}

template<class TRequest, class TResult, class TFunction>
FOperationDescriptor MakeAsyncOperation(FOperationInfo InInfo, TFunction InFunction)
{
	return {std::move(InInfo), &RecordType<TRequest>(), &RecordType<TResult>(), true,
	        [Function = std::move(InFunction)](const void* InRequest)
	        {
		        TPendingOperation<TResult> Pending = InvokeAutomation(
		            [&]
		            {
			            return Function(*static_cast<const TRequest*>(InRequest));
		            });
		        if (!Pending.Poll)
		        {
			        throw std::logic_error("Empty pending operation");
		        }
		        return FOperationTask{{},
		                              [Poll = std::move(Pending.Poll)]() -> std::optional<FArchiveNode>
		                              {
			                              if (const auto Result = InvokeAutomation(Poll))
			                              {
				                              return WriteRecordWire(RecordType<TResult>(), &*Result);
			                              }
			                              return {};
		                              },
		                              std::move(Pending.Cancel)};
	        }};
}

FArchiveNode CurrentAutomationFailure();
// Random 128-bit namespace for ephemeral handles; not an authentication token.
std::string CreateAutomationIdentity();
} // namespace Hyperion
