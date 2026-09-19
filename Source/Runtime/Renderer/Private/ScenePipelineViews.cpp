#include "Hyperion/Renderer/SceneRenderPipeline.h"

namespace Hyperion
{
namespace
{
std::vector<std::string> LegacyPassExclusions()
{
	return {"HdrForwardOpaque", "DeferredBase", "HdrCompatibility", "HdrTransparent"};
}

void AddView(std::vector<FRenderView>& InViews, std::vector<FRenderPassTargets>& InTargets, FRenderView InMain,
             std::string InUsage, FRenderPassTargets InTarget, std::uint64_t InIdentity)
{
	InMain.Identity = InIdentity;
	InMain.Usage = std::move(InUsage);
	InMain.bSkipMissingPass = true;
	InViews.push_back(std::move(InMain));
	InTargets.push_back(std::move(InTarget));
}
} // namespace

FSceneRayOptions MakeSceneRayOptions(ESceneRenderPipeline InPipeline)
{
	FSceneRayOptions Result;
	Result.MaterialUsages =
	    InPipeline == ESceneRenderPipeline::Deferred
	        ? std::vector<std::string>{"DeferredBase", "HdrCompatibility", "HdrTransparent", "Forward"}
	        : std::vector<std::string>{"HdrForwardOpaque", "HdrTransparent", "Forward"};
	Result.MaterialUsageExclusions["Forward"] = LegacyPassExclusions();
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
		AddView(Views, Targets, InMain, "DeferredBase", std::move(Base), 1);
	}
	else
	{
		InMain.Parameters.insert(InMain.Parameters.end(), ClusterParameters.begin(), ClusterParameters.end());
		auto Forward = ColorTargets("Forward/HDR", MainLoad, InClear);
		Forward.DepthStencil = DepthTarget(MainLoad);
		ShadowMaps.Bind(InMain, Forward, ShadowLifetime);
		AddView(Views, Targets, InMain, "HdrForwardOpaque", std::move(Forward), 1);
	}
	if (bDeferred)
	{
		auto Compatibility = ColorTargets("Deferred/Compatibility", EAttachmentLoad::Load);
		Compatibility.DepthStencil = DepthTarget(EAttachmentLoad::Load);
		ShadowMaps.Bind(InMain, Compatibility, ShadowLifetime);
		AddView(Views, Targets, InMain, "HdrCompatibility", std::move(Compatibility), 2);
	}
	auto Transparent = ColorTargets("Scene/Transparent", EAttachmentLoad::Load);
	if (bDeferred)
	{
		InMain.Parameters.insert(InMain.Parameters.end(), ClusterParameters.begin(), ClusterParameters.end());
	}
	Transparent.DepthStencil = DepthTarget(EAttachmentLoad::Load);
	ShadowMaps.Bind(InMain, Transparent, ShadowLifetime);
	AddView(Views, Targets, InMain, "HdrTransparent", std::move(Transparent), 3);
	Result.TransparentIndex = Views.size() - 1;
	auto DisplayView = InMain;
	DisplayView.ExcludedPasses = LegacyPassExclusions();
	auto DisplayTargets = Session.FrameTargets({}, InMain.DepthConvention);
	if (OutputTarget.Kind == ERenderTargetKind::Texture)
	{
		DisplayTargets = OutputTargets();
		DisplayTargets.Color->View = EGraphColorView::DrawBatch;
		DisplayTargets.DepthStencil = DepthTarget(EAttachmentLoad::Clear);
	}
	DisplayTargets.Name = "Display/LegacyMaterials";
	AddView(Views, Targets, DisplayView, "Forward", std::move(DisplayTargets), 4);
	return Result;
}
} // namespace Hyperion
