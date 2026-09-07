#include "ViewerOptions.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Hyperion
{
namespace
{
bool ParseApplicationOption(FOptions& InOptions, const std::string& InArg, int InArgc, char** InArgv, int& InIndex)
{
	if (InArg == "--frames" && InIndex + 1 < InArgc)
	{
		InOptions.Frames = std::stoi(InArgv[++InIndex]);
	}
	else if (InArg == "--config" && InIndex + 1 < InArgc)
	{
		InOptions.Config = InArgv[++InIndex];
	}
	else if (InArg == "--capture" && InIndex + 1 < InArgc)
	{
		InOptions.Capture = InArgv[++InIndex];
	}
	else if (InArg == "--model" && InIndex + 1 < InArgc)
	{
		InOptions.Model = InArgv[++InIndex];
	}
	else if (InArg == "--scene" && InIndex + 1 < InArgc)
	{
		InOptions.Scene = InArgv[++InIndex];
	}
	else if (InArg == "--scene-culling" && InIndex + 1 < InArgc)
	{
		InOptions.SceneCulling = InArgv[++InIndex];
	}
	else if (InArg == "--verify-model")
	{
		InOptions.bVerifyModel = true;
	}
	else if (InArg == "--save-config" && InIndex + 1 < InArgc)
	{
		InOptions.SaveConfig = InArgv[++InIndex];
	}
	else if (InArg == "--backend" && InIndex + 1 < InArgc)
	{
		InOptions.Backend = InArgv[++InIndex];
	}
	else if (InArg == "--hidden")
	{
		InOptions.bHidden = true;
	}
	else if (InArg == "--exercise-window")
	{
		InOptions.bExercise = true;
	}
	else if (InArg == "--verify-clear")
	{
		InOptions.bVerifyClear = true;
	}
	else if (InArg == "--verify-triangle")
	{
		InOptions.bVerifyTriangle = true;
	}
	else if (InArg == "--verify-ui")
	{
		InOptions.bVerifyUi = true;
	}
	else if (InArg == "--no-ui")
	{
		InOptions.bNoUi = true;
	}
	else
	{
		return false;
	}
	return true;
}

bool ParseCaptureOption(FOptions& InOptions, const std::string& InArg, int InArgc, char** InArgv, int& InIndex)
{
	if (InArg == "--renderdoc")
	{
		InOptions.bRenderDoc = true;
	}
	else if (InArg == "--renderdoc-library" && InIndex + 1 < InArgc)
	{
		InOptions.RenderDocLibrary = InArgv[++InIndex];
		InOptions.bRenderDoc = true;
	}
	else if (InArg == "--rdc-output" && InIndex + 1 < InArgc)
	{
		InOptions.RdcOutput = InArgv[++InIndex];
	}
	else if (InArg == "--capture-rdc" && InIndex + 1 < InArgc)
	{
		InOptions.RdcFrames.push_back(std::stoi(InArgv[++InIndex]));
		InOptions.bRenderDoc = true;
	}
	else if (InArg == "--open-rdc")
	{
		InOptions.bOpenRdc = true;
		InOptions.bRenderDoc = true;
	}
	else if (InArg == "--exercise-rdc-ui")
	{
		InOptions.bExerciseRdcUi = true;
	}
	else
	{
		return false;
	}
	return true;
}

void ValidateOptions(FOptions& InOptions)
{
	if ((!InOptions.Model.empty() && !InOptions.Scene.empty()) ||
	    (InOptions.SceneCulling != "none" && InOptions.SceneCulling != "linear" && InOptions.SceneCulling != "bvh"))
	{
		throw std::invalid_argument("Choose --model or --scene; --scene-culling accepts none, linear or bvh");
	}
	if (InOptions.Frames < 0)
	{
		throw std::invalid_argument("--frames must be nonnegative");
	}
	if ((!InOptions.Capture.empty() || InOptions.bVerifyClear || InOptions.bVerifyTriangle || InOptions.bVerifyUi ||
	     InOptions.bVerifyModel) &&
	    InOptions.Frames == 0)
	{
		throw std::invalid_argument("Capture verification requires a bounded --frames run");
	}
	if ((InOptions.bVerifyClear || InOptions.bVerifyTriangle || InOptions.bVerifyUi || InOptions.bVerifyModel) &&
	    InOptions.Capture.empty())
	{
		throw std::invalid_argument("Verification requires --capture");
	}
	std::sort(InOptions.RdcFrames.begin(), InOptions.RdcFrames.end());
	for (std::size_t I = 0; I < InOptions.RdcFrames.size(); ++I)
	{
		if (InOptions.RdcFrames[I] < 1 || !InOptions.Frames ||
		    InOptions.RdcFrames[I] + int(InOptions.bExerciseRdcUi) > InOptions.Frames ||
		    (I && InOptions.RdcFrames[I] <= InOptions.RdcFrames[I - 1] + int(InOptions.bExerciseRdcUi)))
		{
			throw std::invalid_argument("--capture-rdc requires distinct positive frame numbers within --frames (UI "
			                            "exercise needs one release frame)");
		}
	}
	if (InOptions.bExerciseRdcUi && (InOptions.RdcFrames.empty() || InOptions.bNoUi))
	{
		throw std::invalid_argument("--exercise-rdc-ui requires --capture-rdc and the debug UI");
	}
}
} // namespace

FOptions ParseOptions(int InArgc, char** InArgv)
{
	FOptions Options;
	for (int Index = 1; Index < InArgc; ++Index)
	{
		const std::string Arg = InArgv[Index];
		if (!ParseApplicationOption(Options, Arg, InArgc, InArgv, Index) &&
		    !ParseCaptureOption(Options, Arg, InArgc, InArgv, Index))
		{
			throw std::invalid_argument("Unknown or incomplete option: " + Arg);
		}
	}
	ValidateOptions(Options);
	return Options;
}

void ApplyOptions(const FOptions& InOptions, FAppSettings& InSettings)
{
	if (!InOptions.Scene.empty())
	{
		InSettings.SceneSource = InOptions.Scene.string();
		InSettings.ModelSource.clear();
	}
	if (!InOptions.Model.empty())
	{
		InSettings.ModelSource = InOptions.Model.string();
		InSettings.SceneSource.clear();
	}
	if (!InSettings.SceneSource.empty() && !InSettings.ModelSource.empty())
	{
		throw std::invalid_argument("Only one Viewer camera owner may be selected: scene_source or model_source");
	}
	if (!InSettings.SceneSource.empty())
	{
		std::erase(InSettings.Plugins, std::string("triangle"));
		std::erase(InSettings.Plugins, std::string("model-viewer"));
		if (std::find(InSettings.Plugins.begin(), InSettings.Plugins.end(), "scene-viewer") == InSettings.Plugins.end())
		{
			InSettings.Plugins.insert(InSettings.Plugins.begin(), "scene-viewer");
		}
	}
	if (!InSettings.ModelSource.empty())
	{
		std::erase(InSettings.Plugins, std::string("scene-viewer"));
		std::erase(InSettings.Plugins, std::string("triangle"));
		if (std::find(InSettings.Plugins.begin(), InSettings.Plugins.end(), "model-viewer") == InSettings.Plugins.end())
		{
			InSettings.Plugins.insert(InSettings.Plugins.begin(), "model-viewer");
		}
	}
	if (InOptions.Backend)
	{
		InSettings.RHIBackend = *InOptions.Backend;
	}
	if (InOptions.RenderDocLibrary)
	{
		InSettings.RenderDocLibrary = *InOptions.RenderDocLibrary;
	}
	if (InOptions.RdcOutput)
	{
		InSettings.RenderDocOutput = *InOptions.RdcOutput;
	}
	InSettings.bRenderDocAutoOpen = InSettings.bRenderDocAutoOpen || InOptions.bOpenRdc;
	if (InOptions.bRenderDoc &&
	    std::find(InSettings.Plugins.begin(), InSettings.Plugins.end(), "renderdoc") == InSettings.Plugins.end())
	{
		InSettings.Plugins.push_back("renderdoc");
	}
}

void VerifyImage(const FImage& InImage, const FAppSettings& InSettings, const FOptions& InOptions)
{
	if (InOptions.bVerifyModel)
	{
		std::size_t Colored{};
		for (std::size_t Index = 0; Index < InImage.Rgba.size(); Index += 4)
		{
			if (std::max({InImage.Rgba[Index], InImage.Rgba[Index + 1], InImage.Rgba[Index + 2]}) > .22f)
			{
				++Colored;
			}
		}
		if (Colored < std::size_t(InImage.Width) * InImage.Height / 40)
		{
			throw std::runtime_error("Model readback has insufficient visible geometry");
		}
	}
	if (InOptions.bVerifyClear)
	{
		for (std::size_t I = 0; I < InImage.Rgba.size(); I += 4)
		{
			if (std::abs(InImage.Rgba[I] - InSettings.ClearRed) > 1.0 / 255 ||
			    std::abs(InImage.Rgba[I + 1] - InSettings.ClearGreen) > 1.0 / 255 ||
			    std::abs(InImage.Rgba[I + 2] - InSettings.ClearBlue) > 1.0 / 255)
			{
				throw std::runtime_error("Clear readback mismatch");
			}
		}
	}
	if (InOptions.bVerifyTriangle)
	{
		auto Center = (std::size_t(InImage.Height / 2) * InImage.Width + InImage.Width / 2) * 4;
		auto Corner = (std::size_t(10) * InImage.Width + 10) * 4;
		if (InImage.Rgba[Center] < .15f || InImage.Rgba[Center + 1] < .15f || InImage.Rgba[Center + 2] < .15f ||
		    std::abs(InImage.Rgba[Corner] - InSettings.ClearRed) > 1.0 / 255)
		{
			throw std::runtime_error("Triangle readback mismatch");
		}
	}
	if (InOptions.bVerifyUi)
	{
		std::size_t Bright{};
		for (std::uint32_t Y = 30; Y < std::min(InImage.Height, 650u); ++Y)
		{
			for (std::uint32_t X = 30; X < std::min(InImage.Width, 340u); ++X)
			{
				auto I = (std::size_t(Y) * InImage.Width + X) * 4;
				if (InImage.Rgba[I] > .3f && InImage.Rgba[I + 1] > .3f)
				{
					++Bright;
				}
			}
		}
		if (Bright < 500)
		{
			throw std::runtime_error("GUI text/controls missing from GPU readback");
		}
	}
}
} // namespace Hyperion
