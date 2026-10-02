#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace Hyperion
{
enum class ESceneRenderPipeline : std::uint8_t
{
	Deferred = 0,
	Forward = 1,
	Count
};

enum class EGBufferPreset : std::uint8_t
{
	Compact = 0,
	HighPrecision = 1,
	Count
};

enum class EGBufferVisualizer : std::uint32_t
{
	Lit = 0,
	BaseColor = 1,
	ShadingNormal = 2,
	MaterialChannels = 3,
	Emissive = 4,
	SceneDepth = 5,
	GeometryNormal = 6,
	Count
};

inline constexpr ESceneRenderPipeline DefaultSceneRenderPipeline = ESceneRenderPipeline::Deferred;
inline constexpr EGBufferPreset DefaultGBufferPreset = EGBufferPreset::Compact;
inline constexpr EGBufferVisualizer DefaultGBufferVisualizer = EGBufferVisualizer::Lit;

struct FSceneRenderPipelineOption
{
	ESceneRenderPipeline Id;
	std::string_view Token;
	std::string_view Label;
};

struct FGBufferPresetOption
{
	EGBufferPreset Id;
	std::string_view Token;
	std::string_view Label;
};

struct FGBufferVisualizerOption
{
	EGBufferVisualizer Id;
	std::string_view Label;
	std::string_view ShaderDefine;
};

std::span<const FSceneRenderPipelineOption> SceneRenderPipelineOptions();
std::span<const FGBufferPresetOption> GBufferPresetOptions();
std::span<const FGBufferVisualizerOption> GBufferVisualizerOptions();
const FSceneRenderPipelineOption& DescribeSceneRenderPipeline(ESceneRenderPipeline InId);
const FGBufferPresetOption& DescribeGBufferPreset(EGBufferPreset InId);
const FGBufferVisualizerOption& DescribeGBufferVisualizer(EGBufferVisualizer InId);
ESceneRenderPipeline ParseSceneRenderPipeline(std::string_view InToken);
EGBufferPreset ParseGBufferPreset(std::string_view InToken);
std::string_view ToSceneRenderPipelineToken(ESceneRenderPipeline InId);
std::string_view ToGBufferPresetToken(EGBufferPreset InId);
EGBufferVisualizer ParseGBufferVisualizer(std::uint32_t InValue);
EGBufferVisualizer ParseAppGBufferVisualizer(std::int64_t InValue);
std::uint32_t ToVisualizerWireValue(EGBufferVisualizer InId);
std::uint32_t ToVisualizerShaderCode(EGBufferVisualizer InId);
std::uint32_t GBufferVisualizerMinimum();
std::uint32_t GBufferVisualizerMaximum();

// Presentation may reorder or relabel entries, but cannot replace a stable identity or its protocol spelling.
void ValidateRasterOptionPresentation(std::span<const FSceneRenderPipelineOption> InOptions);
void ValidateRasterOptionPresentation(std::span<const FGBufferPresetOption> InOptions);
void ValidateRasterOptionPresentation(std::span<const FGBufferVisualizerOption> InOptions);

template<class TOption, class TIdentity>
std::size_t RasterOptionIndex(std::span<const TOption> InOptions, TIdentity InId)
{
	for (std::size_t Index = 0; Index < InOptions.size(); ++Index)
	{
		if (InOptions[Index].Id == InId)
		{
			return Index;
		}
	}
	throw std::invalid_argument("Raster option identity is absent from the presentation");
}

template<class TOption> auto RasterOptionIdentity(std::span<const TOption> InOptions, std::size_t InIndex)
{
	if (InIndex >= InOptions.size())
	{
		throw std::invalid_argument("Raster option presentation index is out of range");
	}
	return InOptions[InIndex].Id;
}

template<class TOption> std::vector<std::string> RasterOptionLabels(std::span<const TOption> InOptions)
{
	std::vector<std::string> Result;
	Result.reserve(InOptions.size());
	for (const auto& Option : InOptions)
	{
		Result.emplace_back(Option.Label);
	}
	return Result;
}
} // namespace Hyperion
