#pragma once
#include "Hyperion/Gui/Gui.h"
#include "Hyperion/Scene/SceneLight.h"

namespace Hyperion
{
struct FSkyControlResult
{
	bool bChanged{};
	// Logical bounds support normalized input acceptance, including high DPI.
	FVec4 AssetBounds;
	FVec4 PathBounds;
	FVec4 ApplyBounds;
	FVec4 VisibilityBounds;
};

FSkyControlResult DrawSkyAssetControls(FGui& InGui, FSceneEnvironmentLight& InLight, std::string& InPath,
                                       std::span<const std::string> InChoices);
} // namespace Hyperion
