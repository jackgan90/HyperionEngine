#pragma once
#include "Hyperion/Renderer/ModelPreparation.h"
#include "Hyperion/Renderer/RenderResources.h"

namespace Hyperion
{
std::vector<FRenderMaterialDesc> PrepareModelMaterials(const FPreparedModel& InModel, FShaderCompiler& InCompiler,
                                                       EShaderFormat InFormat);
std::vector<FVertexAttribute> ModelVertexAttributes();
} // namespace Hyperion
