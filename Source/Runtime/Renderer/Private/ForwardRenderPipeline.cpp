#include "Hyperion/Renderer/ForwardRenderPipeline.h"
#include "Hyperion/Core/Profiling.h"
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
                                   const std::function<void(FRenderGraph&)>& InExtensions)
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
	LastStatistics.bShadows = ShadowMaps.Prepare(InMain, Light, InShadows,
	                                             [this](const ISceneVisibility& InVolume)
	                                             {
		                                             return Session.GetScene().QueryBounds(InVolume);
	                                             });
	LastStatistics.ShadowSetupMilliseconds =
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	if (AllocatedShadowBytes != ShadowMaps.TextureBytes())
	{
		// Replacing resolution retires the previous attachment set after its submitted users complete.
		Lifetime = Session.GetResources().CreateScopeLifetime();
		AllocatedShadowBytes = ShadowMaps.TextureBytes();
	}
	auto Views = ShadowMaps.Views(InMain, Lifetime);
	ShadowMaps.Bind(InMain, Lifetime);
	InMain.ClearColor = InClear;
	InMain.Name = "Forward";
	Views.push_back(std::move(InMain));
	Session.BuildViews(InGraph, Views, std::move(InFrame), 1, true);
	if (LastStatistics.bShadows && InShadows.DebugMode >= 2 && InShadows.DebugMode <= 5)
	{
		const auto& Main = Views.back();
		const float Size = std::min({320.f, float(Main.Width), float(Main.Height)});
		Session.AppendDepthPreview(InGraph, Main.SampledDepth.at(InShadows.DebugMode - 2), Lifetime,
		                           InShadows.PreviewViewport.value_or(
		                               FViewport{float(Main.Width) - Size, float(Main.Height) - Size, Size, Size}));
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

const FForwardPipelineStatistics& FForwardRenderPipeline::Statistics() const
{
	return LastStatistics;
}

const FCascadedShadowMap& FForwardRenderPipeline::Shadows() const
{
	return ShadowMaps;
}
} // namespace Hyperion
