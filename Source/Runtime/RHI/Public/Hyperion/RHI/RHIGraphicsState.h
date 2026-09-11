#pragma once
#include <array>
#include <cstdint>

namespace Hyperion
{
enum class ERHIFill : std::uint8_t
{
	Solid,
	Wireframe
};
enum class ERHICull : std::uint8_t
{
	None,
	Front,
	Back
};
enum class ERHICompare : std::uint8_t
{
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always
};
enum class ERHIStencilOp : std::uint8_t
{
	Keep,
	Zero,
	Replace,
	IncrementClamp,
	DecrementClamp,
	Invert,
	IncrementWrap,
	DecrementWrap
};
enum class ERHIBlendFactor : std::uint8_t
{
	Zero,
	One,
	SourceColor,
	InverseSourceColor,
	SourceAlpha,
	InverseSourceAlpha,
	DestinationAlpha,
	InverseDestinationAlpha,
	DestinationColor,
	InverseDestinationColor,
	SourceAlphaSaturate,
	Constant,
	InverseConstant
};
enum class ERHIBlendOp : std::uint8_t
{
	Add,
	Subtract,
	ReverseSubtract,
	Minimum,
	Maximum
};
enum class ERHIPrimitiveTopology : std::uint8_t
{
	TriangleList,
	LineList,
	PointList
};
enum class ERHIDepthFormat : std::uint8_t
{
	None,
	D32,
	D32S8
};

enum class ERHIColorFormat : std::uint8_t
{
	Rgba8Unorm,
	Rgba8Srgb,
	Rgba16Float,
	Rgba32Float,
	Count
};

inline constexpr std::uint32_t MaximumColorTargets = 8;

struct FStencilFaceDesc
{
	ERHICompare Compare = ERHICompare::Always;
	ERHIStencilOp Fail = ERHIStencilOp::Keep;
	ERHIStencilOp DepthFail = ERHIStencilOp::Keep;
	ERHIStencilOp Pass = ERHIStencilOp::Keep;
	bool operator==(const FStencilFaceDesc&) const = default;
};

struct FGraphicsState
{
	ERHIFill Fill = ERHIFill::Solid;
	ERHICull Cull = ERHICull::None;
	bool bFrontCounterClockwise = true;
	std::int32_t DepthBias{};
	float DepthBiasClamp{};
	float SlopeScaledDepthBias{};
	bool bDepthClip = true;
	bool bDepthTest{};
	bool bDepthWrite{};
	ERHICompare DepthCompare = ERHICompare::LessEqual;
	bool bStencil{};
	FStencilFaceDesc FrontStencil;
	FStencilFaceDesc BackStencil;
	std::uint8_t StencilReadMask = 255;
	std::uint8_t StencilWriteMask = 255;
	bool bBlend{};
	ERHIBlendFactor SourceRgb = ERHIBlendFactor::One;
	ERHIBlendFactor DestinationRgb = ERHIBlendFactor::Zero;
	ERHIBlendOp RgbOperation = ERHIBlendOp::Add;
	ERHIBlendFactor SourceAlpha = ERHIBlendFactor::One;
	ERHIBlendFactor DestinationAlpha = ERHIBlendFactor::Zero;
	ERHIBlendOp AlphaOperation = ERHIBlendOp::Add;
	std::uint8_t ColorWriteMask = 15;
	bool bAlphaToCoverage{};
	std::uint32_t SampleMask = 0xffffffffU;
	bool operator==(const FGraphicsState&) const = default;
};

struct FGraphicsTarget
{
	bool bSrgb{};
	ERHIDepthFormat Depth = ERHIDepthFormat::None;
	std::uint32_t ColorCount = 1;
	std::uint32_t SampleCount = 1;
	std::array<ERHIColorFormat, MaximumColorTargets> ColorFormats{};

	ERHIColorFormat GetColorFormat(std::uint32_t InSlot) const
	{
		return InSlot == 0 && bSrgb ? ERHIColorFormat::Rgba8Srgb : ColorFormats.at(InSlot);
	}

	bool operator==(const FGraphicsTarget&) const = default;
};

struct FGraphicsDynamicState
{
	std::uint32_t StencilReference{};
	std::array<float, 4> BlendConstants{1, 1, 1, 1};
	bool operator==(const FGraphicsDynamicState&) const = default;
};

struct FViewport
{
	float X{};
	float Y{};
	float Width{};
	float Height{};
	float MinDepth{};
	float MaxDepth = 1;
	bool operator==(const FViewport&) const = default;
};
} // namespace Hyperion
