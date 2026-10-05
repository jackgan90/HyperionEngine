#pragma once
#include "Hyperion/RenderControls/RenderSettings.h"
#include "Hyperion/Renderer/SceneRenderPipeline.h"

namespace Hyperion
{
FScenePipelineSettings MakePipelineSettings(const FRenderSettings& InSettings);
FRenderSettings LoadRenderSettings(const std::filesystem::path& InPath);
void WriteRenderSettings(const std::filesystem::path& InPath, const FRenderSettings& InSettings);
} // namespace Hyperion
