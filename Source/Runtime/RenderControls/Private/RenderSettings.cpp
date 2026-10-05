#include "Hyperion/RenderControls/RenderSettings.h"
#include "Hyperion/Reflection/MappedMember.h"
#include <cmath>

namespace Hyperion
{
namespace
{
template<class TOption> std::string OptionTokens(std::span<const TOption> InOptions)
{
	std::string Result;
	for (const auto& Option : InOptions)
	{
		if (!Result.empty())
		{
			Result += " / ";
		}
		Result += Option.Token;
	}
	return Result;
}

} // namespace

void ValidateRenderSettings(const FRenderSettings& InSettings)
{
	(void)DescribeSceneRenderPipeline(InSettings.Pipeline);
	(void)DescribeGBufferPreset(InSettings.GBuffer);
	(void)DescribeGBufferVisualizer(InSettings.DebugMode);
	if (!std::isfinite(InSettings.Exposure) || InSettings.Exposure < .05f || InSettings.Exposure > 8)
	{
		throw std::invalid_argument("Exposure must be 0.05-8");
	}
	InSettings.Contact.Validate();
	ValidateShadowSettings(InSettings.Shadows);
}

template<> const FRecordDescriptor& RecordType<FRenderSettings>()
{
	static const auto Type = MakeRecord<FRenderSettings>(
	    "hyperion.render.settings",
	    {MappedMember<std::string>("pipeline", &FRenderSettings::Pipeline, ToSceneRenderPipelineToken,
	                               ParseSceneRenderPipeline,
	                               Inspect("Pipeline: " + OptionTokens(SceneRenderPipelineOptions()))),
	     MappedMember<std::string>("gbuffer", &FRenderSettings::GBuffer, ToGBufferPresetToken, ParseGBufferPreset,
	                               Inspect("GBuffer: " + OptionTokens(GBufferPresetOptions()))),
	     Member("exposure", &FRenderSettings::Exposure, Inspect("Exposure", .05, 8)),
	     MappedMember<std::uint32_t>("debugMode", &FRenderSettings::DebugMode, ToVisualizerWireValue,
	                                 ParseGBufferVisualizer,
	                                 Inspect("GBuffer debug (" + std::to_string(GBufferVisualizerMinimum()) + "-" +
	                                             std::to_string(GBufferVisualizerMaximum()) + ")",
	                                         GBufferVisualizerMinimum(), GBufferVisualizerMaximum())),
	     Member("clusteredLighting", &FRenderSettings::bClusteredLighting, Inspect("Clustered lighting")),
	     Member("contact", &FRenderSettings::Contact, Inspect("Contact shadows")),
	     Member("shadows", &FRenderSettings::Shadows, Inspect("Directional shadows")),
	     Member("vsync", &FRenderSettings::bVsync, Inspect("VSync")),
	     Member("reversedZ", &FRenderSettings::bReversedZ, Inspect("Reversed Z"))},
	    1, ValidateRenderSettings);
	return Type;
}

template<> const FRecordDescriptor& RecordType<FRenderSettingsState>()
{
	static const auto Type = MakeRecord<FRenderSettingsState>(
	    "hyperion.render.settings.state",
	    {Member("values", &FRenderSettingsState::Values), Member("revision", &FRenderSettingsState::Revision),
	     Member("activeReversedZ", &FRenderSettingsState::bActiveReversedZ)});
	return Type;
}

} // namespace Hyperion
