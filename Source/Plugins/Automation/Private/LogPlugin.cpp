#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include "Hyperion/Core/Logging/LogHistory.h"

namespace Hyperion
{
template<> std::span<const TRecordEnumEntry<ELogLevel>> RecordEnumEntries<ELogLevel>()
{
	static constexpr TRecordEnumEntry<ELogLevel> Entries[] = {
	    {ELogLevel::Info, "info", "Informational output"},
	    {ELogLevel::Warning, "warning", "Warning diagnostic"},
	    {ELogLevel::Error, "error", "Error diagnostic; raw stderr uses this level"},
	    {ELogLevel::Debug, "debug", "Debug diagnostic"}};
	return Entries;
}

template<> const FRecordDescriptor& RecordType<FLogEntry>()
{
	static const auto Type = MakeRecord<FLogEntry>(
	    "automation.log.entry",
	    {Member("sequence", &FLogEntry::Sequence, {.Description = "One-based row sequence within the current process"}),
	     Member("time", &FLogEntry::Time, {.Description = "Local capture time HH:MM:SS.mmm"}),
	     Member("thread", &FLogEntry::Thread,
	            {.Description = "Logging thread token; standard streams use the reader thread"}),
	     Member("level", &FLogEntry::Level),
	     Member("source", &FLogEntry::Source, {.Description = "engine, stdout or stderr"}),
	     Member("message", &FLogEntry::Message, {.Description = "Original line fragment, at most 8192 UTF-8 bytes"})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FLogReadRequest>()
{
	static const auto Type = MakeRecord<FLogReadRequest>(
	    "automation.log.read", {Member("after", &FLogReadRequest::After,
	                                   {.Description = "Last seen sequence; zero reads from process startup"}),
	                            Member("limit", &FLogReadRequest::Limit,
	                                   {.Description = "Maximum rows, 1 through 256; byte budget may return fewer"})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FLogPage>()
{
	static const auto Type = MakeRecord<FLogPage>(
	    "automation.log.page",
	    {Member("entries", &FLogPage::Entries),
	     Member("next", &FLogPage::Next,
	            {.Description = "Use as after for the next read, including subsequent new output"}),
	     Member("total", &FLogPage::Total, {.Description = "Total current-process rows at read time"})});
	return Type;
}

namespace
{
class FLogAutomationPlugin final : public FPlugin
{
public:
	void Start(FPluginContext& InContext) override
	{
		auto& Catalog = InContext.Require<FOperationCatalog>();
		InContext.Defer(
		    [&Catalog]
		    {
			    Catalog.UnregisterOwner("automation-log");
		    });
		auto* History = InContext.Find<FLogHistory>();
		FOperationInfo Info;
		Info.Id = "application.log.read";
		Info.Owner = "automation-log";
		Info.Summary = "Read current-process Editor log history";
		Info.Description =
		    "Reads the same disk-backed history as Window > Log, including startup and closed-panel output. "
		    "Rows retain severity and source. Long lines are split at UTF-8 boundaries. No previous-run records.";
		Info.Effects = "Read-only; no scene, asset, layout or history mutation.";
		Info.Completion = "Bounded page copied from the current log history.";
		Info.bReadOnly = true;
		Info.Unavailable = History ? "" : "This host has no current-process log history.";
		const FLogReadRequest Example;
		Info.Example = WriteRecordWire(RecordType<FLogReadRequest>(), &Example);
		Catalog.Register(MakeOperation<FLogReadRequest, FLogPage>(std::move(Info),
		                                                          [History](const FLogReadRequest& InRequest)
		                                                          {
			                                                          return History->Read(InRequest);
		                                                          }));
	}
};
} // namespace

void RegisterLogAutomation(FPluginRegistry& InRegistry)
{
	FPluginDescriptor Descriptor;
	Descriptor.Id = "automation-log";
	Descriptor.Dependencies = {"automation-catalog"};
	Descriptor.Before = {"automation-session"};
	Descriptor.Requires = {typeid(FOperationCatalog)};
	Descriptor.Optional = {typeid(FLogHistory)};
	Descriptor.Create = []
	{
		return std::make_unique<FLogAutomationPlugin>();
	};
	InRegistry.Add(std::move(Descriptor));
}
} // namespace Hyperion
