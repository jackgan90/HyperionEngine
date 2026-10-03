#pragma once
#include "EditorPreferences.h"
#include "Hyperion/Core/Logging/LogHistory.h"
#include "Hyperion/Core/ProfilingSession.h"
#include "Hyperion/Platform/Window.h"
#include "Hyperion/Renderer/RenderSettings.h"
#include "Hyperion/Renderer/SceneSpatialIndex.h"
#include "Hyperion/Scene/Scene.h"

namespace Hyperion
{
struct FEditorOptions
{
	FLogHistory* LogHistory{};
	bool bExerciseLog{};
	std::filesystem::path EngineContent;
	std::optional<std::filesystem::path> AssetRoot;
	bool bReadOnly{};
	std::filesystem::path Layout;
	std::filesystem::path UiPreferences;
	std::filesystem::path PreferencesPath;
	std::filesystem::path RenderSettingsPath;
	FRenderSettings Rendering;
	FEditorPreferences Preferences;
	std::string PreferenceError;
	std::string ExerciseCapture;
	std::optional<float> ApplicationScale;
	std::filesystem::path Capture;
	std::filesystem::path Report;
	std::filesystem::path Benchmark;
	std::filesystem::path ExerciseDocument;
	std::filesystem::path ExerciseViews;
	std::filesystem::path ExerciseRenderControls;
	std::filesystem::path ExercisePlacement;
	std::filesystem::path ExerciseModelPlacement;
	std::filesystem::path ExerciseReparent;
	std::filesystem::path ExerciseOutlines;
	std::filesystem::path ExerciseContent;
	std::filesystem::path ExerciseImport;
	std::filesystem::path ExerciseAssets;
	std::string Scene;
	std::uint32_t Frames{};
	std::uint32_t BenchmarkWarmup = 120;
	std::uint32_t BenchmarkSamples = 300;
	bool bBenchmarkCamera{};
	float BenchmarkCameraStep = .1f;
	FProfilingOptions Profiling;
	FSize BenchmarkViewport;
	bool bBenchmarkLight{};
	ESceneCullingMode CullingMode = ESceneCullingMode::Bvh;
	bool bInstanceBatching = true;
	bool bBenchmarkCollapsed{};
	bool bHidden{};
	bool bKernelOnly{};
	std::vector<std::string> DisabledPlugins;
	bool bExercise{};
	bool bExerciseGizmo{};
	bool bExercisePicking{};
	bool bExerciseMultiSelection{};
	bool bExerciseClipboard{};
	bool bExerciseFraming{};
	bool bExerciseSelectionShortcuts{};
};

FEditorOptions ParseEditorOptions(int InCount, char** InValues);
} // namespace Hyperion
