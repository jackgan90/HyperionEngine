#include "Hyperion/Renderer/RenderSettings.h"
#include "Hyperion/IO/IOService.h"
#include "Hyperion/Reflection/Json.h"
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

FGBufferLayout PresetLayout(EGBufferPreset InPreset)
{
	switch (InPreset)
	{
		case EGBufferPreset::Compact:
			return {};
		case EGBufferPreset::HighPrecision:
			return FGBufferLayout::HighPrecision();
		default:
			throw std::invalid_argument("Unknown GBuffer preset");
	}
}
} // namespace

FScenePipelineSettings MakePipelineSettings(const FRenderSettings& InSettings)
{
	(void)DescribeSceneRenderPipeline(InSettings.Pipeline);
	(void)DescribeGBufferVisualizer(InSettings.DebugMode);
	FScenePipelineSettings Result;
	Result.Pipeline = InSettings.Pipeline;
	Result.GBuffer = PresetLayout(InSettings.GBuffer);
	Result.Exposure = InSettings.Exposure;
	Result.DebugMode = InSettings.DebugMode;
	Result.bClusteredLighting = InSettings.bClusteredLighting;
	Result.ContactShadows = InSettings.Contact;
	return Result;
}

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

FRenderSettings LoadRenderSettings(const std::filesystem::path& InPath)
{
	if (!std::filesystem::exists(InPath))
	{
		return {};
	}
	const auto Bytes = FLocalFileSystem{}.Read(InPath, 1024 * 1024);
	const std::string Text(reinterpret_cast<const char*>(Bytes.data()), Bytes.size());
	return *std::static_pointer_cast<FRenderSettings>(ReadRecord(RecordType<FRenderSettings>(), ParseJson(Text)));
}

void WriteRenderSettings(const std::filesystem::path& InPath, const FRenderSettings& InSettings)
{
	ValidateRenderSettings(InSettings);
	if (!InPath.parent_path().empty())
	{
		std::filesystem::create_directories(InPath.parent_path());
	}
	const auto Text = WriteJson(WriteRecord(RecordType<FRenderSettings>(), &InSettings));
	FLocalFileSystem{}.WriteAtomic(InPath, std::as_bytes(std::span(Text.data(), Text.size())));
}
} // namespace Hyperion
