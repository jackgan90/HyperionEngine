#include "Hyperion/Scene/LightShadows.h"
#include "Hyperion/Reflection/MappedMember.h"
#include <cmath>

namespace Hyperion
{
namespace
{
template<class T> std::vector<FPropertyChoice> PreviewChoices(std::span<const T> InOptions)
{
	std::vector<FPropertyChoice> Result;
	for (const auto& Option : InOptions)
	{
		Result.push_back({WriteValue(Option.WireValue), std::string(Option.Label)});
	}
	return Result;
}
} // namespace

void ValidateDirectionalShadowSettings(const FDirectionalShadowSettings& InSettings)
{
	(void)DescribeShadowPreview(InSettings.DebugMode);
	if ((InSettings.Resolution != 1024 && InSettings.Resolution != 2048) || !std::isfinite(InSettings.Distance) ||
	    InSettings.Distance <= 0 || !std::isfinite(InSettings.SplitLambda) || InSettings.SplitLambda < 0 ||
	    InSettings.SplitLambda > 1 || !std::isfinite(InSettings.NormalOffset) || InSettings.NormalOffset < 0 ||
	    InSettings.NormalOffset > 2 || !std::isfinite(InSettings.ReceiverBias) || InSettings.ReceiverBias < 0 ||
	    InSettings.ReceiverBias > 2 || !std::isfinite(InSettings.BlendFraction) || InSettings.BlendFraction < .01f ||
	    InSettings.BlendFraction > .25f || !std::isfinite(InSettings.FadeFraction) || InSettings.FadeFraction < .01f ||
	    InSettings.FadeFraction > .5f)
	{
		throw std::invalid_argument("Directional shadow settings exceed supported ranges");
	}
}

void FContactShadowSettings::Validate() const
{
	(void)DescribeShadowPreview(DebugMode);
	if (!std::isfinite(Length) || Length <= 0 || Length > 100 || !std::isfinite(Thickness) || Thickness <= 0 ||
	    Thickness > 10 || !std::isfinite(Bias) || Bias < 0 || Bias > Length || Steps < 8 || Steps > 512 ||
	    PreviewMip > 16)
	{
		throw std::invalid_argument("Invalid contact shadow length, thickness, bias, steps or preview");
	}
}

void ValidateLightShadowSettings(const FSceneLightShadowSettings& InSettings)
{
	ValidateDirectionalShadowSettings(InSettings.Directional);
	InSettings.Contact.Validate();
}

template<> const FRecordDescriptor& RecordType<FDirectionalShadowSettings>()
{
	static const auto Type = MakeRecord<FDirectionalShadowSettings>(
	    "hyperion.light.directional-shadows",
	    {Member("enabled", &FDirectionalShadowSettings::bEnabled, Inspect("Enabled")),
	     Member("resolution", &FDirectionalShadowSettings::Resolution, Inspect("Resolution (1024 / 2048)")),
	     Member("distance", &FDirectionalShadowSettings::Distance, Inspect("Distance")),
	     Member("splitLambda", &FDirectionalShadowSettings::SplitLambda, Inspect("Split distribution", 0, 1)),
	     Member("normalOffset", &FDirectionalShadowSettings::NormalOffset, Inspect("Normal offset", 0, 2)),
	     Member("receiverBias", &FDirectionalShadowSettings::ReceiverBias, Inspect("Receiver bias", 0, 2)),
	     Member("blendFraction", &FDirectionalShadowSettings::BlendFraction, Inspect("Cascade blend")),
	     Member("fadeFraction", &FDirectionalShadowSettings::FadeFraction, Inspect("Distance fade")),
	     MappedMember<std::uint32_t>(
	         "debugMode", &FDirectionalShadowSettings::DebugMode,
	         [](EDirectionalShadowPreview InValue)
	         {
		         return ToShadowPreviewWireValue(InValue);
	         },
	         ParseDirectionalShadowPreview,
	         {.Inspector = FPropertyPresentation{.Label = "Preview",
	                                             .Choices = PreviewChoices(DirectionalShadowPreviewOptions())}})},
	    1, ValidateDirectionalShadowSettings);
	return Type;
}

template<> const FRecordDescriptor& RecordType<FContactShadowSettings>()
{
	static const auto Type = MakeRecord<FContactShadowSettings>(
	    "hyperion.contact.settings",
	    {Member("enabled", &FContactShadowSettings::bEnabled, Inspect("Enabled")),
	     Member("length", &FContactShadowSettings::Length, Inspect("Length")),
	     Member("thickness", &FContactShadowSettings::Thickness, Inspect("Thickness")),
	     Member("bias", &FContactShadowSettings::Bias, Inspect("Bias")),
	     Member("steps", &FContactShadowSettings::Steps, Inspect("Steps", 8, 512)),
	     MappedMember<std::uint32_t>(
	         "debugMode", &FContactShadowSettings::DebugMode,
	         [](EContactShadowPreview InValue)
	         {
		         return ToShadowPreviewWireValue(InValue);
	         },
	         ParseContactShadowPreview,
	         {.Inspector =
	              FPropertyPresentation{.Label = "Preview", .Choices = PreviewChoices(ContactShadowPreviewOptions())}}),
	     Member("previewMip", &FContactShadowSettings::PreviewMip, Inspect("HZB mip", 0, 16))},
	    1,
	    [](const FContactShadowSettings& InSettings)
	    {
		    InSettings.Validate();
	    });
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneLightShadowSettings>()
{
	static const auto Type = MakeRecord<FSceneLightShadowSettings>(
	    "hyperion.light.shadows",
	    {Member("directional", &FSceneLightShadowSettings::Directional, Inspect("Directional shadows")),
	     Member("contact", &FSceneLightShadowSettings::Contact, Inspect("Contact shadows"))},
	    1, ValidateLightShadowSettings);
	return Type;
}
} // namespace Hyperion
