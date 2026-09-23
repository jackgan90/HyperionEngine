#include "Hyperion/Gui/Gui.h"
#include "Hyperion/Renderer/RenderCaptureControl.h"
#include "SceneOperations.h"

namespace Hyperion
{
namespace
{
struct FCapturePreference
{
	bool bEnabled{};
};

struct FGuiScale
{
	float Scale = 1;
};

FOperationInfo ControlInfo(std::string InId, std::string InDescription, bool bInReadOnly, bool bInAvailable)
{
	FOperationInfo Info;
	Info.Id = std::move(InId);
	Info.Owner = "automation-scene";
	Info.Summary = Info.Id;
	Info.Description = std::move(InDescription);
	Info.bReadOnly = bInReadOnly;
	Info.Effects =
	    bInReadOnly ? "Reads target state." : "Updates target controls or explicitly requested local files/processes.";
	Info.Completion = "Main control action completed; description specifies any subsequent frame/restart requirement.";
	Info.Unavailable = bInAvailable ? "" : "Target provider unavailable.";
	return Info;
}
} // namespace

template<> const FRecordDescriptor& RecordType<FCapturePreference>()
{
	static const auto Type = MakeRecord<FCapturePreference>(
	    "automation.capture.preference", {Member("enabled", &FCapturePreference::bEnabled, {.bRequired = true})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FGuiScale>()
{
	static const auto Type = MakeRecord<FGuiScale>(
	    "automation.gui.scale",
	    {Member("scale", &FGuiScale::Scale,
	            {.bRequired = true, .Description = "Application scale 1-2. Applied at the next GUI BeginFrame."})});
	return Type;
}

namespace
{
void RegisterCaptureActions(FOperationCatalog& InCatalog, IRenderCaptureControl* InCapture)
{
	auto Info = ControlInfo("renderdoc.status",
	                        "Read compiled/provider availability, pending work, last capture and replay status. Editor "
	                        "preference changes may require restart.",
	                        true, InCapture);
	InCatalog.Register(MakeOperation<FSceneInfoRequest, FRenderCaptureInfo>(Info,
	                                                                        [InCapture](const auto&)
	                                                                        {
		                                                                        return InCapture->RenderCaptureInfo();
	                                                                        }));
	Info = ControlInfo(
	    "renderdoc.capture",
	    "Request a subsequent frame through the host's normal RenderDoc capture path. Job completes only after a new "
	    "capture is produced. Target must be drawable. Replay behavior follows host preference.",
	    false, InCapture);
	InCatalog.Register(MakeAsyncOperation<FSceneInfoRequest, FRenderCaptureInfo>(
	    Info,
	    [InCapture](const auto&)
	    {
		    const auto Before = InCapture->RenderCaptureInfo().Completed;
		    try
		    {
			    InCapture->RequestRenderCapture();
		    }
		    catch (const FSceneEditError& Error)
		    {
			    throw FAutomationError(Error.Code, Error.what());
		    }
		    return TPendingOperation<FRenderCaptureInfo>{
		        [InCapture, Before]() -> std::optional<FRenderCaptureInfo>
		        {
			        const auto Status = InCapture->RenderCaptureInfo();
			        if (Status.Completed > Before)
			        {
				        return Status;
			        }
			        if (Status.bFailed || !Status.bRunning || (!Status.bBusy && !Status.bAvailable))
			        {
				        throw FAutomationError("capture_failed", Status.Message);
			        }
			        return {};
		        }};
	    }));
	Info =
	    ControlInfo("renderdoc.open", "Open the last completed capture in the configured RenderDoc replay application.",
	                false, InCapture);
	InCatalog.Register(
	    MakeOperation<FSceneInfoRequest, FRenderCaptureInfo>(Info,
	                                                         [InCapture](const auto&)
	                                                         {
		                                                         try
		                                                         {
			                                                         InCapture->OpenRenderCapture();
			                                                         return InCapture->RenderCaptureInfo();
		                                                         }
		                                                         catch (const FSceneEditError& Error)
		                                                         {
			                                                         throw FAutomationError(Error.Code, Error.what());
		                                                         }
	                                                         }));
}

void RegisterCapturePreference(FOperationCatalog& InCatalog, IRenderCaptureControl* InCapture)
{
	auto Info =
	    ControlInfo("renderdoc.set_preference",
	                "Persist the Editor's RenderDoc capture preference. Enabling a provider absent at startup requires "
	                "restart; status reports actual availability. Viewer uses application settings instead.",
	                false, InCapture && InCapture->RenderCaptureInfo().Preference.has_value());
	const FCapturePreference Preference{true};
	Info.Example = WriteRecordWire(RecordType<FCapturePreference>(), &Preference);
	InCatalog.Register(MakeOperation<FCapturePreference, FRenderCaptureInfo>(
	    Info,
	    [InCapture](const auto& InRequest)
	    {
		    try
		    {
			    InCapture->SetRenderCapturePreference(InRequest.bEnabled);
			    return InCapture->RenderCaptureInfo();
		    }
		    catch (const FSceneEditError& Error)
		    {
			    throw FAutomationError(Error.Code, Error.what());
		    }
	    }));
}

void RegisterGuiScale(FOperationCatalog& InCatalog, FGui* InGui)
{
	auto Info = ControlInfo("gui.scale.get", "Read currently applied GUI scale.", true, InGui);
	InCatalog.Register(MakeOperation<FSceneInfoRequest, FGuiScale>(Info,
	                                                               [InGui](const auto&)
	                                                               {
		                                                               return FGuiScale{InGui->ApplicationScale()};
	                                                               }));
	Info = ControlInfo("gui.scale.set",
	                   "Request application scale 1-2 through the same GUI scale service as human controls. Applied on "
	                   "the next GUI frame and persisted by the normal host preference lifecycle.",
	                   false, InGui);
	const FGuiScale Scale{1.25f};
	Info.Example = WriteRecordWire(RecordType<FGuiScale>(), &Scale);
	InCatalog.Register(MakeOperation<FGuiScale, FGuiScale>(Info,
	                                                       [InGui](const auto& InRequest)
	                                                       {
		                                                       InGui->SetApplicationScale(InRequest.Scale);
		                                                       return InRequest;
	                                                       }));
}
} // namespace

void RegisterRenderCapture(FOperationCatalog& InCatalog, IRenderCaptureControl* InCapture, FGui* InGui)
{
	RegisterCaptureActions(InCatalog, InCapture);
	RegisterCapturePreference(InCatalog, InCapture);
	RegisterGuiScale(InCatalog, InGui);
}
} // namespace Hyperion
