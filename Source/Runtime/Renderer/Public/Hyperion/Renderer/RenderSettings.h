#pragma once
#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include "Hyperion/Renderer/ShadowControls.h"

namespace Hyperion
{
struct FRenderSettings
{
	std::string Pipeline{ToSceneRenderPipelineToken(DefaultSceneRenderPipeline)};
	std::string GBuffer{ToGBufferPresetToken(DefaultGBufferPreset)};
	float Exposure = 1;
	std::uint32_t DebugMode = ToVisualizerWireValue(DefaultGBufferVisualizer);
	bool bClusteredLighting = true;
	FContactShadowSettings Contact;
	FCascadedShadowSettings Shadows;
	bool bVsync = true;
	bool bReversedZ = true;
};

struct FRenderSettingsState
{
	FRenderSettings Values;
	std::uint64_t Revision{};
	bool bActiveReversedZ = true;
};

class IRenderSettings
{
public:
	virtual ~IRenderSettings() = default;
	virtual FRenderSettingsState RenderSettings() const = 0;
	virtual void SetRenderSettings(std::uint64_t InRevision, const FRenderSettings& InSettings) = 0;
	virtual void SaveRenderSettings(const std::filesystem::path& InPath) = 0;
};

FScenePipelineSettings MakePipelineSettings(const FRenderSettings& InSettings);
void ValidateRenderSettings(const FRenderSettings& InSettings);
FRenderSettings LoadRenderSettings(const std::filesystem::path& InPath);
void WriteRenderSettings(const std::filesystem::path& InPath, const FRenderSettings& InSettings);
template<> const FRecordDescriptor& RecordType<FContactShadowSettings>();
template<> const FRecordDescriptor& RecordType<FRenderSettings>();
template<> const FRecordDescriptor& RecordType<FRenderSettingsState>();
} // namespace Hyperion
