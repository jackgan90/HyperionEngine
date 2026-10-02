#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include "SceneMaterialRoutes.h"
#include <stdexcept>

namespace Hyperion
{
namespace
{
void AddView(std::vector<FRenderView>& InViews, std::vector<FRenderPassTargets>& InTargets, FRenderView InMain,
             ESceneMaterialRoute InRoute, FRenderPassTargets InTarget, std::uint64_t InIdentity,
             ESceneRenderPipeline InPipeline)
{
	const auto& Route = DescribeSceneMaterialRoute(InRoute);
	if (!Route.Supports(InPipeline))
	{
		throw std::logic_error("Scene material route is unavailable in the selected pipeline: " +
		                       std::string(Route.Usage));
	}
	InMain.Identity = InIdentity;
	InMain.Usage = Route.Usage;
	InMain.bSkipMissingPass = true;
	InViews.push_back(std::move(InMain));
	InTargets.push_back(std::move(InTarget));
}
} // namespace

FSceneRayOptions MakeSceneRayOptions(ESceneRenderPipeline InPipeline)
{
	FSceneRayOptions Result;
	Result.MaterialUsages = ScenePickMaterialUsages(InPipeline);
	const auto& Legacy = DescribeSceneMaterialRoute(ESceneMaterialRoute::Legacy);
	Result.MaterialUsageExclusions[std::string(Legacy.Usage)] = SceneLegacyPassExclusions();
	return Result;
}

FSceneRenderPipeline::FViewFamily FSceneRenderPipeline::MakeViews(FRenderView InMain, FVec4 InClear) const
{
	FViewFamily Result;
	auto& Views = Result.Views;
	auto& Targets = Result.Targets;
	Views = ShadowMaps.Views(InMain);
	Targets = ShadowMaps.Targets(ShadowLifetime);
	Result.BaseIndex = Views.size();
	const auto MainLoad = InMain.Viewport ? EAttachmentLoad::Load : EAttachmentLoad::Clear;
	const bool bDeferred = Settings.Pipeline == ESceneRenderPipeline::Deferred;
	if (bDeferred)
	{
		FRenderPassTargets Base;
		Base.Name = "Deferred/BasePass";
		Base.DepthStencil = DepthTarget(MainLoad);
		for (const auto& Texture : GBuffer)
		{
			Base.Colors.push_back({{ERenderTargetKind::Texture, Texture, Lifetime, false}, {MainLoad}});
		}
		AddView(Views, Targets, InMain, ESceneMaterialRoute::DeferredBase, std::move(Base), 1, Settings.Pipeline);
	}
	else
	{
		InMain.Parameters.insert(InMain.Parameters.end(), ClusterParameters.begin(), ClusterParameters.end());
		auto Forward = ColorTargets("Forward/HDR", MainLoad, InClear);
		Forward.DepthStencil = DepthTarget(MainLoad);
		ShadowMaps.Bind(InMain, Forward, ShadowLifetime);
		AddView(Views, Targets, InMain, ESceneMaterialRoute::ForwardOpaque, std::move(Forward), 1, Settings.Pipeline);
	}
	if (bDeferred)
	{
		auto Compatibility = ColorTargets("Deferred/Compatibility", EAttachmentLoad::Load);
		Compatibility.DepthStencil = DepthTarget(EAttachmentLoad::Load);
		ShadowMaps.Bind(InMain, Compatibility, ShadowLifetime);
		AddView(Views, Targets, InMain, ESceneMaterialRoute::Compatibility, std::move(Compatibility), 2,
		        Settings.Pipeline);
	}
	auto Transparent = ColorTargets("Scene/Transparent", EAttachmentLoad::Load);
	if (bDeferred)
	{
		InMain.Parameters.insert(InMain.Parameters.end(), ClusterParameters.begin(), ClusterParameters.end());
	}
	Transparent.DepthStencil = DepthTarget(EAttachmentLoad::Load);
	ShadowMaps.Bind(InMain, Transparent, ShadowLifetime);
	AddView(Views, Targets, InMain, ESceneMaterialRoute::Transparent, std::move(Transparent), 3, Settings.Pipeline);
	Result.TransparentIndex = Views.size() - 1;
	auto DisplayView = InMain;
	DisplayView.ExcludedPasses = SceneLegacyPassExclusions();
	auto DisplayTargets = Session.FrameTargets({}, InMain.DepthConvention);
	if (OutputTarget.Kind == ERenderTargetKind::Texture)
	{
		DisplayTargets = OutputTargets();
		DisplayTargets.Color->View = EGraphColorView::DrawBatch;
		DisplayTargets.DepthStencil = DepthTarget(EAttachmentLoad::Clear);
	}
	DisplayTargets.Name = "Display/LegacyMaterials";
	AddView(Views, Targets, DisplayView, ESceneMaterialRoute::Legacy, std::move(DisplayTargets), 4, Settings.Pipeline);
	return Result;
}
} // namespace Hyperion
