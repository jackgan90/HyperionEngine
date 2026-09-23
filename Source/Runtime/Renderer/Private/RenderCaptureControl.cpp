#include "Hyperion/Renderer/RenderCaptureControl.h"
#include "Hyperion/SceneEditing/SceneDocument.h"

namespace Hyperion
{
void IRenderCaptureControl::SetRenderCapturePreference(bool)
{
	throw FSceneEditError("unavailable", "This host configures RenderDoc through startup settings");
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
