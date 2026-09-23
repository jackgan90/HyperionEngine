#pragma once
#include "Hyperion/Reflection/Wire.h"

namespace Hyperion
{
class FAutomationError : public std::runtime_error
{
public:
	FAutomationError(std::string InCode, std::string InMessage, std::string InPath = {},
	                 FArchiveNode InDetails = FArchiveNode(FArchiveNode::FObject{}));
	std::string Code;
	std::string Path;
	FArchiveNode Details;
};

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
		        const TResult Result = Function(*static_cast<const TRequest*>(InRequest));
		        return FOperationTask{WriteRecordWire(RecordType<TResult>(), &Result)};
	        }};
}

template<class TRequest, class TResult, class TFunction>
FOperationDescriptor MakeAsyncOperation(FOperationInfo InInfo, TFunction InFunction)
{
	return {std::move(InInfo), &RecordType<TRequest>(), &RecordType<TResult>(), true,
	        [Function = std::move(InFunction)](const void* InRequest)
	        {
		        TPendingOperation<TResult> Pending = Function(*static_cast<const TRequest*>(InRequest));
		        if (!Pending.Poll)
		        {
			        throw std::logic_error("Empty pending operation");
		        }
		        return FOperationTask{{},
		                              [Poll = std::move(Pending.Poll)]() -> std::optional<FArchiveNode>
		                              {
			                              if (const auto Result = Poll())
			                              {
				                              return WriteRecordWire(RecordType<TResult>(), &*Result);
			                              }
			                              return {};
		                              },
		                              std::move(Pending.Cancel)};
	        }};
}

FArchiveNode AutomationFailure(std::string InCode, std::string InMessage, std::string InPath = {},
                               FArchiveNode InDetails = FArchiveNode(FArchiveNode::FObject{}));
FArchiveNode CurrentAutomationFailure();
// Random 128-bit namespace for ephemeral handles; not an authentication token.
std::string CreateAutomationIdentity();
} // namespace Hyperion
