#pragma once
#include "Hyperion/Materials/ShaderParameters.h"
#define HYP_SHADER_DOMAIN DeferredLighting
#define HYP_SHADER_DECLARATIONS "Hyperion/Renderer/ShaderParameters/DeferredLightingParameters.inl"
#include "Hyperion/Materials/ShaderParameterDeclarations.inl"
#undef HYP_SHADER_DECLARATIONS
#undef HYP_SHADER_DOMAIN
