#include "Hyperion/Config/ApplicationSettings.h"
#include "Hyperion/IO/Path.h"
#include "SceneOperations.h"

namespace Hyperion
{
namespace
{
struct FSettingsEdit
{
	std::uint64_t Revision{};
	FAppSettings Values;
};

struct FSettingsSave
{
	std::uint64_t Revision{};
	std::string Path;
};

struct FSettingsSaved
{
	bool bSaved{};
};
} // namespace

template<> const FRecordDescriptor& RecordType<FSettingsEdit>()
{
	static const auto Type = MakeRecord<FSettingsEdit>(
	    "automation.settings.edit", {Member("revision", &FSettingsEdit::Revision, {.bRequired = true}),
	                                 Member("values", &FSettingsEdit::Values, {.bRequired = true})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSettingsSave>()
{
	static const auto Type = MakeRecord<FSettingsSave>(
	    "automation.settings.save",
	    {Member("revision", &FSettingsSave::Revision, {.bRequired = true}),
	     Member(
	         "path", &FSettingsSave::Path,
	         {.Description = "Target-local configuration path; empty uses the application's startup config file."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSettingsSaved>()
{
	static const auto Type =
	    MakeRecord<FSettingsSaved>("automation.settings.saved", {Member("saved", &FSettingsSaved::bSaved)});
	return Type;
}

void RegisterApplicationSettings(FOperationCatalog& InCatalog, IApplicationSettings* InSettings)
{
	FOperationInfo Info;
	Info.Id = "application.settings.get";
	Info.Owner = "automation-scene";
	Info.Summary = "Read Viewer render and startup settings";
	Info.Description = "Shares the GUI/persistence property inventory. Fields labelled restart affect the next launch; "
	                   "activeReversedZ reports the actual depth convention.";
	Info.bReadOnly = true;
	Info.Effects = "Reads current configuration.";
	Info.Completion = "Current Main snapshot.";
	Info.Unavailable = InSettings ? "" : "This host does not use Viewer application settings.";
	InCatalog.Register(
	    MakeOperation<FSceneInfoRequest, FApplicationSettingsState>(Info,
	                                                                [InSettings](const auto&)
	                                                                {
		                                                                return InSettings->ApplicationSettings();
	                                                                }));
	Info.Id = "application.settings.set";
	Info.Summary = "Replace Viewer render and startup settings";
	Info.Description +=
	    " Read first and retain all fields. Complete candidate validation occurs before replacement; save explicitly "
	    "for persistence. Plugin/backend/thread/window startup choices do not hot reload.";
	Info.bReadOnly = false;
	Info.Effects = "Updates the shared configuration. Live render controls affect subsequent frames; startup-only "
	               "controls require restart.";
	const FSettingsEdit Example{1, {}};
	Info.Example = WriteRecordWire(RecordType<FSettingsEdit>(), &Example);
	InCatalog.Register(MakeOperation<FSettingsEdit, FApplicationSettingsState>(
	    Info,
	    [InSettings](const auto& InRequest)
	    {
		    try
		    {
			    InSettings->EditApplicationSettings(InRequest.Revision, InRequest.Values);
			    return InSettings->ApplicationSettings();
		    }
		    catch (const FSceneEditError& Error)
		    {
			    throw FAutomationError(Error.Code, Error.what());
		    }
	    }));
	Info.Id = "application.settings.save";
	Info.Summary = "Persist the current Viewer configuration";
	Info.Description = "Writes the current settings snapshot using the existing configuration save path. Later changes "
	                   "remain in memory and require another save.";
	Info.Effects = "Writes the explicit path or current configuration file.";
	Info.Completion = "Configuration write completed successfully.";
	const FSettingsSave SaveExample{1, ""};
	Info.Example = WriteRecordWire(RecordType<FSettingsSave>(), &SaveExample);
	InCatalog.Register(MakeAsyncOperation<FSettingsSave, FSettingsSaved>(
	    Info,
	    [InSettings](const auto& InRequest)
	    {
		    if (InSettings->ApplicationSettings().Revision != InRequest.Revision)
		    {
			    throw FAutomationError("stale_revision", "Application settings changed");
		    }
		    const auto Write = InSettings->SaveApplicationSettings(PathFromUtf8(InRequest.Path));
		    return TPendingOperation<FSettingsSaved>{[Write]() -> std::optional<FSettingsSaved>
		                                             {
			                                             if (!Write.Ready())
			                                             {
				                                             return {};
			                                             }
			                                             if (!*Write.GetReady())
			                                             {
				                                             throw FAutomationError("save_failed",
				                                                                    "Configuration write failed");
			                                             }
			                                             return FSettingsSaved{true};
		                                             }};
	    }));
}
} // namespace Hyperion
