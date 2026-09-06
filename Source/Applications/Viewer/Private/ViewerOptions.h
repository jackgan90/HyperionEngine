#pragma once
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/Config/AppSettings.h"
#include <optional>

namespace Hyperion
{
struct FOptions
{
	std::filesystem::path Config = std::filesystem::path(HYP_SOURCE_DIR) / "experiments/Triangle.json";
	std::filesystem::path Capture;
	std::filesystem::path SaveConfig;
	std::filesystem::path Model;
	bool bVerifyModel{};
	std::optional<std::string> Backend;
	int Frames{};
	bool bHidden{};
	bool bExercise{};
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
void ApplyOptions(const FOptions& InOptions, FAppSettings& InSettings);
void VerifyImage(const FImage& InImage, const FAppSettings& InSettings, const FOptions& InOptions);
} // namespace Hyperion
