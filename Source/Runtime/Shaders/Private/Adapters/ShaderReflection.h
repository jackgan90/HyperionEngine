#pragma once
#include "Hyperion/Shaders/ShaderCompiler.h"

namespace Hyperion::ShadersPrivate
{
void ReflectDxil(FShaderArtifact& InArtifact, const std::string& InPayload);
void ReflectSpirv(FShaderArtifact& InArtifact, std::string& InPayload, const FShaderArtifact& InLogical);
void ValidateShaderBindings(const FShaderArtifact& InArtifact);
} // namespace Hyperion::ShadersPrivate
