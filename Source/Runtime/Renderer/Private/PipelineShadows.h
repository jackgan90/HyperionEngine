#pragma once
#include "Hyperion/Renderer/CascadedShadowMap.h"

namespace Hyperion
{
class FRenderSession;
class FRenderResourceService;

bool PreparePipelineShadows(FRenderSession& InSession, FCascadedShadowMap& InShadowMaps, const FRenderView& InMain,
                            const FMaterialFrameContext& InFrame, const FCascadedShadowSettings& InSettings,
                            FVec3 InDirection);
void UpdatePipelineShadowLifetime(FRenderResourceService& InResources, const FCascadedShadowMap& InShadowMaps,
                                  EDepthConvention InDepthConvention, std::shared_ptr<const void>& OutLifetime,
                                  std::uint64_t& OutBytes, EDepthConvention& OutDepthConvention);
} // namespace Hyperion
