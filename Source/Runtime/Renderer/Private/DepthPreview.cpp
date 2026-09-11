#include "Hyperion/Renderer/FullscreenPass.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "RenderResourcesInternal.h"

namespace Hyperion
{
namespace
{
FFullscreenPassDesc PreviewPass(std::shared_ptr<const FMaterialTextureSource> InSource,
                                std::shared_ptr<const void> InLifetime, FViewport InViewport)
{
	static const auto Material = MakeFullscreenMaterial("Depth preview", "DepthPreview.hlsl");
	FFullscreenPassDesc Pass;
	Pass.Material = Material;
	Pass.Lifetime = std::move(InLifetime);
	Pass.Viewport = InViewport;
	Pass.Targets = FRenderPassTargets::ColorOnly();
	Pass.Targets.Name = "Shadow depth preview";
	Pass.Targets.Reads = {{ERenderTargetKind::Texture, InSource, Pass.Lifetime, false}};
	Pass.Parameters = {{"Pixel:DepthMap", FMaterialValue::FromTexture(std::move(InSource))}};
	return Pass;
}
} // namespace

FGraphicsDrawBatch FRenderResourceService::BuildDepthPreview(std::shared_ptr<const FMaterialTextureSource> InSource,
                                                             std::shared_ptr<const void> InLifetime,
                                                             FViewport InViewport)
{
	return GetPreparation().BuildDepthPreview(std::move(InSource), std::move(InLifetime), InViewport);
}

FGraphicsDrawBatch FRenderResourcePreparation::BuildDepthPreview(std::shared_ptr<const FMaterialTextureSource> InSource,
                                                                 std::shared_ptr<const void> InLifetime,
                                                                 FViewport InViewport) const
{
	if (!InSource || !InSource->GetDepthTarget())
	{
		throw std::invalid_argument("Depth preview requires a depth source");
	}
	return BuildFullscreen(PreviewPass(std::move(InSource), std::move(InLifetime), InViewport));
}

void FRenderSession::AppendDepthPreview(FRenderGraph& InGraph, std::shared_ptr<const FMaterialTextureSource> InSource,
                                        std::shared_ptr<const void> InLifetime, FViewport InViewport, bool bInDeferred)
{
	AddFullscreenPass(*this, InGraph, PreviewPass(std::move(InSource), std::move(InLifetime), InViewport), bInDeferred);
}
} // namespace Hyperion
