#include "Hyperion/AssetImport/ImportWorkspace.h"
#include "Hyperion/Reflection/Wire.h"
#include "Support/TestSupport.h"

namespace
{
using namespace Hyperion;

const FArchiveNode& Field(const FArchiveNode& InValue, const char* InKey)
{
	return std::get<FArchiveNode::FObject>(InValue.Value).at(InKey);
}

template<class TFunction> void Reject(TFunction InFunction)
{
	bool bRejected{};
	try
	{
		InFunction();
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

template<class T, class S> void CheckState(S InState, std::string_view InToken)
{
	T Value;
	Value.Status = InState;
	const auto& Type = RecordType<T>();
	const auto Wire = WriteRecordWire(Type, &Value);
	HYP_CHECK(ReadValue<std::string>(Field(Wire, "status")) == InToken);
	const auto Decoded = ReadRecordWire(Type, Wire);
	HYP_CHECK(static_cast<const T*>(Decoded.get())->Status == InState);
	const auto Restored = ReadRecord(Type, WriteRecord(Type, &Value));
	HYP_CHECK(static_cast<const T*>(Restored.get())->Status == InState);
	HYP_CHECK(ResolveRecordMember(Type, &T::Status).FieldId == "status");
}

template<class T> void CheckInvalid(std::initializer_list<std::string> InTokens)
{
	const auto& Type = RecordType<T>();
	for (const auto& Token : InTokens)
	{
		Reject(
		    [&]
		    {
			    (void)ReadRecordWire(Type, FArchiveNode(FArchiveNode::FObject{{"status", WriteValue(Token)}}));
		    });
	}
	T Invalid;
	Invalid.Status = static_cast<decltype(Invalid.Status)>(999);
	Reject(
	    [&]
	    {
		    (void)WriteRecordWire(Type, &Invalid);
	    });
}
} // namespace

void CheckImportStates()
{
	using namespace Hyperion;
	CheckState<FImportTaskInfo>(EImportTaskState::Running, "running");
	CheckState<FImportTaskInfo>(EImportTaskState::Completed, "completed");
	CheckState<FImportTaskInfo>(EImportTaskState::Failed, "failed");
	CheckState<FImportDraftInfo>(EImportDraftState::None, "");
	CheckState<FImportDraftInfo>(EImportDraftState::Preparing, "preparing");
	CheckState<FImportDraftInfo>(EImportDraftState::Ready, "ready");
	CheckState<FImportDraftInfo>(EImportDraftState::Publishing, "publishing");
	CheckState<FImportDraftInfo>(EImportDraftState::Failed, "failed");
	CheckState<FImportDraftInfo>(EImportDraftState::Discarded, "discarded");
	CheckInvalid<FImportTaskInfo>({"", "ready", "cancelled", "COMPLETED", "unknown"});
	CheckInvalid<FImportDraftInfo>({"running", "completed", "cancelled", "READY", "unknown"});
	const auto TaskSchema = RecordWireSchema(RecordType<FImportTaskInfo>());
	const auto DraftSchema = RecordWireSchema(RecordType<FImportDraftInfo>());
	HYP_CHECK(
	    WriteJson(Field(Field(TaskSchema, "properties"), "status")) ==
	    WriteJson(ParseJson(
	        R"({"type":"string","default":"running","description":"running, completed or failed. Accepted publication is not cancellable."})")));
	HYP_CHECK(WriteJson(Field(Field(DraftSchema, "properties"), "status")) ==
	          WriteJson(ParseJson(R"({"type":"string","default":""})")));
}
