#include "Hyperion/Renderer/SceneLightControls.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FSceneMainLight>()
{
	static const auto Type = MakeRecord<FSceneMainLight>(
	    "scene.main_light",
	    {Member("revision", &FSceneMainLight::Revision,
	            {.bRequired = true, .Description = "Expected scene revision from light.main.get."}),
	     Member("handle", &FSceneMainLight::Handle, {.bRequired = true}),
	     Member("light", &FSceneMainLight::Light, {.bRequired = true}),
	     Member("direction", &FSceneMainLight::Direction,
	            {.bRequired = true, .Description = "Finite, nonzero world-space surface-to-light vector."})});
	return Type;
}
} // namespace Hyperion
