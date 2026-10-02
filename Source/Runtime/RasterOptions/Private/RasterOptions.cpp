#include "Hyperion/RasterOptions/RasterOptions.h"
#include <algorithm>
#include <array>
#include <limits>
#include <string>

namespace Hyperion
{
namespace
{
constexpr std::array PipelineOptions{FSceneRenderPipelineOption{ESceneRenderPipeline::Deferred, "deferred", "Deferred"},
                                     FSceneRenderPipelineOption{ESceneRenderPipeline::Forward, "forward", "Forward"}};
constexpr std::array PresetOptions{FGBufferPresetOption{EGBufferPreset::Compact, "compact", "Compact"},
                                   FGBufferPresetOption{EGBufferPreset::HighPrecision, "high", "High precision"}};
constexpr std::array VisualizerOptions{
    FGBufferVisualizerOption{EGBufferVisualizer::Lit, "Lit", "HYP_GBUFFER_VIEW_LIT"},
    FGBufferVisualizerOption{EGBufferVisualizer::BaseColor, "Base color", "HYP_GBUFFER_VIEW_BASE_COLOR"},
    FGBufferVisualizerOption{EGBufferVisualizer::ShadingNormal, "Shading normal", "HYP_GBUFFER_VIEW_SHADING_NORMAL"},
    FGBufferVisualizerOption{EGBufferVisualizer::MaterialChannels, "Metallic / Roughness / AO",
                             "HYP_GBUFFER_VIEW_MATERIAL_CHANNELS"},
    FGBufferVisualizerOption{EGBufferVisualizer::Emissive, "Emissive", "HYP_GBUFFER_VIEW_EMISSIVE"},
    FGBufferVisualizerOption{EGBufferVisualizer::SceneDepth, "Scene depth", "HYP_GBUFFER_VIEW_SCENE_DEPTH"},
    FGBufferVisualizerOption{EGBufferVisualizer::GeometryNormal, "Geometry normal",
                             "HYP_GBUFFER_VIEW_GEOMETRY_NORMAL"}};

template<class TOption> constexpr std::string_view ProtocolName(const TOption& InOption)
{
	if constexpr (requires { InOption.Token; })
	{
		return InOption.Token;
	}
	else
	{
		return InOption.ShaderDefine;
	}
}

template<class TOption, std::size_t Size> consteval bool IsComplete(const std::array<TOption, Size>& InOptions)
{
	using FIdentity = decltype(TOption::Id);
	if (Size != static_cast<std::size_t>(FIdentity::Count))
	{
		return false;
	}
	for (std::size_t Index = 0; Index < Size; ++Index)
	{
		const auto& Option = InOptions[Index];
		if (static_cast<std::size_t>(Option.Id) >= Size || ProtocolName(Option).empty() || Option.Label.empty())
		{
			return false;
		}
		for (std::size_t Earlier = 0; Earlier < Index; ++Earlier)
		{
			if (InOptions[Earlier].Id == Option.Id || ProtocolName(InOptions[Earlier]) == ProtocolName(Option))
			{
				return false;
			}
		}
	}
	return true;
}

static_assert(IsComplete(PipelineOptions));
static_assert(IsComplete(PresetOptions));
static_assert(IsComplete(VisualizerOptions));

template<class TOption, class TIdentity> const TOption& Describe(std::span<const TOption> InOptions, TIdentity InId)
{
	return InOptions[RasterOptionIndex(InOptions, InId)];
}

template<class TOption> auto ParseToken(std::span<const TOption> InOptions, std::string_view InToken)
{
	for (const auto& Option : InOptions)
	{
		if (Option.Token == InToken)
		{
			return Option.Id;
		}
	}
	throw std::invalid_argument("Unknown raster option token: " + std::string(InToken));
}

template<class TOption>
void ValidatePresentation(std::span<const TOption> InOptions, std::span<const TOption> InCanonical)
{
	if (InOptions.size() != InCanonical.size())
	{
		throw std::invalid_argument("Raster option presentation must contain every supported identity");
	}
	for (std::size_t Index = 0; Index < InOptions.size(); ++Index)
	{
		const auto& Option = InOptions[Index];
		const auto& Canonical = Describe(InCanonical, Option.Id);
		if (ProtocolName(Option) != ProtocolName(Canonical) || Option.Label.empty() ||
		    RasterOptionIndex(InOptions, Option.Id) != Index)
		{
			throw std::invalid_argument("Raster option presentation contains an ambiguous identity or protocol name");
		}
	}
}
} // namespace

std::span<const FSceneRenderPipelineOption> SceneRenderPipelineOptions()
{
	return PipelineOptions;
}

std::span<const FGBufferPresetOption> GBufferPresetOptions()
{
	return PresetOptions;
}

std::span<const FGBufferVisualizerOption> GBufferVisualizerOptions()
{
	return VisualizerOptions;
}

const FSceneRenderPipelineOption& DescribeSceneRenderPipeline(ESceneRenderPipeline InId)
{
	return Describe(SceneRenderPipelineOptions(), InId);
}

const FGBufferPresetOption& DescribeGBufferPreset(EGBufferPreset InId)
{
	return Describe(GBufferPresetOptions(), InId);
}

const FGBufferVisualizerOption& DescribeGBufferVisualizer(EGBufferVisualizer InId)
{
	return Describe(GBufferVisualizerOptions(), InId);
}

ESceneRenderPipeline ParseSceneRenderPipeline(std::string_view InToken)
{
	return ParseToken(SceneRenderPipelineOptions(), InToken);
}

EGBufferPreset ParseGBufferPreset(std::string_view InToken)
{
	return ParseToken(GBufferPresetOptions(), InToken);
}

std::string_view ToSceneRenderPipelineToken(ESceneRenderPipeline InId)
{
	return DescribeSceneRenderPipeline(InId).Token;
}

std::string_view ToGBufferPresetToken(EGBufferPreset InId)
{
	return DescribeGBufferPreset(InId).Token;
}

EGBufferVisualizer ParseGBufferVisualizer(std::uint32_t InValue)
{
	return DescribeGBufferVisualizer(static_cast<EGBufferVisualizer>(InValue)).Id;
}

EGBufferVisualizer ParseAppGBufferVisualizer(std::int64_t InValue)
{
	if (InValue < 0 || InValue > std::numeric_limits<std::uint32_t>::max())
	{
		throw std::invalid_argument("Application GBuffer visualizer value is out of range");
	}
	return ParseGBufferVisualizer(static_cast<std::uint32_t>(InValue));
}

std::uint32_t ToVisualizerWireValue(EGBufferVisualizer InId)
{
	return static_cast<std::uint32_t>(DescribeGBufferVisualizer(InId).Id);
}

std::uint32_t ToVisualizerShaderCode(EGBufferVisualizer InId)
{
	return static_cast<std::uint32_t>(DescribeGBufferVisualizer(InId).Id);
}

std::uint32_t GBufferVisualizerMinimum()
{
	return ToVisualizerWireValue(std::min_element(VisualizerOptions.begin(), VisualizerOptions.end(),
	                                              [](const auto& InLeft, const auto& InRight)
	                                              {
		                                              return InLeft.Id < InRight.Id;
	                                              })
	                                 ->Id);
}

std::uint32_t GBufferVisualizerMaximum()
{
	return ToVisualizerWireValue(std::max_element(VisualizerOptions.begin(), VisualizerOptions.end(),
	                                              [](const auto& InLeft, const auto& InRight)
	                                              {
		                                              return InLeft.Id < InRight.Id;
	                                              })
	                                 ->Id);
}

void ValidateRasterOptionPresentation(std::span<const FSceneRenderPipelineOption> InOptions)
{
	ValidatePresentation(InOptions, SceneRenderPipelineOptions());
}

void ValidateRasterOptionPresentation(std::span<const FGBufferPresetOption> InOptions)
{
	ValidatePresentation(InOptions, GBufferPresetOptions());
}

void ValidateRasterOptionPresentation(std::span<const FGBufferVisualizerOption> InOptions)
{
	ValidatePresentation(InOptions, GBufferVisualizerOptions());
}
} // namespace Hyperion
