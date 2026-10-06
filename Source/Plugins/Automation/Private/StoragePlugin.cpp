#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include "Hyperion/Config/StorageSettings.h"

namespace Hyperion
{
namespace
{
class FStorageAutomationPlugin final : public FPlugin
{
public:
	void Start(FPluginContext& InContext) override
	{
		auto& Catalog = InContext.Require<FOperationCatalog>();
		InContext.Defer(
		    [&Catalog]
		    {
			    Catalog.UnregisterOwner("automation-storage");
		    });
		auto* Storage = InContext.Find<FStorageSettings>();
		FOperationInfo Info;
		Info.Id = "application.storage.get";
		Info.Owner = "automation-storage";
		Info.Summary = "Read application storage locations and restart state";
		Info.Description =
		    "Reports active, saved and next-launch user/cache roots, locator, profile and launch overrides.";
		Info.Effects = "Read-only; no content, configuration or history changes.";
		Info.Completion = "Current storage snapshot.";
		Info.bReadOnly = true;
		Info.Unavailable = Storage ? "" : "This target has no application storage provider; attach to Editor.";
		const FStorageSettingsQuery Query;
		Info.Example = WriteRecordWire(RecordType<FStorageSettingsQuery>(), &Query);
		Catalog.Register(MakeOperation<FStorageSettingsQuery, FStorageSettingsState>(Info,
		                                                                             [Storage](const auto&)
		                                                                             {
			                                                                             return Storage->Refresh();
		                                                                             }));
		Info.Id = "application.storage.set";
		Info.Summary = "Save user-data and cache locations for the next launch";
		Info.Description =
		    "Read first and retain revision. Empty roots restore defaults. Validates absolute paths and "
		    "writability, then atomically persists. Current paths remain fixed; CLI/environment overrides still win.";
		Info.Effects =
		    "Probes candidate directories and saves bootstrap settings. No live relocation or scene history changes.";
		Info.Completion = "Settings saved; response reports whether restart changes effective roots.";
		Info.bReadOnly = false;
		const FStorageSettingsEdit Example{1, {}};
		Info.Example = WriteRecordWire(RecordType<FStorageSettingsEdit>(), &Example);
		Catalog.Register(MakeOperation<FStorageSettingsEdit, FStorageSettingsState>(
		    Info,
		    [Storage](const auto& InRequest)
		    {
			    try
			    {
				    return Storage->Set(InRequest);
			    }
			    catch (const FStorageSettingsError& Error)
			    {
				    throw FAutomationError(Error.Code, Error.what());
			    }
		    }));
	}
};
} // namespace

void RegisterStorageAutomation(FPluginRegistry& InRegistry)
{
	FPluginDescriptor Descriptor;
	Descriptor.Id = "automation-storage";
	Descriptor.Dependencies = {"automation-catalog"};
	Descriptor.Before = {"automation-session"};
	Descriptor.Requires = {typeid(FOperationCatalog)};
	Descriptor.Optional = {typeid(FStorageSettings)};
	Descriptor.Create = []
	{
		return std::make_unique<FStorageAutomationPlugin>();
	};
	InRegistry.Add(std::move(Descriptor));
}
} // namespace Hyperion
