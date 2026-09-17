#include "PipelineShadows.h"
#include "Hyperion/Renderer/RenderSession.h"

namespace Hyperion
{
bool PreparePipelineShadows(FRenderSession& InSession, FCascadedShadowMap& InShadowMaps, const FRenderView& InMain,
                            const FMaterialFrameContext& InFrame, const FCascadedShadowSettings& InSettings,
                            FVec3 InDirection)
{
	const auto Revision = InSession.GetScene().GetCollectionRevision();
	const auto State = Revision
	                       ? std::optional(std::array{*Revision, InSession.GetResources().GetPublicationRevision()})
	                       : std::nullopt;
	auto Effective = InSettings;
	Effective.bEnabled &= !InFrame.GetSceneToken() || InFrame.CastsSceneShadows();
	return InShadowMaps.Prepare(
	    InMain, InDirection, Effective,
	    [&InSession](const ISceneVisibility& InVolume)
	    {
		    return InSession.GetScene().QueryBounds(InVolume);
	    },
	    State);
}

void UpdatePipelineShadowLifetime(FRenderResourceService& InResources, const FCascadedShadowMap& InShadowMaps,
                                  EDepthConvention InDepthConvention, std::shared_ptr<const void>& OutLifetime,
                                  std::uint64_t& OutBytes, EDepthConvention& OutDepthConvention)
{
	if (!OutLifetime || OutBytes != InShadowMaps.TextureBytes() || OutDepthConvention != InDepthConvention)
	{
		// Submitted frames retain the previous attachment set until their users complete.
		OutLifetime = InResources.CreateScopeLifetime();
		OutBytes = InShadowMaps.TextureBytes();
		OutDepthConvention = InDepthConvention;
	}
}
} // namespace Hyperion
