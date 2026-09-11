#include "Hyperion/Renderer/ForwardRenderPipeline.h"
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/FullscreenPass.h"
#include <bit>
#include <chrono>
#include <stdexcept>

namespace Hyperion
{
FForwardRenderPipeline::FForwardRenderPipeline(FRenderSession& InSession)
    : Session(InSession), Lifetime(InSession.GetResources().CreateScopeLifetime())
{
}

void FForwardRenderPipeline::Build(FRenderGraph& InGraph, FRenderView InMain,
                                   std::shared_ptr<const FMaterialFrameContext> InFrame,
                                   const FCascadedShadowSettings& InShadows, FVec4 InClear,
                                   const std::function<void(FRenderGraph&)>& InExtensions, bool bInDeferPreparation)
{
	HYP_PERF_SCOPE_C(Render, ForwardPipeline);
	const auto Start = std::chrono::steady_clock::now();
	if (!InFrame)
	{
		throw std::invalid_argument("Forward pipeline requires a frozen material frame");
	}
	LastStatistics = {};
	LastStatistics.Spatial = Session.GetScene().BeginViews();
	const auto Direction = Session.ResolveFrameSemantic(*InFrame, "Engine.Scene.MainDirectionalLightDirection");
	FVec3 Light;
	if (Direction && Direction->Type == FMaterialParameterType::Numeric(EMaterialScalar::Float, 3))
	{
		Light = {std::bit_cast<float>(Direction->Words[0]), std::bit_cast<float>(Direction->Words[1]),
		         std::bit_cast<float>(Direction->Words[2])};
	}
	const auto SceneRevision = Session.GetScene().GetCollectionRevision();
	const auto SceneState =
	    SceneRevision ? std::optional(std::array{*SceneRevision, Session.GetResources().GetPublicationRevision()})
	                  : std::nullopt;
	LastStatistics.bShadows = ShadowMaps.Prepare(
	    InMain, Light, InShadows,
	    [this](const ISceneVisibility& InVolume)
	    {
		    return Session.GetScene().QueryBounds(InVolume);
	    },
	    SceneState);
	LastStatistics.ShadowSetupMilliseconds =
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	if (AllocatedShadowBytes != ShadowMaps.TextureBytes() || ShadowDepthConvention != InMain.DepthConvention)
	{
		// Replacing resolution retires the previous attachment set after its submitted users complete.
		Lifetime = Session.GetResources().CreateScopeLifetime();
		AllocatedShadowBytes = ShadowMaps.TextureBytes();
		ShadowDepthConvention = InMain.DepthConvention;
	}
	auto Views = ShadowMaps.Views(InMain);
	auto Targets = ShadowMaps.Targets(Lifetime);
	auto MainTargets = Session.FrameTargets({}, InMain.DepthConvention);
	MainTargets.Color->Actions.Load = EAttachmentLoad::Clear;
	MainTargets.Color->Clear = InClear;
	MainTargets.Name = "Forward";
	ShadowMaps.Bind(InMain, MainTargets, Lifetime);
	Targets.push_back(std::move(MainTargets));
	Views.push_back(std::move(InMain));
	Session.BuildViews(InGraph, Views, Targets, std::move(InFrame), 1, true, bInDeferPreparation);
	bPending = bInDeferPreparation;
	if (LastStatistics.bShadows && InShadows.DebugMode >= 2 && InShadows.DebugMode <= 5)
	{
		const auto& Main = Views.back();
		const float Size = std::min({320.f, float(Main.Width), float(Main.Height)});
		Session.AppendDepthPreview(InGraph, Targets.back().Reads.at(InShadows.DebugMode - 2).Texture, Lifetime,
		                           InShadows.PreviewViewport.value_or(
		                               FViewport{float(Main.Width) - Size, float(Main.Height) - Size, Size, Size}),
		                           bInDeferPreparation);
	}
	LastStatistics.Views = Session.ViewStatistics();
	LastStatistics.ShadowTextureBytes = ShadowMaps.TextureBytes();
	LastStatistics.PreparationMilliseconds =
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	if (InExtensions)
	{
		InExtensions(InGraph);
	}
}

FSceneVisibilityStats FForwardPipelineStatistics::MainView() const
{
	FSceneVisibilityStats Result;
	bool bFirst = true;
	for (const auto& View : Views)
	{
		if (View.Usage == "ShadowDepth")
		{
			continue;
		}
		if (bFirst)
		{
			Result = View.Visibility;
			Result.VisibleItems = 0;
			Result.Draws = 0;
			Result.Batches = {};
			bFirst = false;
		}
		Result.VisibleItems += View.Visibility.VisibleItems;
		Result.Draws += View.Visibility.Draws;
		Result.Batches += View.Visibility.Batches;
	}
	return Result;
}

FForwardPipelineStatistics FForwardFrame::Statistics() const
{
	auto Result = Base;
	if (bDeferred)
	{
		const auto Family = Preparation.Statistics();
		Result.PreparationMilliseconds += Family.Milliseconds;
		Result.Views = Family.Views;
	}
	if (Fullscreen)
	{
		Result.FullscreenPreparationMilliseconds = Fullscreen->Milliseconds;
		Result.FullscreenDraws = Fullscreen->Draws;
		Result.PreparationMilliseconds += Fullscreen->Milliseconds;
	}
	return Result;
}

FForwardFrame FForwardRenderPipeline::GetFrame() const
{
	FForwardFrame Result;
	Result.Base = LastStatistics;
	Result.bDeferred = bPending;
	if (bPending)
	{
		Result.Preparation = Session.GetViewPreparation();
	}
	return Result;
}

void FForwardRenderPipeline::Complete()
{
	if (bPending)
	{
		LastStatistics.PreparationMilliseconds += Session.CompleteViews();
		LastStatistics.Views = Session.ViewStatistics();
		bPending = false;
	}
}

const FForwardPipelineStatistics& FForwardRenderPipeline::Statistics() const
{
	return LastStatistics;
}

const FCascadedShadowMap& FForwardRenderPipeline::Shadows() const
{
	return ShadowMaps;
}
} // namespace Hyperion
