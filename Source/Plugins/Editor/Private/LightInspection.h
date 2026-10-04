#pragma once
#include "Hyperion/Renderer/SceneLightControls.h"

namespace Hyperion
{
enum class ELightDiagnosticProperty
{
	Priority,
	SkyAsset
};

struct FLightDiagnosticProperty
{
	ESceneLightDiagnosticKind Kind;
	ELightDiagnosticProperty Property;
};

std::optional<FLightDiagnosticProperty> FindLightDiagnosticProperty(const FRecordDescriptor& InType,
                                                                    std::string_view InField);
void InspectLightDiagnostic(const FSceneNodeView& InView, FLightDiagnosticProperty InProperty,
                            const FSceneLightingInfo& InInfo, FPropertyPresentation& InOutPresentation);
} // namespace Hyperion
