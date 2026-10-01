#pragma once
#include "Hyperion/Renderer/ShaderParameters/ContactShadowParameters.h"
#include "Hyperion/Renderer/ShaderParameters/DeferredLightingParameters.h"
#include "Hyperion/Renderer/ShaderParameters/DepthPreviewParameters.h"
#include "Hyperion/Renderer/ShaderParameters/HierarchicalDepthParameters.h"
#include "Hyperion/Renderer/ShaderParameters/OutlineParameters.h"
#include "Hyperion/Renderer/ShaderParameters/OutputParameters.h"
#include "Hyperion/Renderer/ShaderParameters/SkyParameters.h"
#include "Support/ShaderSourceSupport.h"

namespace Hyperion
{
inline const FShaderParameterContracts& TestShaderContracts()
{
	static const FShaderParameterContracts Contracts{GetDeferredLightingShaderContracts(),
	                                                 GetContactShadowShaderContracts(),
	                                                 GetDepthPreviewShaderContracts(),
	                                                 GetHierarchicalDepthShaderContracts(),
	                                                 GetOutlineShaderContracts(),
	                                                 GetOutputShaderContracts(),
	                                                 GetSkyShaderContracts()};
	return Contracts;
}

inline const std::filesystem::path& RendererTestShaderRoot()
{
	static const auto Root = []
	{
		const auto Directory = TestShaderRoot();
		FLocalFileSystem Files;
		for (const auto& Include : GenerateShaderIncludes(TestShaderContracts()))
		{
			Files.WriteAtomic(Directory / Include.first, std::as_bytes(std::span(Include.second)));
		}
		return Directory;
	}();
	return Root;
}
} // namespace Hyperion
