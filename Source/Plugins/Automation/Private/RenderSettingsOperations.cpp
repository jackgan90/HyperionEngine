#include "Hyperion/IO/Path.h"
#include "Hyperion/Renderer/RenderSettings.h"
#include "SceneOperations.h"

namespace Hyperion
{
namespace
{
struct FRenderSettingsEdit
{
	std::uint64_t Revision{};
	FRenderSettings Values;
};

struct FRenderSettingsSave
{
	std::uint64_t Revision{};
	std::string Path;
};
} // namespace

template<> const FRecordDescriptor& RecordType<FRenderSettingsEdit>()
{
	static const auto Type = MakeRecord<FRenderSettingsEdit>(
	    "automation.render.settings.edit", {Member("revision", &FRenderSettingsEdit::Revision, {.bRequired = true}),
	                                        Member("values", &FRenderSettingsEdit::Values, {.bRequired = true})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FRenderSettingsSave>()
{
	static const auto Type = MakeRecord<FRenderSettingsSave>(
	    "automation.render.settings.save",
	    {Member("revision", &FRenderSettingsSave::Revision, {.bRequired = true}),
	     Member("path", &FRenderSettingsSave::Path,
	            {.Description = "Target-local path; empty uses startup settings path."})});
	return Type;
}

void RegisterRenderSettings(FOperationCatalog& InCatalog, IRenderSettings* InSettings)
{
	FOperationInfo Info;
	Info.Id = "render.settings.get";
	Info.Owner = "automation-scene";
	Info.Version = 2;
	Info.Summary = "Read live rendering settings";
	Info.Description = "Shared render settings and session shadow defaults. The highest-priority eligible "
	                   "directional light supplies CSM and contact shadows; its authored shadow settings override "
	                   "the defaults. activeReversedZ is committed for subsequent scene and 3D asset preview frames.";
	Info.bReadOnly = true;
	Info.Effects = "Reads Main settings state.";
	Info.Completion = "Current snapshot.";
	Info.Unavailable = InSettings ? "" : "This target has no render settings provider.";
	InCatalog.Register(MakeOperation<FSceneInfoRequest, FRenderSettingsState>(Info,
	                                                                          [InSettings](const auto&)
	                                                                          {
		                                                                          return InSettings->RenderSettings();
	                                                                          }));
	Info.Id = "render.settings.set";
	Info.Summary = "Replace rendering settings";
	Info.Description = "Read first and retain fields. Values are validated before replacement. reversedZ applies to "
	                   "subsequent scene and 3D asset preview frames. Save separately for the next launch. "
	                   "Does not modify scene history.";
	Info.bReadOnly = false;
	Info.Effects = "Changes subsequent viewport rendering, including depth convention.";
	Info.Completion = "Main settings committed; does not wait for GPU presentation.";
	const FRenderSettingsEdit Example{1, {}};
	Info.Example = WriteRecordWire(RecordType<FRenderSettingsEdit>(), &Example);
	InCatalog.Register(MakeOperation<FRenderSettingsEdit, FRenderSettingsState>(
	    Info,
	    [InSettings](const auto& InRequest)
	    {
		    InSettings->SetRenderSettings(InRequest.Revision, InRequest.Values);
		    return InSettings->RenderSettings();
	    }));
	Info.Id = "render.settings.save";
	Info.Version = 1;
	Info.Summary = "Save rendering settings";
	Info.Description = "Persist the current rendering settings to a target-local path; no scene or asset write.";
	Info.Effects = "Writes a settings file.";
	Info.Completion = "File write completed.";
	const FRenderSettingsSave SaveExample{1, ""};
	Info.Example = WriteRecordWire(RecordType<FRenderSettingsSave>(), &SaveExample);
	InCatalog.Register(MakeOperation<FRenderSettingsSave, FRenderSettingsState>(
	    Info,
	    [InSettings](const auto& InRequest)
	    {
		    if (InSettings->RenderSettings().Revision != InRequest.Revision)
		    {
			    throw FAutomationError(AutomationErrors::StaleRevision, "Render settings changed");
		    }
		    InSettings->SaveRenderSettings(PathFromUtf8(InRequest.Path));
		    return InSettings->RenderSettings();
	    }));
}
} // namespace Hyperion
