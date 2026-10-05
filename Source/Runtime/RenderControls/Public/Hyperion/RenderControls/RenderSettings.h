#pragma once
#include "Hyperion/RasterOptions/RasterOptions.h"
#include "Hyperion/RenderControls/ShadowControls.h"
#include <filesystem>

namespace Hyperion
{
struct FRenderSettings
{
	ESceneRenderPipeline Pipeline = DefaultSceneRenderPipeline;
	EGBufferPreset GBuffer = DefaultGBufferPreset;
	float Exposure = 1;
	EGBufferVisualizer DebugMode = DefaultGBufferVisualizer;
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

void ValidateRenderSettings(const FRenderSettings& InSettings);
template<> const FRecordDescriptor& RecordType<FContactShadowSettings>();
template<> const FRecordDescriptor& RecordType<FRenderSettings>();
template<> const FRecordDescriptor& RecordType<FRenderSettingsState>();
} // namespace Hyperion
