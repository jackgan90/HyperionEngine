#include "Hyperion/Renderer/SceneLightControls.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FSceneSkyStatus>()
{
	static const auto Type = MakeRecord<FSceneSkyStatus>(
	    "scene.sky.status", {Member("state", &FSceneSkyStatus::State), Member("error", &FSceneSkyStatus::Error)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneLightDiagnostic>()
{
	static const auto Type = MakeRecord<FSceneLightDiagnostic>(
	    "scene.light.diagnostic",
	    {Member("handle", &FSceneLightDiagnostic::Handle), Member("id", &FSceneLightDiagnostic::Id),
	     Member("type", &FSceneLightDiagnostic::Type), Member("priority", &FSceneLightDiagnostic::Priority),
	     Member("enabled", &FSceneLightDiagnostic::bEnabled), Member("selected", &FSceneLightDiagnostic::bSelected),
	     Member("tied", &FSceneLightDiagnostic::bTied), Member("message", &FSceneLightDiagnostic::Message),
	     Member("asset", &FSceneLightDiagnostic::Asset)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneLightingInfo>()
{
	static const auto Type = MakeRecord<FSceneLightingInfo>(
	    "scene.lighting.info",
	    {Member("revision", &FSceneLightingInfo::Revision),
	     Member("shadowDirectionalLight", &FSceneLightingInfo::ShadowDirectionalLight),
	     Member("skyLight", &FSceneLightingInfo::SkyLight), Member("lights", &FSceneLightingInfo::Lights)});
	return Type;
}
} // namespace Hyperion
