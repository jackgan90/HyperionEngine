#pragma once
#include "Hyperion/RasterOptions/ShadowPreviewOptions.h"
#include "Hyperion/Reflection/RecordValue.h"

namespace Hyperion
{
// CPU authoring values; renderer allocation and preview placement are deliberately separate.
struct FDirectionalShadowSettings
{
	bool bEnabled = true;
	std::uint32_t Resolution = 2048;
	float Distance = 100;
	float SplitLambda = .6f;
	float NormalOffset = .6f;
	float ReceiverBias = .15f;
	float BlendFraction = .1f;
	float FadeFraction = .1f;
	EDirectionalShadowPreview DebugMode = EDirectionalShadowPreview::Lit;
	bool operator==(const FDirectionalShadowSettings&) const = default;
};

struct FContactShadowSettings
{
	bool bEnabled{};
	float Length = .35f;
	float Thickness = .05f;
	float Bias = .003f;
	std::uint32_t Steps = 96;
	EContactShadowPreview DebugMode = EContactShadowPreview::Lit;
	std::uint32_t PreviewMip = 4;
	void Validate() const;
	bool operator==(const FContactShadowSettings&) const = default;
};

// Distinct methods allow future local-light maps without giving point/spot lights CSM fields.
struct FSceneLightShadowSettings
{
	FDirectionalShadowSettings Directional;
	FContactShadowSettings Contact;
	bool operator==(const FSceneLightShadowSettings&) const = default;
};

void ValidateDirectionalShadowSettings(const FDirectionalShadowSettings& InSettings);
void ValidateLightShadowSettings(const FSceneLightShadowSettings& InSettings);
template<> const FRecordDescriptor& RecordType<FDirectionalShadowSettings>();
template<> const FRecordDescriptor& RecordType<FContactShadowSettings>();
template<> const FRecordDescriptor& RecordType<FSceneLightShadowSettings>();
} // namespace Hyperion
