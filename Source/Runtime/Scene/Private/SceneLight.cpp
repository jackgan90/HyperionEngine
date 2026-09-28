#include "Hyperion/Scene/SceneLight.h"
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace Hyperion
{
bool FSceneDirectionalLight::operator==(const FSceneDirectionalLight& InOther) const
{
	return Color.X == InOther.Color.X && Color.Y == InOther.Color.Y && Color.Z == InOther.Color.Z &&
	       Intensity == InOther.Intensity && bCastShadows == InOther.bCastShadows &&
	       ShadowSettings == InOther.ShadowSettings && Priority == InOther.Priority;
}

bool FSceneEnvironmentLight::operator==(const FSceneEnvironmentLight& InOther) const
{
	return Color.X == InOther.Color.X && Color.Y == InOther.Color.Y && Color.Z == InOther.Color.Z &&
	       Intensity == InOther.Intensity && Source == InOther.Source && Sky == InOther.Sky &&
	       Tint.X == InOther.Tint.X && Tint.Y == InOther.Tint.Y && Tint.Z == InOther.Tint.Z &&
	       YawDegrees == InOther.YawDegrees && bVisible == InOther.bVisible && Data == InOther.Data &&
	       Priority == InOther.Priority;
}

FVec3 SceneLightRadiance(FVec3 InColor, float InIntensity)
{
	const auto Radiance = ScaleVector(InColor, InIntensity);
	if (!IsFinite(InColor) || !std::isfinite(InIntensity) || InColor.X < 0 || InColor.Y < 0 || InColor.Z < 0 ||
	    InIntensity < 0 || !IsFinite(Radiance))
	{
		throw std::invalid_argument("Scene light requires finite nonnegative color, intensity and radiance");
	}
	return Radiance;
}

void ValidateSceneDirectionalLight(const FSceneDirectionalLight& InLight)
{
	SceneLightRadiance(InLight.Color, InLight.Intensity);
	if (InLight.ShadowSettings)
	{
		ValidateLightShadowSettings(*InLight.ShadowSettings);
	}
}

void ValidateSceneEnvironmentLight(const FSceneEnvironmentLight& InLight)
{
	SceneLightRadiance(InLight.Color, InLight.Intensity);
	SceneLightRadiance(InLight.Tint, InLight.Intensity);
	if (!std::isfinite(InLight.YawDegrees) || (InLight.Source != ESceneEnvironmentSource::ConstantColor &&
	                                           InLight.Source != ESceneEnvironmentSource::SkyAsset))
	{
		throw std::invalid_argument("Invalid environment source or yaw");
	}
	ValidateAssetRef(InLight.Sky);
	if (InLight.Sky.TypeId != RecordType<FSkyAsset>().Id)
	{
		throw std::invalid_argument("Environment reference must target a sky asset");
	}
}

template<> const FRecordDescriptor& RecordType<FSceneDirectionalLight>()
{
	static const auto Type = []
	{
		auto Result = MakeRecord<FSceneDirectionalLight>(
		    "hyperion.scenedirectionallight",
		    {Member("priority", &FSceneDirectionalLight::Priority,
		            {.Inspector =
		                 FPropertyPresentation{.Label = "Priority",
		                                       .Tooltip = "Higher priority wins the directional shadow map and contact "
		                                                  "shadow source. All enabled lights illuminate."}}),
		     Member("color", &FSceneDirectionalLight::Color,
		            {.Inspector = FPropertyPresentation{.Label = "Color", .Widget = EPropertyWidget::Color3}}),
		     Member("intensity", &FSceneDirectionalLight::Intensity, Inspect("Intensity", 0, {})),
		     Member("castShadows", &FSceneDirectionalLight::bCastShadows, Inspect("Cast shadows")),
		     Member("shadowSettings", &FSceneDirectionalLight::ShadowSettings,
		            {.Inspector = FPropertyPresentation{.Label = "Shadow settings",
		                                                .Tooltip = "Store shadow parameters on this light. Without an "
		                                                           "override, legacy session defaults apply."}})},
		    3, ValidateSceneDirectionalLight);
		Result.Migrations.emplace(2,
		                          [](FArchiveNode::FObject&)
		                          {
		                          });
		Result.Migrations.emplace(1,
		                          [](FArchiveNode::FObject&)
		                          {
		                          });
		return Result;
	}();
	return Type;
}

template<> std::span<const TRecordEnumEntry<ESceneEnvironmentSource>> RecordEnumEntries<ESceneEnvironmentSource>()
{
	static constexpr TRecordEnumEntry<ESceneEnvironmentSource> Values[] = {
	    {ESceneEnvironmentSource::ConstantColor, "ConstantColor", "Use the component constant ambient color."},
	    {ESceneEnvironmentSource::SkyAsset, "SkyAsset", "Use the referenced sky asset for environment lighting."}};
	return Values;
}

namespace
{
FPropertyCondition WhenSource(ESceneEnvironmentSource InSource)
{
	return {"source", {WriteValue(InSource)}};
}

void MigrateEnvironmentLightV2(FArchiveNode::FObject& InFields)
{
	InFields.try_emplace("source", WriteValue(ESceneEnvironmentSource::ConstantColor));
	if (const auto Yaw = InFields.find("yawRadians"); Yaw != InFields.end())
	{
		const auto Radians = ReadValue<double>(Yaw->second);
		InFields.erase(Yaw);
		double Degrees = Radians * (180 / std::numbers::pi);
		if (std::isfinite(Radians) && std::abs(Degrees) > 360)
		{
			// Keep single-turn values as authored. Reduce larger angles before float conversion loses phase.
			Degrees = std::atan2(std::sin(Radians), std::cos(Radians)) * (180 / std::numbers::pi);
		}
		InFields["yawDegrees"] = WriteValue(float(Degrees));
	}
	// An absent v2 reference now means the Engine default sky.
	if (const auto Sky = InFields.find("sky");
	    Sky != InFields.end() && std::holds_alternative<std::monostate>(Sky->second.Value))
	{
		InFields.erase(Sky);
	}
}

std::vector<FRecordMember> EnvironmentLightMembers()
{
	const auto Constant = WhenSource(ESceneEnvironmentSource::ConstantColor);
	const auto Sky = WhenSource(ESceneEnvironmentSource::SkyAsset);
	return {Member("priority", &FSceneEnvironmentLight::Priority,
	               {.Inspector =
	                    FPropertyPresentation{
	                        .Label = "Priority",
	                        .Tooltip = "Higher priority wins the single global sky and environment lighting source."}}),
	        Member("color", &FSceneEnvironmentLight::Color,
	               {.Inspector = FPropertyPresentation{.Label = "Color",
	                                                   .Widget = EPropertyWidget::Color3,
	                                                   .Tooltip = "Constant ambient radiance color.",
	                                                   .VisibleWhen = Constant}}),
	        Member("intensity", &FSceneEnvironmentLight::Intensity, Inspect("Intensity", 0, {})),
	        Member("source", &FSceneEnvironmentLight::Source,
	               {.Inspector = FPropertyPresentation{.Label = "Source", .Choices = {"Constant color", "Sky asset"}}}),
	        Member("sky", &FSceneEnvironmentLight::Sky,
	               {.Inspector = FPropertyPresentation{.Label = "Sky asset",
	                                                   .ReferenceType = RecordType<FSkyAsset>().Id,
	                                                   .VisibleWhen = Sky}}),
	        Member("tint", &FSceneEnvironmentLight::Tint,
	               {.Inspector = FPropertyPresentation{.Label = "Tint",
	                                                   .Widget = EPropertyWidget::Color3,
	                                                   .Tooltip = "Multiplies sky background, diffuse and specular "
	                                                              "lighting per channel.",
	                                                   .VisibleWhen = Sky}}),
	        Member("yawDegrees", &FSceneEnvironmentLight::YawDegrees,
	               {.Inspector = FPropertyPresentation{.Label = "Yaw (degrees)", .VisibleWhen = Sky}}),
	        Member("visible", &FSceneEnvironmentLight::bVisible,
	               {.Inspector = FPropertyPresentation{.Label = "Show background",
	                                                   .Tooltip = "Draw the sky behind scene geometry. Lighting is "
	                                                              "unaffected.",
	                                                   .VisibleWhen = Sky}})};
}
} // namespace

template<> const FRecordDescriptor& RecordType<FSceneEnvironmentLight>()
{
	static const auto Type = []
	{
		auto Result = MakeRecord<FSceneEnvironmentLight>("hyperion.sceneenvironmentlight", EnvironmentLightMembers(), 4,
		                                                 ValidateSceneEnvironmentLight);
		// Version 1 had no source field; ConstantColor was its only behavior.
		Result.Migrations.emplace(1,
		                          [](FArchiveNode::FObject& InFields)
		                          {
			                          InFields.try_emplace("source",
			                                               WriteValue(ESceneEnvironmentSource::ConstantColor));
		                          });
		Result.Migrations.emplace(2, MigrateEnvironmentLightV2);
		Result.Migrations.emplace(3,
		                          [](FArchiveNode::FObject&)
		                          {
		                          });
		return Result;
	}();
	return Type;
}
} // namespace Hyperion
