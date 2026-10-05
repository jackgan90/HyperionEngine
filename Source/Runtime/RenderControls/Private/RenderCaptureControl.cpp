#include "Hyperion/RenderControls/RenderCaptureControl.h"
#include "Hyperion/SceneEditing/SceneDocument.h"

namespace Hyperion
{
void IRenderCaptureControl::SetRenderCapturePreference(bool)
{
	throw FSceneEditError(SceneEditErrors::Unavailable, "This host configures RenderDoc through startup settings");
}

FRenderCaptureHudInfo IRenderCaptureControl::RenderCaptureHudInfo() const
{
	return {};
}

void IRenderCaptureControl::SetRenderCaptureHudPreference(bool)
{
	throw FSceneEditError(SceneEditErrors::Unavailable, "This host does not provide a RenderDoc HUD preference");
}

template<> const FRecordDescriptor& RecordType<FRenderCaptureHudInfo>()
{
	static const auto Type = MakeRecord<FRenderCaptureHudInfo>(
	    "hyperion.render.capture.hud.info",
	    {Member("preference", &FRenderCaptureHudInfo::Preference,
	            {.Description = "Saved HUD preference; null when host does not support preferences."}),
	     Member("enabled", &FRenderCaptureHudInfo::Enabled,
	            {.Description = "Actual process-wide HUD visibility; null when capture runtime is unavailable."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FRenderCaptureInfo>()
{
	static const auto Type = MakeRecord<FRenderCaptureInfo>(
	    "hyperion.render.capture.info",
	    {Member("compiled", &FRenderCaptureInfo::bCompiled), Member("available", &FRenderCaptureInfo::bAvailable),
	     Member("busy", &FRenderCaptureInfo::bBusy), Member("running", &FRenderCaptureInfo::bRunning),
	     Member("failed", &FRenderCaptureInfo::bFailed), Member("preference", &FRenderCaptureInfo::Preference),
	     Member("completed", &FRenderCaptureInfo::Completed), Member("path", &FRenderCaptureInfo::Path),
	     Member("message", &FRenderCaptureInfo::Message), Member("replayMessage", &FRenderCaptureInfo::ReplayMessage)});
	return Type;
}
} // namespace Hyperion
