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
	if (Session.GetScene().GetLogicalSceneIdentity())
	{
		throw std::logic_error("Bound scene pipeline requires a scene view request and frame seed");
	}
	BuildResolved(InGraph, std::move(InMain), std::move(InFrame), InShadows, InClear, InExtensions,
	              bInDeferPreparation);
}

void FSceneRenderPipeline::Build(FRenderGraph& InGraph, const FSceneViewRequest& InRequest,
                                 std::shared_ptr<const FSceneFrameSeed> InSeed,
                                 const FCascadedShadowSettings& InShadows, FVec4 InClear,
                                 const std::function<void(FRenderGraph&)>& InExtensions, bool bInDeferPreparation)
{
	if (!InSeed)
	{
		throw std::invalid_argument("Scene pipeline requires an owned frame seed");
	}
	const auto Resolved = Session.ResolveSceneFrame(*InSeed, InRequest);
	if (Resolved.HasCamera())
	{
		BuildResolved(InGraph, Resolved.View, Resolved.Frame, InShadows, InClear, InExtensions, bInDeferPreparation);
	}
	else
	{
		LastStatistics = {};
		bPending = false;
		FullscreenStatistics.reset();
		auto Disabled = InShadows;
		Disabled.bEnabled = false;
		ShadowMaps.Prepare(Resolved.View, {0, 0, 1}, Disabled, {});
		Session.BuildSceneClear(InGraph, Resolved, InClear);
		if (InExtensions)
		{
			InExtensions(InGraph);
		}
	}
	LastStatistics.SceneToken = InSeed->GetToken();
	LastStatistics.CameraStatus = Resolved.CameraStatus;
	LastStatistics.MainCameraView = Resolved.HasCamera() ? std::optional(Resolved.View) : std::nullopt;
}

void FSceneRenderPipeline::BuildResolved(FRenderGraph& InGraph, FRenderView InMain,
                                         std::shared_ptr<const FMaterialFrameContext> InFrame,
                                         const FCascadedShadowSettings& InShadows, FVec4 InClear,
                                         const std::function<void(FRenderGraph&)>& InExtensions,
                                         bool bInDeferPreparation)
{
	HYP_PERF_SCOPE_C(Render, ScenePipeline);
	const auto Start = std::chrono::steady_clock::now();
	if (!InFrame)
	{
		throw std::invalid_argument("Scene pipeline requires a frozen material frame");
	}
	Session.ValidateSceneFrame(*InFrame);
	if (Settings.Pipeline == ESceneRenderPipeline::Deferred && InMain.Viewport)
	{
		const auto& View = *InMain.Viewport;
		const float Range = View.MaxDepth - View.MinDepth;
		if (!(Range > 0) || !std::isfinite(1.f / Range))
		{
			throw std::invalid_argument("Deferred reconstruction requires a positive invertible viewport depth range");
		}
	}
	Resize(InMain.Width, InMain.Height, InMain.DepthConvention);
	LastStatistics = {};
	FullscreenStatistics = std::make_shared<FFullscreenPreparationStatistics>();
	LastStatistics.Spatial = Session.GetScene().BeginViews();
	const auto Revision = Session.GetScene().GetCollectionRevision();
	const auto State =
	    Revision ? std::optional(std::array{*Revision, Session.GetResources().GetPublicationRevision()}) : std::nullopt;
	auto EffectiveShadows = InShadows;
	EffectiveShadows.bEnabled &= !InFrame->GetSceneToken() || InFrame->CastsSceneShadows();
	LastStatistics.bShadows = ShadowMaps.Prepare(
	    InMain, Direction(Session, *InFrame), EffectiveShadows,
	    [this](const ISceneVisibility& InVolume)
	    {
		    return Session.GetScene().QueryBounds(InVolume);
	    },
	    State);
	LastStatistics.ShadowSetupMilliseconds =
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	if (!ShadowLifetime || ShadowBytes != ShadowMaps.TextureBytes() || ShadowDepthConvention != InMain.DepthConvention)
	{
		ShadowLifetime = Session.GetResources().CreateScopeLifetime();
		ShadowBytes = ShadowMaps.TextureBytes();
		ShadowDepthConvention = InMain.DepthConvention;
	}
	const FVec4 Clear{ClearHdr(InClear.X, Settings.Exposure), ClearHdr(InClear.Y, Settings.Exposure),
	                  ClearHdr(InClear.Z, Settings.Exposure), InClear.W};
	if (InMain.Viewport)
	{
		ClearTargets(InGraph, Clear);
	}
	const bool bDeferred = Settings.Pipeline == ESceneRenderPipeline::Deferred;
	LastStatistics.LocalLights.bActive = (bDeferred || Settings.bClusteredLighting) && !InShadows.DebugMode;
	LastStatistics.LocalLights.bClustered = Settings.bClusteredLighting;
	if (const auto& Metadata = InFrame->GetSceneMetadata())
	{
		LastStatistics.LocalLights.Points = Metadata->PointLights.size();
		LastStatistics.LocalLights.Spots = Metadata->SpotLights.size();
	}
	PrepareClusters(InMain, *InFrame, Settings.bClusteredLighting && !InShadows.DebugMode);
	auto Family = MakeViews(InMain, Clear);
	Session.BuildViews(InGraph, Family.Views, Family.Targets, InFrame, 1, true, bInDeferPreparation,
	                   [&](std::size_t InIndex)
	                   {
		                   if (bDeferred && InIndex == Family.BaseIndex)
		                   {
			                   AddFullscreenPass(Session, InGraph, Lighting(InMain, *InFrame, Clear),
			                                     bInDeferPreparation);
			                   if (!InShadows.DebugMode && !Settings.bClusteredLighting)
			                   {
				                   AddLocalLights(InGraph, InMain, *InFrame, bInDeferPreparation);
			                   }
		                   }
		                   if (InIndex + 1 == Family.TransparentIndex)
		                   {
			                   AddSky(InGraph, InMain, *InFrame, bInDeferPreparation);
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
