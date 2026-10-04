#include "Hyperion/RasterOptions/RasterOptions.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <vector>

void CheckShadowPreviews();

namespace
{
using namespace Hyperion;

template<class TAction> void Rejects(TAction InAction)
{
	bool bRejected{};
	try
	{
		InAction();
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

void FixedProtocol()
{
	static_assert(static_cast<std::uint8_t>(ESceneRenderPipeline::Deferred) == 0);
	static_assert(static_cast<std::uint8_t>(ESceneRenderPipeline::Forward) == 1);
	HYP_CHECK(ParseSceneRenderPipeline("deferred") == ESceneRenderPipeline::Deferred);
	HYP_CHECK(ParseSceneRenderPipeline("forward") == ESceneRenderPipeline::Forward);
	HYP_CHECK(ParseGBufferPreset("compact") == EGBufferPreset::Compact);
	HYP_CHECK(ParseGBufferPreset("high") == EGBufferPreset::HighPrecision);
	HYP_CHECK(ToSceneRenderPipelineToken(DefaultSceneRenderPipeline) == "deferred");
	HYP_CHECK(ToGBufferPresetToken(DefaultGBufferPreset) == "compact");
	HYP_CHECK(ToVisualizerWireValue(DefaultGBufferVisualizer) == 0);
	const std::array Modes{EGBufferVisualizer::Lit,           EGBufferVisualizer::BaseColor,
	                       EGBufferVisualizer::ShadingNormal, EGBufferVisualizer::MaterialChannels,
	                       EGBufferVisualizer::Emissive,      EGBufferVisualizer::SceneDepth,
	                       EGBufferVisualizer::GeometryNormal};
	const std::array<std::string_view, 7> Labels{
	    "Lit",      "Base color",  "Shading normal", "Metallic / Roughness / AO",
	    "Emissive", "Scene depth", "Geometry normal"};
	const std::array<std::string_view, 7> Defines{"HYP_GBUFFER_VIEW_LIT",
	                                              "HYP_GBUFFER_VIEW_BASE_COLOR",
	                                              "HYP_GBUFFER_VIEW_SHADING_NORMAL",
	                                              "HYP_GBUFFER_VIEW_MATERIAL_CHANNELS",
	                                              "HYP_GBUFFER_VIEW_EMISSIVE",
	                                              "HYP_GBUFFER_VIEW_SCENE_DEPTH",
	                                              "HYP_GBUFFER_VIEW_GEOMETRY_NORMAL"};
	HYP_CHECK(GBufferVisualizerMinimum() == 0 && GBufferVisualizerMaximum() == 6);
	for (std::uint32_t Raw = 0; Raw < Modes.size(); ++Raw)
	{
		HYP_CHECK(ParseGBufferVisualizer(Raw) == Modes[Raw] && ParseAppGBufferVisualizer(Raw) == Modes[Raw]);
		HYP_CHECK(ToVisualizerWireValue(Modes[Raw]) == Raw && ToVisualizerShaderCode(Modes[Raw]) == Raw);
		HYP_CHECK(DescribeGBufferVisualizer(Modes[Raw]).Label == Labels[Raw]);
		HYP_CHECK(DescribeGBufferVisualizer(Modes[Raw]).ShaderDefine == Defines[Raw]);
	}
}

template<class TOption> void Presentation(std::span<const TOption> InCanonical)
{
	std::vector<TOption> Options(InCanonical.begin(), InCanonical.end());
	std::reverse(Options.begin(), Options.end());
	const std::span<const TOption> Reordered(Options);
	ValidateRasterOptionPresentation(Reordered);
	for (const auto& Option : InCanonical)
	{
		const auto Index = RasterOptionIndex(Reordered, Option.Id);
		HYP_CHECK(RasterOptionIdentity(Reordered, Index) == Option.Id);
		HYP_CHECK(Reordered[Index].Label == Option.Label);
	}
	Rejects(
	    [&]
	    {
		    (void)RasterOptionIdentity(Reordered, Reordered.size());
	    });
	Options.front().Label = "Relabeled";
	ValidateRasterOptionPresentation(Reordered);
	Options.front() = Options.back();
	Rejects(
	    [&]
	    {
		    ValidateRasterOptionPresentation(Reordered);
	    });
	Options.assign(InCanonical.begin(), InCanonical.end());
	Options.pop_back();
	Rejects(
	    [&]
	    {
		    ValidateRasterOptionPresentation(std::span<const TOption>(Options));
	    });
}

void InvalidProtocol()
{
	for (const auto* Token : {"", "unknown", "Forward", " deferred"})
	{
		Rejects(
		    [&]
		    {
			    (void)ParseSceneRenderPipeline(Token);
		    });
		Rejects(
		    [&]
		    {
			    (void)ParseGBufferPreset(Token);
		    });
	}
	Rejects(
	    []
	    {
		    (void)ToSceneRenderPipelineToken(static_cast<ESceneRenderPipeline>(255));
	    });
	Rejects(
	    []
	    {
		    (void)ToGBufferPresetToken(static_cast<EGBufferPreset>(255));
	    });
	for (const auto Value : {7u, std::numeric_limits<std::uint32_t>::max()})
	{
		Rejects(
		    [&]
		    {
			    (void)ParseGBufferVisualizer(Value);
		    });
		Rejects(
		    [&]
		    {
			    (void)ToVisualizerWireValue(static_cast<EGBufferVisualizer>(Value));
		    });
		Rejects(
		    [&]
		    {
			    (void)ToVisualizerShaderCode(static_cast<EGBufferVisualizer>(Value));
		    });
	}
	for (const auto Value :
	     {std::int64_t(-1), std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()})
	{
		Rejects(
		    [&]
		    {
			    (void)ParseAppGBufferVisualizer(Value);
		    });
	}
	std::vector<FSceneRenderPipelineOption> Ambiguous(SceneRenderPipelineOptions().begin(),
	                                                  SceneRenderPipelineOptions().end());
	Ambiguous.back().Token = Ambiguous.front().Token;
	Rejects(
	    [&]
	    {
		    ValidateRasterOptionPresentation(std::span<const FSceneRenderPipelineOption>(Ambiguous));
	    });
	std::vector<FGBufferVisualizerOption> Defines(GBufferVisualizerOptions().begin(), GBufferVisualizerOptions().end());
	Defines.back().ShaderDefine = Defines.front().ShaderDefine;
	Rejects(
	    [&]
	    {
		    ValidateRasterOptionPresentation(std::span<const FGBufferVisualizerOption>(Defines));
	    });
}
} // namespace

int main()
{
	try
	{
		FixedProtocol();
		CheckShadowPreviews();
		Presentation(SceneRenderPipelineOptions());
		Presentation(GBufferPresetOptions());
		Presentation(GBufferVisualizerOptions());
		InvalidProtocol();
		std::cout << "Stable raster option identities, protocol and presentation passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
