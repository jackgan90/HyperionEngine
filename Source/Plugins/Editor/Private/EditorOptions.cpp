#include "EditorApplication.h"
#include <cmath>

namespace Hyperion
{
FEditorOptions ParseEditorOptions(int InCount, char** InValues)
{
	FEditorOptions Result;
	const auto Root = std::filesystem::path(HYP_SOURCE_DIR);
	Result.Mounts = Root / (std::filesystem::exists(Root / "ContentMounts.local.json") ? "ContentMounts.local.json"
	                                                                                   : "ContentMounts.json");
	Result.Layout = Root / "out/editor/Layout.ini";
	Result.UiPreferences = Root / "out/editor/UiScale.ini";
	for (int Index = 1; Index < InCount; ++Index)
	{
		const std::string Argument = InValues[Index];
		if (Argument == "--kernel-only")
		{
			Result.bKernelOnly = true;
			continue;
		}
		if (Argument == "--hidden")
		{
			Result.bHidden = true;
			continue;
		}
		if (Argument == "--exercise")
		{
			Result.bExercise = true;
			continue;
		}
		if (Argument == "--exercise-gizmo")
		{
			Result.bExerciseGizmo = true;
			continue;
		}
		if (Argument == "--exercise-picking")
		{
			Result.bExercisePicking = true;
			continue;
		}
		if (Argument == "--benchmark-camera")
		{
			Result.bBenchmarkCamera = true;
			continue;
		}
		if (Argument == "--benchmark-collapsed")
		{
			Result.bBenchmarkCollapsed = true;
			continue;
		}
		if (Index + 1 >= InCount)
		{
			throw std::invalid_argument("Missing value for " + Argument);
		}
		const std::string Value = InValues[++Index];
		if (Argument == "--disable-plugin")
		{
			Result.DisabledPlugins.push_back(Value);
		}
		else if (Argument == "--mounts")
		{
			Result.Mounts = Value;
		}
		else if (Argument == "--layout")
		{
			Result.Layout = Value;
		}
		else if (Argument == "--scene")
		{
			Result.Scene = Value;
		}
		else if (Argument == "--ui-preferences")
		{
			Result.UiPreferences = Value;
		}
		else if (Argument == "--ui-scale")
		{
			std::size_t Consumed{};
			const float Scale = std::stof(Value, &Consumed);
			if (Consumed != Value.size() || !std::isfinite(Scale) || Scale < 1 || Scale > 2)
			{
				throw std::invalid_argument("--ui-scale must be finite and between 1 and 2");
			}
			Result.ApplicationScale = Scale;
		}
		else if (Argument == "--capture")
		{
			Result.Capture = Value;
		}
		else if (Argument == "--report")
		{
			Result.Report = Value;
		}
		else if (Argument == "--benchmark")
		{
			Result.Benchmark = Value;
		}
		else if (Argument == "--exercise-document")
		{
			Result.ExerciseDocument = Value;
		}
		else if (Argument == "--exercise-views")
		{
			Result.ExerciseViews = Value;
		}
		else if (Argument == "--exercise-placement")
		{
			Result.ExercisePlacement = Value;
		}
		else if (Argument == "--benchmark-warmup")
		{
			Result.BenchmarkWarmup = static_cast<std::uint32_t>(std::stoul(Value));
		}
		else if (Argument == "--benchmark-samples")
		{
			Result.BenchmarkSamples = static_cast<std::uint32_t>(std::stoul(Value));
		}
		else if (Argument == "--frames")
		{
			Result.Frames = static_cast<std::uint32_t>(std::stoul(Value));
		}
		else
		{
			throw std::invalid_argument("Unknown editor option: " + Argument);
		}
	}
	if ((!Result.Benchmark.empty() && (Result.Scene.empty() || !Result.BenchmarkSamples || Result.bExercise)) ||
	    (Result.bBenchmarkCamera && Result.Benchmark.empty()))
	{
		throw std::invalid_argument(
		    "Editor benchmark requires a scene and positive sample count; exercise is separate");
	}
	return Result;
}
} // namespace Hyperion
