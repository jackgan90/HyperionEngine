#include "Hyperion/Renderer/ShadowControls.h"

namespace Hyperion
{
void ResolveSceneLightShadows(const FSceneMetadata* InMetadata, FCascadedShadowSettings& InOutDirectional,
                              FContactShadowSettings& InOutContact)
{
	if (!InMetadata || !InMetadata->Lighting.Directional.Handle)
	{
		return;
	}
	const auto Light = InMetadata->DirectionalLights.find(*InMetadata->Lighting.Directional.Handle);
	if (Light == InMetadata->DirectionalLights.end() || !Light->second.bEnabled || !Light->second.Light.ShadowSettings)
	{
		return;
	}
	const auto& Authored = *Light->second.Light.ShadowSettings;
	static_cast<FDirectionalShadowSettings&>(InOutDirectional) = Authored.Directional;
	InOutContact = Authored.Contact;
}

} // namespace Hyperion
