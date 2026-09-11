#pragma once
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/Config/AppSettings.h"
#include "Hyperion/Renderer/CascadedShadowMap.h"
#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include <optional>

namespace Hyperion
{
struct FOptions
{
	std::filesystem::path Config = std::filesystem::path(HYP_SOURCE_DIR) / "experiments/Triangle.json";
	std::filesystem::path Capture;
	std::filesystem::path SaveConfig;
	std::filesystem::path Model;
	std::filesystem::path Scene;
	std::filesystem::path Benchmark;
	int BenchmarkWarmup = 60;
	bool bBenchmarkCamera{};
	float BenchmarkCameraStep = .1f;
	bool bNoVsync{};
	bool bNoInstanceBatching{};
	FScenePipelineSettings Pipeline;
	bool bPipelineOption{};
	bool bGBufferOption{};
	bool bExposureOption{};
	std::optional<int> GBufferDebug;
	FCascadedShadowSettings Shadows;
	std::optional<FVec3> ShadowLight;
	bool bBenchmarkLight{};
	std::uint32_t ProfilingMask{};
	int ProfileStart{};
	int ProfileFrames{};
	bool bProfileSampling{};
	bool bProfileWait{};
	std::string SceneCulling = "bvh";
	bool bVerifyModel{};
	std::optional<std::string> Backend;
	std::optional<int> MainRenderLead;
	std::optional<int> RenderRhiLead;
	int Frames{};
	bool bHidden{};
	bool bExercise{};
	bool bExerciseDepthConfig{};
	bool bVerifyClear{};
	bool bVerifyTriangle{};
	bool bVerifyUi{};
	bool bNoUi{};
	bool bRenderDoc{};
	bool bOpenRdc{};
	bool bExerciseRdcUi{};
	std::optional<std::string> RenderDocLibrary;
	std::optional<std::string> RdcOutput;
	std::vector<int> RdcFrames;
};

FOptions ParseOptions(int InArgc, char** InArgv);
bool ParseProfilingOption(FOptions& InOptions, const std::string& InArg, int InArgc, char** InArgv, int& InIndex);
bool ParseShadowOption(FOptions& InOptions, const std::string& InArg, int InArgc, char** InArgv, int& InIndex);
void ApplyOptions(const FOptions& InOptions, FAppSettings& InSettings);
void VerifyImage(const FImage& InImage, const FAppSettings& InSettings, const FOptions& InOptions);
} // namespace Hyperion
