#pragma once
#include "Hyperion/Materials/MaterialParameters.h"
#include "Hyperion/Renderer/RenderPass.h"

namespace Hyperion
{
class FRenderSession;
class FMaterialDefinition;

// Render publishes the complete inputs; RHI preparation owns all captured values.
struct FFullscreenPreparationStatistics
{
	double Milliseconds{};
	std::size_t Draws{};
};

struct FFullscreenPassDesc
{
	std::shared_ptr<const FMaterialDefinition> Material;
	FMaterialParameterValues Parameters;
	FRenderPassTargets Targets;
	FViewport Viewport;
	bool bFullTargetViewport{}; // Omit graph viewport when initializing the whole attachment.
	std::shared_ptr<FFullscreenPreparationStatistics> Statistics;
	std::shared_ptr<const void> Lifetime;
};

std::shared_ptr<const FMaterialDefinition> MakeFullscreenMaterial(std::string InName, std::string InPixelShader,
                                                                  bool bInSrgb = false);
void AddFullscreenPass(FRenderSession& InSession, FRenderGraph& InGraph, FFullscreenPassDesc InPass,
                       bool bInDeferPreparation = true);
} // namespace Hyperion
