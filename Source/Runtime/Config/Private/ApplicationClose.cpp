#include "Hyperion/Config/ApplicationClose.h"

namespace Hyperion
{
template<> std::span<const TRecordEnumEntry<EApplicationCloseAction>> RecordEnumEntries<EApplicationCloseAction>()
{
	static constexpr TRecordEnumEntry<EApplicationCloseAction> Values[] = {
	    {EApplicationCloseAction::RejectDirty, "RejectDirty", "Refuse exit while documents are dirty."},
	    {EApplicationCloseAction::Save, "Save", "Save dirty documents, then exit."},
	    {EApplicationCloseAction::Discard, "Discard", "Explicitly discard unsaved changes and exit."},
	    {EApplicationCloseAction::Cancel, "Cancel", "Cancel pending exit; admitted saves continue."}};
	return Values;
}

template<> const FRecordDescriptor& RecordType<FApplicationCloseRequest>()
{
	static const auto Type = MakeRecord<FApplicationCloseRequest>(
	    "application.close.request",
	    {Member("action", &FApplicationCloseRequest::Action,
	            {.Description = "0: reject dirty (default); 1: save all then exit; 2: explicitly discard and exit; 3: "
	                            "cancel pending exit (does not cancel admitted saves)."}),
	     Member("scenePath", &FApplicationCloseRequest::ScenePath,
	            {.Description = "Save destination for a dirty untitled scene; otherwise the current path is used. "
	                            "Viewer Save uses its current scene path if omitted."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FApplicationCloseState>()
{
	static const auto Type = MakeRecord<FApplicationCloseState>(
	    "application.close.state",
	    {Member("state", &FApplicationCloseState::State,
	            {.Description = "idle, saving, failed or closing. Closing is acceptance, not proof of process exit; "
	                            "the target disconnects during normal shutdown."}),
	     Member("error", &FApplicationCloseState::Error), Member("dirty", &FApplicationCloseState::bDirty),
	     Member("busy", &FApplicationCloseState::bBusy), Member("scenePath", &FApplicationCloseState::ScenePath)});
	return Type;
}
} // namespace Hyperion
