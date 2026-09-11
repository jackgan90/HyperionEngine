#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include "Hyperion/Core/Profiling.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>

namespace Hyperion
{
namespace
{
FVec3 Direction(FRenderSession& InSession, const FMaterialFrameContext& InFrame)
{
	const auto Value = InSession.ResolveFrameSemantic(InFrame, "Engine.Scene.MainDirectionalLightDirection");
	if (!Value || Value->Type != FMaterialParameterType::Numeric(EMaterialScalar::Float, 3))
	{
		throw std::invalid_argument("Scene lighting direction must resolve from Global, Frame or Scene");
	}
	return {std::bit_cast<float>(Value->Words[0]), std::bit_cast<float>(Value->Words[1]),
	        std::bit_cast<float>(Value->Words[2])};
}

float ClearHdr(float InDisplay, float InExposure)
{
	const float Value = std::clamp(InDisplay, 0.f, 1.f);
	const float Linear = Value <= .04045f ? Value / 12.92f : std::pow((Value + .055f) / 1.055f, 2.4f);
	// Saturated display clears are approximated within one display code value.
	return std::min(Linear / std::max(1.f - Linear, .0001f) / InExposure, 65000.f);
}

} // namespace

void FSceneRenderPipeline::Build(FRenderGraph& InGraph, FRenderView InMain,
                                 std::shared_ptr<const FMaterialFrameContext> InFrame,
                                 const FCascadedShadowSettings& InShadows, FVec4 InClear,
                                 const std::function<void(FRenderGraph&)>& InExtensions, bool bInDeferPreparation)
{
	HYP_PERF_SCOPE_C(Render, ScenePipeline);
	const auto Start = std::chrono::steady_clock::now();
	if (!InFrame)
	{
		throw std::invalid_argument("Scene pipeline requires a frozen material frame");
	}
	if (Settings.Pipeline == ESceneRenderPipeline::Deferred && InMain.Viewport)
	{
		const auto& View = *InMain.Viewport;
		const float Range = View.MaxDepth - View.MinDepth;
		if (!(Range > 0) || !std::isfinite(1.f / Range))
		{
			throw std::invalid_argument("Deferred reconstruction requires a positive invertible viewport depth range");
		}
	}
	Resize(InMain.Width, InMain.Height);
	LastStatistics = {};
	FullscreenStatistics = std::make_shared<FFullscreenPreparationStatistics>();
	LastStatistics.Spatial = Session.GetScene().BeginViews();
	const auto Revision = Session.GetScene().GetCollectionRevision();
	const auto State =
	    Revision ? std::optional(std::array{*Revision, Session.GetResources().GetPublicationRevision()}) : std::nullopt;
	LastStatistics.bShadows = ShadowMaps.Prepare(
	    InMain, Direction(Session, *InFrame), InShadows,
	    [this](const ISceneVisibility& InVolume)
	    {
		    return Session.GetScene().QueryBounds(InVolume);
	    },
	    State);
	LastStatistics.ShadowSetupMilliseconds =
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	if (!ShadowLifetime || ShadowBytes != ShadowMaps.TextureBytes())
	{
		ShadowLifetime = Session.GetResources().CreateScopeLifetime();
		ShadowBytes = ShadowMaps.TextureBytes();
	}
	const FVec4 Clear{ClearHdr(InClear.X, Settings.Exposure), ClearHdr(InClear.Y, Settings.Exposure),
	                  ClearHdr(InClear.Z, Settings.Exposure), InClear.W};
	if (InMain.Viewport)
	{
		ClearTargets(InGraph, Clear);
	}
	const bool bDeferred = Settings.Pipeline == ESceneRenderPipeline::Deferred;
	auto Family = MakeViews(InMain, Clear);
	Session.BuildViews(InGraph, Family.Views, Family.Targets, InFrame, 1, true, bInDeferPreparation,
	                   [&](std::size_t InIndex)
	                   {
		                   if (bDeferred && InIndex == Family.BaseIndex)
		                   {
			                   AddFullscreenPass(Session, InGraph, Lighting(InMain, *InFrame, Clear),
			                                     bInDeferPreparation);
		                   }
		                   if (InIndex == Family.TransparentIndex)
		                   {
			                   AddFullscreenPass(Session, InGraph, Tonemap(InMain), bInDeferPreparation);
		                   }
	                   });
	bPending = bInDeferPreparation;
	if (bDeferred && Settings.DebugMode)
	{
		AddFullscreenPass(Session, InGraph, Debug(InMain), bInDeferPreparation);
	}
	if (LastStatistics.bShadows && InShadows.DebugMode >= 2 && InShadows.DebugMode <= 5)
	{
		const float Size = std::min({320.f, float(InMain.Width), float(InMain.Height)});
		Session.AppendDepthPreview(
		    InGraph, Family.Targets[Family.TransparentIndex].Reads.at(InShadows.DebugMode - 2).Texture, ShadowLifetime,
		    InShadows.PreviewViewport.value_or(
		        FViewport{float(InMain.Width) - Size, float(InMain.Height) - Size, Size, Size}),
		    bInDeferPreparation);
	}
	LastStatistics.Views = Session.ViewStatistics();
	LastStatistics.ShadowTextureBytes = ShadowMaps.TextureBytes();
	LastStatistics.SceneTargetBytes = TargetBytes();
	LastStatistics.PreparationMilliseconds =
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	if (InExtensions)
	{
		InExtensions(InGraph);
	}
}

FForwardFrame FSceneRenderPipeline::GetFrame() const
{
	FForwardFrame Result;
	Result.Base = LastStatistics;
	Result.Fullscreen = FullscreenStatistics;
	Result.bDeferred = bPending;
	if (bPending)
	{
		Result.Preparation = Session.GetViewPreparation();
	}
	return Result;
}
} // namespace Hyperion
