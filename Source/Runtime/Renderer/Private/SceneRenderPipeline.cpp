#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include "Hyperion/Core/Profiling.h"
#include "PipelineShadows.h"
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
		FeatureResources = {};
		for (const auto& Feature : Features)
		{
			Feature->Reset();
		}
		bPending = false;
		FullscreenStatistics.reset();
		auto Disabled = InShadows;
		Disabled.bEnabled = false;
		ShadowMaps.Prepare(Resolved.View, {0, 0, 1}, Disabled, {});
		if (OutputTarget.Kind == ERenderTargetKind::Backbuffer)
		{
			Session.BuildSceneClear(InGraph, Resolved, InClear);
		}
		else
		{
			FRenderSceneSnapshot ClearSnapshot;
			ClearSnapshot.Targets = OutputTargets(InClear);
			ClearSnapshot.Targets.Name = "Scene without active camera";
			InGraph.Add(Session.GetResources().GetPreparation().DeclarePass(InGraph, ClearSnapshot));
		}
		if (InExtensions)
		{
			InExtensions(InGraph);
		}
	}
	LastStatistics.SceneToken = InSeed->GetToken();
	LastStatistics.CameraStatus = Resolved.CameraStatus;
	LastStatistics.MainCameraView = Resolved.HasCamera() ? std::optional(Resolved.View) : std::nullopt;
}

void FSceneRenderPipeline::PrepareShadows(const FRenderView& InMain, const FMaterialFrameContext& InFrame,
                                          const FCascadedShadowSettings& InShadows)
{
	const auto Start = std::chrono::steady_clock::now();
	LastStatistics.bShadows =
	    PreparePipelineShadows(Session, ShadowMaps, InMain, InFrame, InShadows, Direction(Session, InFrame));
	LastStatistics.ShadowSetupMilliseconds =
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	UpdatePipelineShadowLifetime(Session.GetResources(), ShadowMaps, InMain.DepthConvention, ShadowLifetime,
	                             ShadowBytes, ShadowDepthConvention);
}

void FSceneRenderPipeline::ValidateView(const FRenderView& InView, const FCascadedShadowSettings& InShadows) const
{
	if (const auto* Output = OutputTarget.Texture ? OutputTarget.Texture->GetColorTarget() : nullptr)
	{
		if (Output->Width != InView.Width || Output->Height != InView.Height || InShadows.DebugMode ||
		    Settings.ContactShadows.DebugMode)
		{
			throw std::invalid_argument("Offscreen scene output requires matching dimensions and no depth overlays");
		}
	}
	if (Settings.Pipeline == ESceneRenderPipeline::Deferred && InView.Viewport)
	{
		const auto& View = *InView.Viewport;
		const float Range = View.MaxDepth - View.MinDepth;
		if (!(Range > 0) || !std::isfinite(1.f / Range))
		{
			throw std::invalid_argument("Deferred reconstruction requires a positive invertible viewport depth range");
		}
	}
}

void FSceneRenderPipeline::SetTransientGeometry(std::shared_ptr<const FTransientGeometry> InGeometry)
{
	TransientGeometry = std::move(InGeometry);
}

FRenderFeatureContext FSceneRenderPipeline::BeginFeatures(FRenderGraph& InGraph, const FRenderView& InView,
                                                          std::shared_ptr<const FMaterialFrameContext> InFrame,
                                                          bool bInDeferPreparation)
{
	FeatureResources = {};
	FeatureResources.Depth = {ERenderTargetKind::Texture, SceneDepth, Lifetime, false};
	FeatureResources.Color = {ERenderTargetKind::Texture, SceneColor, Lifetime, false};
	FeatureResources.Output = OutputTarget;
	for (std::size_t Index = 0; Index < GBuffer.size(); ++Index)
	{
		FeatureResources.GBuffer[Index] = {ERenderTargetKind::Texture, GBuffer[Index], Lifetime, false};
	}
	FRenderFeatureContext FeatureContext{Session,
	                                     InGraph,
	                                     InView,
	                                     *InFrame,
	                                     Settings,
	                                     FeatureResources,
	                                     LastStatistics,
	                                     FullscreenStatistics,
	                                     bInDeferPreparation,
	                                     InFrame,
	                                     TransientGeometry};
	for (const auto& Feature : Features)
	{
		Feature->BeginFrame(FeatureContext);
	}
	return FeatureContext;
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
	ValidateView(InMain, InShadows);
	Resize(InMain.Width, InMain.Height, InMain.DepthConvention);
	LastStatistics = {};
	FullscreenStatistics = std::make_shared<FFullscreenPreparationStatistics>();
	auto FeatureContext = BeginFeatures(InGraph, InMain, InFrame, bInDeferPreparation);
	LastStatistics.Spatial = Session.GetScene().BeginViews();
	PrepareShadows(InMain, *InFrame, InShadows);
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
		                   if (InIndex == Family.BaseIndex)
		                   {
			                   BuildFeatures(ERenderFeatureStage::AfterOpaque, FeatureContext);
		                   }
		                   if (bDeferred && InIndex == Family.BaseIndex)
		                   {
			                   BuildFeatures(ERenderFeatureStage::BeforeLighting, FeatureContext);
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
			                   BuildFeatures(ERenderFeatureStage::BeforeTonemap, FeatureContext);
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
	BuildFeatures(ERenderFeatureStage::AfterTonemap, FeatureContext);
	for (const auto& Feature : Features)
	{
		Feature->EndFrame(FeatureContext);
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

void FSceneRenderPipeline::BuildFeatures(ERenderFeatureStage InStage, FRenderFeatureContext& InContext)
{
	for (const auto& Feature : Features)
	{
		Feature->Build(InStage, InContext);
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
