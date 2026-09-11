#pragma once
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
	std::string RenderPipeline = "deferred";
	std::string GBufferLayout = "compact";
	double Exposure = 1;
	int GBufferDebug = 0;
	std::string ModelSource;
	std::string SceneSource;
	bool bVsync = true;
	bool bReversedZ = true;
	bool bShowGui = true;
	std::string RenderDocLibrary;
	std::string RenderDocOutput = "out/captures/renderdoc";
	bool bRenderDocAutoOpen = false;
	double TriangleScale = 0.85;
	double ClearRed = 0.025;
	double ClearGreen = 0.035;
	double ClearBlue = 0.065;
	std::vector<std::string> Plugins{"triangle", "debug-ui"};
};

const FTypeDescriptor& SettingsType();
void SaveSettings(const std::filesystem::path& InPath, const FAppSettings& InSettings);
FAppSettings LoadSettings(const std::filesystem::path& InPath);
} // namespace Hyperion
