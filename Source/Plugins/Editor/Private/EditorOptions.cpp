#include "EditorOptions.h"
#include "EditorHostOptions.h"
#include "Hyperion/IO/Path.h"
#include <array>
#include <charconv>
#include <cmath>

namespace Hyperion
{
namespace
{
struct FCaptureExerciseName
{
	EEditorCaptureExercise Exercise;
	std::string_view Name;
};

constexpr std::array CaptureExercises{FCaptureExerciseName{EEditorCaptureExercise::Toggle, "toggle"},
                                      FCaptureExerciseName{EEditorCaptureExercise::Capture, "capture"},
                                      FCaptureExerciseName{EEditorCaptureExercise::Unavailable, "unavailable"},
                                      FCaptureExerciseName{EEditorCaptureExercise::Hud, "hud"}};

bool HasIsolatedEditorAcceptanceRequest(const FEditorOptions& InOptions)
{
	return !InOptions.ExerciseAssets.empty() || InOptions.bExercise || InOptions.bExerciseGizmo ||
	       InOptions.bExercisePicking || InOptions.bExerciseMultiSelection || InOptions.bExerciseClipboard ||
	       InOptions.bExerciseFraming || InOptions.bExerciseSelectionShortcuts || !InOptions.ExerciseDocument.empty() ||
	       !InOptions.ExerciseViews.empty() || !InOptions.ExercisePlacement.empty() ||
	       !InOptions.ExerciseOutlines.empty() || InOptions.ExerciseCapture.has_value() ||
	       !InOptions.ExerciseRenderControls.empty() || !InOptions.ExerciseReparent.empty() ||
	       !InOptions.ExerciseModelPlacement.empty() || !InOptions.ExerciseSceneLifecycle.empty();
}

std::uint32_t UnsignedOption(std::string_view InValue)
{
	std::uint32_t Result{};
	const auto Parsed = std::from_chars(InValue.data(), InValue.data() + InValue.size(), Result);
	if (Parsed.ec != std::errc{} || Parsed.ptr != InValue.data() + InValue.size())
	{
		throw std::invalid_argument("Expected an unsigned integer option");
	}
	return Result;
}

bool ParseFlag(FEditorOptions& InOptions, std::string_view InArgument)
{
	const std::pair<std::string_view, bool FEditorOptions::*> Flags[] = {
	    {"--read-only", &FEditorOptions::bReadOnly},
	    {"--kernel-only", &FEditorOptions::bKernelOnly},
	    {"--hidden", &FEditorOptions::bHidden},
	    {"--exercise", &FEditorOptions::bExercise},
	    {"--exercise-log", &FEditorOptions::bExerciseLog},
	    {"--exercise-gizmo", &FEditorOptions::bExerciseGizmo},
	    {"--exercise-picking", &FEditorOptions::bExercisePicking},
	    {"--exercise-multiselect", &FEditorOptions::bExerciseMultiSelection},
	    {"--exercise-clipboard", &FEditorOptions::bExerciseClipboard},
	    {"--exercise-framing", &FEditorOptions::bExerciseFraming},
	    {"--exercise-selection-shortcuts", &FEditorOptions::bExerciseSelectionShortcuts},
	    {"--benchmark-camera", &FEditorOptions::bBenchmarkCamera},
	    {"--benchmark-light", &FEditorOptions::bBenchmarkLight},
	    {"--benchmark-collapsed", &FEditorOptions::bBenchmarkCollapsed}};
	for (const auto& [Name, Member] : Flags)
	{
		if (InArgument == Name)
		{
			InOptions.*Member = true;
			return true;
		}
	}
	if (InArgument == "--no-instance-batching")
	{
		InOptions.bInstanceBatching = false;
		return true;
	}
	if (InArgument == "--profile-wait")
	{
		InOptions.Profiling.bWait = true;
		return true;
	}
	if (InArgument == "--profile" || InArgument == "--profile-detail" || InArgument == "--profile-gpu" ||
	    InArgument == "--profile-sampling")
	{
		InOptions.Profiling.Mask |= ProfileBasicMask;
		if (InArgument == "--profile-detail")
		{
			InOptions.Profiling.Mask |= ProfileCategoryMask(EProfileCategory::Detail);
		}
		if (InArgument == "--profile-gpu")
		{
			InOptions.Profiling.Mask |= ProfileCategoryMask(EProfileCategory::Gpu);
		}
		if (InArgument == "--profile-sampling")
		{
			InOptions.Profiling.bSampling = true;
		}
		return true;
	}
	return false;
}

bool ParsePath(FEditorOptions& InOptions, std::string_view InArgument, const std::string& InValue)
{
	const std::pair<std::string_view, std::filesystem::path FEditorOptions::*> Paths[] = {
	    {"--engine-content", &FEditorOptions::EngineContent},
	    {"--exercise-import", &FEditorOptions::ExerciseImport},
	    {"--layout", &FEditorOptions::Layout},
	    {"--render-settings", &FEditorOptions::RenderSettingsPath},
	    {"--editor-preferences", &FEditorOptions::PreferencesPath},
	    {"--ui-preferences", &FEditorOptions::UiPreferences},
	    {"--capture", &FEditorOptions::Capture},
	    {"--report", &FEditorOptions::Report},
	    {"--benchmark", &FEditorOptions::Benchmark},
	    {"--exercise-assets", &FEditorOptions::ExerciseAssets},
	    {"--exercise-document", &FEditorOptions::ExerciseDocument},
	    {"--exercise-scene-lifecycle", &FEditorOptions::ExerciseSceneLifecycle},
	    {"--exercise-views", &FEditorOptions::ExerciseViews},
	    {"--exercise-render-controls", &FEditorOptions::ExerciseRenderControls},
	    {"--exercise-placement", &FEditorOptions::ExercisePlacement},
	    {"--exercise-model-placement", &FEditorOptions::ExerciseModelPlacement},
	    {"--exercise-reparent", &FEditorOptions::ExerciseReparent},
	    {"--exercise-outlines", &FEditorOptions::ExerciseOutlines},
	    {"--exercise-content", &FEditorOptions::ExerciseContent}};
	for (const auto& [Name, Member] : Paths)
	{
		if (InArgument == Name)
		{
			InOptions.*Member = PathFromUtf8(InValue);
			return true;
		}
	}
	if (InArgument == "--asset-root")
	{
		InOptions.AssetRoot = PathFromUtf8(InValue);
		return true;
	}
	if (InArgument == "--scene")
	{
		InOptions.Scene = InValue;
		return true;
	}
	if (InArgument == "--disable-plugin")
	{
		InOptions.DisabledPlugins.push_back(InValue);
		return true;
	}
	return false;
}

float FloatOption(const std::string& InValue, float InMinimum, float InMaximum)
{
	std::size_t Consumed{};
	const float Value = std::stof(InValue, &Consumed);
	if (Consumed != InValue.size() || !std::isfinite(Value) || Value < InMinimum || Value > InMaximum)
	{
		throw std::invalid_argument("Numeric option outside supported range");
	}
	return Value;
}

void ParseValue(FEditorOptions& InOptions, std::string_view InArgument, const std::string& InValue)
{
	if (ParsePath(InOptions, InArgument, InValue))
	{
		return;
	}
	if (InArgument == "--profile-categories")
	{
		InOptions.Profiling.Mask |= ParseProfilingCategories(InValue);
	}
	else if (InArgument == "--profile-start")
	{
		InOptions.Profiling.Start = UnsignedOption(InValue);
	}
	else if (InArgument == "--profile-frames")
	{
		InOptions.Profiling.Frames = UnsignedOption(InValue);
	}
	else if (InArgument == "--frames")
	{
		InOptions.Frames = UnsignedOption(InValue);
	}
	else if (InArgument == "--benchmark-warmup")
	{
		InOptions.BenchmarkWarmup = UnsignedOption(InValue);
	}
	else if (InArgument == "--benchmark-samples")
	{
		InOptions.BenchmarkSamples = UnsignedOption(InValue);
	}
	else if (InArgument == "--benchmark-camera-step")
	{
		InOptions.BenchmarkCameraStep = FloatOption(InValue, .000001f, 100);
	}
	else if (InArgument == "--ui-scale")
	{
		InOptions.ApplicationScale = FloatOption(InValue, 1, 2);
	}
	else if (InArgument == "--benchmark-viewport")
	{
		const auto Separator = InValue.find('x');
		if (Separator == std::string::npos)
		{
			throw std::invalid_argument("Expected WIDTHxHEIGHT");
		}
		InOptions.BenchmarkViewport = {UnsignedOption(InValue.substr(0, Separator)),
		                               UnsignedOption(InValue.substr(Separator + 1))};
		if (!InOptions.BenchmarkViewport.Width || !InOptions.BenchmarkViewport.Height)
		{
			throw std::invalid_argument("Viewport dimensions must be positive");
		}
	}
	else if (InArgument == "--scene-culling")
	{
		if (InValue == "none")
		{
			InOptions.CullingMode = ESceneCullingMode::None;
		}
		else if (InValue == "linear")
		{
			InOptions.CullingMode = ESceneCullingMode::Linear;
		}
		else if (InValue == "bvh")
		{
			InOptions.CullingMode = ESceneCullingMode::Bvh;
		}
		else
		{
			throw std::invalid_argument("Culling must be none, linear or bvh");
		}
	}
	else if (InArgument == "--exercise-capture")
	{
		InOptions.ExerciseCapture = ParseEditorCaptureExercise(InValue);
	}
	else
	{
		throw std::invalid_argument("Unknown editor option: " + std::string(InArgument));
	}
}
} // namespace

EEditorCaptureExercise ParseEditorCaptureExercise(std::string_view InName)
{
	for (const auto& Entry : CaptureExercises)
	{
		if (Entry.Name == InName)
		{
			return Entry.Exercise;
		}
	}
	throw std::invalid_argument("--exercise-capture expects toggle, capture, unavailable or hud");
}

std::string_view EditorCaptureExerciseName(EEditorCaptureExercise InExercise)
{
	for (const auto& Entry : CaptureExercises)
	{
		if (Entry.Exercise == InExercise)
		{
			return Entry.Name;
		}
	}
	throw std::invalid_argument("Unknown Editor capture exercise");
}

bool HasEditorAcceptanceRequest(const FEditorOptions& InOptions)
{
	return HasIsolatedEditorAcceptanceRequest(InOptions) || InOptions.bExerciseLog ||
	       !InOptions.ExerciseContent.empty() || !InOptions.ExerciseImport.empty();
}

bool ShouldPersistEditorContentLayout(const FEditorOptions& InOptions)
{
	return ShouldPersistEditorGui(InOptions) && InOptions.ExerciseContent.empty() && InOptions.ExerciseImport.empty();
}

bool ShouldPersistEditorGui(const FEditorOptions& InOptions)
{
	// Log acceptance retains normal layout; content/import preserve their existing separate layout suppression.
	return InOptions.Benchmark.empty() && !HasIsolatedEditorAcceptanceRequest(InOptions);
}

FEditorOptions ParseEditorOptions(int InCount, char** InValues, std::shared_ptr<FStorageSettings> InStorage)
{
	FEditorOptions Result;
	Result.Storage = InStorage
	                     ? std::move(InStorage)
	                     : std::make_shared<FStorageSettings>(ParseStorageLaunchOptions(InCount, InValues, "Editor"));
	const auto& Paths = Result.Storage->Paths();
	Result.EngineContent = DefaultEngineContent(HYP_DEVELOPMENT_CONTENT);
	Result.Layout = Paths.State / "Layout.ini";
	Result.UiPreferences = Paths.Config / "UiScale.ini";
	Result.PreferencesPath = Paths.Config / "Preferences.ini";
	for (int Index = 1; Index < InCount; ++Index)
	{
		const std::string Argument = InValues[Index];
#if !HYP_BUILD_TESTING
		if (Argument.starts_with("--exercise"))
		{
			throw std::invalid_argument("Editor acceptance is unavailable: configure BUILD_TESTING=ON");
		}
#endif
		if (IsStorageValueArgument(Argument))
		{
			++Index;
			continue;
		}
		if (IsStorageFlagArgument(Argument))
		{
			continue;
		}
		if (ParseFlag(Result, Argument))
		{
			continue;
		}
		if (Index + 1 >= InCount)
		{
			throw std::invalid_argument("Missing value for " + Argument);
		}
		ParseValue(Result, Argument, InValues[++Index]);
	}
	if ((!Result.Benchmark.empty() && (Result.Scene.empty() || !Result.BenchmarkSamples || Result.bExercise)) ||
	    ((Result.bBenchmarkCamera || Result.bBenchmarkLight || Result.BenchmarkViewport.Width) &&
	     Result.Benchmark.empty()))
	{
		throw std::invalid_argument(
		    "Editor benchmark requires a scene and positive sample count; exercise is separate");
	}
	if (Result.ExerciseCapture && Result.PreferencesPath == Paths.Config / "Preferences.ini")
	{
		throw std::invalid_argument("Capture acceptance requires an isolated --editor-preferences path");
	}
	const auto ValidateProfileWindow = [&](std::uint64_t InFrames)
	{
		if (Result.Profiling.Mask && InFrames &&
		    (Result.Profiling.Start >= InFrames || Result.Profiling.Frames > InFrames - Result.Profiling.Start))
		{
			throw std::invalid_argument("Profiling window must fit within the finite frame limit");
		}
	};
	ValidateProfileWindow(Result.Frames);
	if (!Result.Benchmark.empty())
	{
		ValidateProfileWindow(std::uint64_t(Result.BenchmarkWarmup) + Result.BenchmarkSamples);
	}
	return Result;
}
} // namespace Hyperion
