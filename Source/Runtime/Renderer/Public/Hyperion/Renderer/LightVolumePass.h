#pragma once
#include "Hyperion/Renderer/LocalLights.h"
#include "Hyperion/Renderer/RenderPrimitive.h"

namespace Hyperion
{
struct FLightVolumePassDesc
{
	FRenderView View;
	std::vector<FLocalLight> Lights;
	std::array<std::shared_ptr<const FMaterialTextureSource>, 4> GBuffer;
	std::shared_ptr<const FMaterialTextureSource> Depth;
	FRenderPassTargets Targets;
	std::shared_ptr<const void> Lifetime;
	// Assigned by AppendLightVolumes; retires numeric cache entries independently of target/PSO lifetime.
	std::shared_ptr<const void> ConstantLifetime;
};
} // namespace Hyperion
