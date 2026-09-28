#include "Hyperion/Renderer/ShadowControls.h"
#include <cmath>

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

void ValidateShadowSettings(const FCascadedShadowSettings& InSettings)
{
	ValidateDirectionalShadowSettings(InSettings);
}

template<> const FRecordDescriptor& RecordType<FCascadedShadowSettings>()
{
	static const auto Type = []
	{
		auto Result = MakeRecord<FCascadedShadowSettings>(
		    "hyperion.shadow.settings",
		    {Member("enabled", &FCascadedShadowSettings::bEnabled, {.bRequired = true}),
		     Member("resolution", &FCascadedShadowSettings::Resolution, {.bRequired = true}),
		     Member("distance", &FCascadedShadowSettings::Distance, {.bRequired = true}),
		     Member("splitLambda", &FCascadedShadowSettings::SplitLambda, {.bRequired = true}),
		     Member("normalOffset", &FCascadedShadowSettings::NormalOffset, {.bRequired = true}),
		     Member("receiverBias", &FCascadedShadowSettings::ReceiverBias, {.bRequired = true}),
		     Member("blendFraction", &FCascadedShadowSettings::BlendFraction, {.bRequired = true}),
		     Member("fadeFraction", &FCascadedShadowSettings::FadeFraction, {.bRequired = true}),
		     Member("debugMode", &FCascadedShadowSettings::DebugMode, {.bRequired = true})},
		    1, ValidateShadowSettings);
		for (auto& Field : Result.Members)
		{
			Field.Options.Inspector = FPropertyPresentation{Field.Id};
		}
		return Result;
	}();
	return Type;
}
} // namespace Hyperion
