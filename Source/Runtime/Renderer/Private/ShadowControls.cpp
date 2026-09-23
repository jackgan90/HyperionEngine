#include "Hyperion/Renderer/ShadowControls.h"
#include <cmath>

namespace Hyperion
{
void ValidateShadowSettings(const FCascadedShadowSettings& InSettings)
{
	if ((InSettings.Resolution != 1024 && InSettings.Resolution != 2048) || InSettings.DebugMode > 5 ||
	    !std::isfinite(InSettings.Distance) || InSettings.Distance <= 0 || !std::isfinite(InSettings.SplitLambda) ||
	    InSettings.SplitLambda < 0 || InSettings.SplitLambda > 1 || !std::isfinite(InSettings.NormalOffset) ||
	    InSettings.NormalOffset < 0 || InSettings.NormalOffset > 2 || !std::isfinite(InSettings.ReceiverBias) ||
	    InSettings.ReceiverBias < 0 || InSettings.ReceiverBias > 2 || !std::isfinite(InSettings.BlendFraction) ||
	    InSettings.BlendFraction < .01f || InSettings.BlendFraction > .25f || !std::isfinite(InSettings.FadeFraction) ||
	    InSettings.FadeFraction < .01f || InSettings.FadeFraction > .5f)
	{
		throw std::invalid_argument("Shadow settings exceed the host control ranges");
	}
}

template<> const FRecordDescriptor& RecordType<FCascadedShadowSettings>()
{
	static const auto Type = MakeRecord<FCascadedShadowSettings>(
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
	return Type;
}
} // namespace Hyperion
