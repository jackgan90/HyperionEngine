#include "LightInspection.h"

namespace Hyperion
{
std::optional<FLightDiagnosticProperty> FindLightDiagnosticProperty(const FRecordDescriptor& InType,
                                                                    std::string_view InField)
{
	for (const auto& Member : InType.Members)
	{
		if (Member.Id != InField)
		{
			continue;
		}
		if (InType.CppType == typeid(FSceneDirectionalLight) &&
		    Member.Association.Matches(&FSceneDirectionalLight::Priority))
		{
			return FLightDiagnosticProperty{ESceneLightDiagnosticKind::Directional, ELightDiagnosticProperty::Priority};
		}
		if (InType.CppType == typeid(FSceneEnvironmentLight))
		{
			if (Member.Association.Matches(&FSceneEnvironmentLight::Priority))
			{
				return FLightDiagnosticProperty{ESceneLightDiagnosticKind::Sky, ELightDiagnosticProperty::Priority};
			}
			if (Member.Association.Matches(&FSceneEnvironmentLight::Sky))
			{
				return FLightDiagnosticProperty{ESceneLightDiagnosticKind::Sky, ELightDiagnosticProperty::SkyAsset};
			}
		}
	}
	return {};
}

void InspectLightDiagnostic(const FSceneNodeView& InView, FLightDiagnosticProperty InProperty,
                            const FSceneLightingInfo& InInfo, FPropertyPresentation& InOutPresentation)
{
	const bool bSky = InProperty.Kind == ESceneLightDiagnosticKind::Sky;
	for (const auto& Entry : InInfo.Lights)
	{
		if (Entry.Handle != InView.Handle || Entry.Type.Kind() != InProperty.Kind)
		{
			continue;
		}
		if (InProperty.Property == ELightDiagnosticProperty::Priority)
		{
			const bool bEligible =
			    Entry.bEnabled && (bSky || CanCastDirectionalShadows(*InView.Node->DirectionalLight()));
			const auto Tone = !bEligible        ? EPropertyTooltipTone::Default
			                  : Entry.bSelected ? EPropertyTooltipTone::Positive
			                                    : EPropertyTooltipTone::Negative;
			InOutPresentation.TooltipLines.push_back({Entry.Message, Tone});
			if (bEligible && !Entry.bSelected)
			{
				if (!bSky)
				{
					InOutPresentation.TooltipLines.push_back({"This light still contributes direct lighting."});
				}
				InOutPresentation.TooltipLines.push_back(
				    {bSky ? "Increase Priority above the other sky lights to make this sky light effective."
				          : "Increase Priority above the other shadow-casting directional lights to make this light "
				            "the shadow source."});
			}
			if (Entry.bTied)
			{
				InOutPresentation.WarningTooltip =
				    bSky ? "Multiple enabled Sky Lights share the highest Priority."
				         : "Multiple Directional Lights eligible to cast shadows share the highest Priority.";
			}
		}
		else if (Entry.Asset.State == ESceneSkyState::Failed)
		{
			InOutPresentation.Label += " [!]";
			InOutPresentation.Tooltip = Entry.Asset.Error;
		}
		else if (Entry.Asset.State != ESceneSkyState::None && Entry.Asset.State != ESceneSkyState::Ready)
		{
			InOutPresentation.Label += " [...]";
			InOutPresentation.Tooltip = "Sky asset: " + std::string(SceneSkyStateName(Entry.Asset.State));
		}
		return;
	}
}
} // namespace Hyperion
