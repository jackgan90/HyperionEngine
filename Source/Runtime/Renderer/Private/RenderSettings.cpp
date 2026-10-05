#include "Hyperion/Renderer/RenderSettings.h"
#include "Hyperion/IO/IOService.h"
#include "Hyperion/Reflection/Json.h"

namespace Hyperion
{
namespace
{
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
