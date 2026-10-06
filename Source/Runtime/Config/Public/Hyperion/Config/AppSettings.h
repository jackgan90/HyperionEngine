#pragma once
#include "Hyperion/RasterOptions/RasterOptions.h"
#include "Hyperion/RasterOptions/ShadowPreviewOptions.h"
#include "Hyperion/Reflection/RecordValue.h"
#include "Hyperion/Reflection/Reflection.h"

namespace Hyperion
{
struct FAppSettings
{
	std::string Title = "Hyperion | Rendering Lab";
	int Width = 1280;
	int Height = 720;
	int Workers = 4;
	int RhiThreads = 2;
	int MainRenderLead = 1;
	int RenderRhiLead = 1;
	std::string RHIBackend = "d3d12";
	ESceneRenderPipeline RenderPipeline = DefaultSceneRenderPipeline;
	bool bClusteredLighting = true;
	bool bContactShadows = false;
	double ContactShadowLength = .35;
	double ContactShadowThickness = .05;
	double ContactShadowBias = .003;
	int ContactShadowSteps = 96;
	EContactShadowPreview ContactShadowDebug = EContactShadowPreview::Lit;
	int HierarchicalDepthMip = 4;
	EGBufferPreset GBufferLayout = DefaultGBufferPreset;
	double Exposure = 1;
	EGBufferVisualizer GBufferDebug = DefaultGBufferVisualizer;
	bool bVsync = true;
	bool bReversedZ = true;
	bool bShowGui = true;
	std::string RenderDocLibrary;
	// Application composition resolves an empty value from its storage paths.
	std::string RenderDocOutput;
	bool bRenderDocAutoOpen = false;
	double TriangleScale = 0.85;
	double ClearRed = 0.025;
	double ClearGreen = 0.035;
	double ClearBlue = 0.065;
	std::vector<std::string> Plugins{"triangle", "debug-ui"};
	std::vector<std::string> DisabledPlugins;
};

const FTypeDescriptor& SettingsType();
void SaveSettings(const std::filesystem::path& InPath, const FAppSettings& InSettings);
FAppSettings LoadSettings(const std::filesystem::path& InPath);
void ApplyAppSettings(FAppSettings& InTarget, const FAppSettings& InCandidate);
bool EqualAppSettings(const FAppSettings& InFirst, const FAppSettings& InSecond);
template<> const FRecordDescriptor& RecordType<FAppSettings>();
} // namespace Hyperion
