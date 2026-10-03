#include "ImportStateReflection.h"
#include "Hyperion/AssetImport/ImportWorkspace.h"

namespace Hyperion
{
namespace
{
template<class T> struct TImportStateName
{
	T State;
	std::string_view Name;
};

constexpr std::array TaskStates{TImportStateName<EImportTaskState>{EImportTaskState::Running, "running"},
                                TImportStateName<EImportTaskState>{EImportTaskState::Completed, "completed"},
                                TImportStateName<EImportTaskState>{EImportTaskState::Failed, "failed"}};

constexpr std::array DraftStates{TImportStateName<EImportDraftState>{EImportDraftState::None, ""},
                                 TImportStateName<EImportDraftState>{EImportDraftState::Preparing, "preparing"},
                                 TImportStateName<EImportDraftState>{EImportDraftState::Ready, "ready"},
                                 TImportStateName<EImportDraftState>{EImportDraftState::Publishing, "publishing"},
                                 TImportStateName<EImportDraftState>{EImportDraftState::Failed, "failed"},
                                 TImportStateName<EImportDraftState>{EImportDraftState::Discarded, "discarded"}};

template<class T, class S>
FRecordMember StateMember(S T::* InMember, std::span<const TImportStateName<S>> InStates,
                          FRecordMemberOptions InOptions = {})
{
	auto Result = Member("status", InMember, std::move(InOptions));
	// Preserve the established string shape and the typed C++ member association.
	Result.Shape = &RecordValueShape<std::string>;
	Result.Write = [InMember, InStates](const void* InObject)
	{
		for (const auto& Entry : InStates)
		{
			if (Entry.State == static_cast<const T*>(InObject)->*InMember)
			{
				return WriteValue(std::string(Entry.Name));
			}
		}
		throw std::invalid_argument("Invalid import lifecycle state");
	};
	Result.Read = [InMember, InStates](void* InObject, const FArchiveNode& InNode, const FRecordReadContext& InContext)
	{
		const auto Name = ReadValue<std::string>(InNode, InContext);
		for (const auto& Entry : InStates)
		{
			if (Entry.Name == Name)
			{
				static_cast<T*>(InObject)->*InMember = Entry.State;
				return;
			}
		}
		throw std::invalid_argument(InContext.Path + ": unknown import lifecycle state: " + Name);
	};
	return Result;
}
} // namespace

FRecordMember ImportTaskStateMember(FRecordMemberOptions InOptions)
{
	return StateMember<FImportTaskInfo, EImportTaskState>(&FImportTaskInfo::Status, TaskStates, std::move(InOptions));
}

FRecordMember ImportDraftStateMember()
{
	return StateMember<FImportDraftInfo, EImportDraftState>(&FImportDraftInfo::Status, DraftStates);
}
} // namespace Hyperion
