#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Hyperion
{
enum class EMaterialFill : std::uint8_t
{
	Solid,
	Wireframe
};
enum class EMaterialCull : std::uint8_t
{
	None,
	Front,
	Back
};
enum class EMaterialCompare : std::uint8_t
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
enum class EMaterialStencilOp : std::uint8_t
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
enum class EMaterialBlendFactor : std::uint8_t
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
enum class EMaterialBlendOp : std::uint8_t
{
	Add,
	Subtract,
	ReverseSubtract,
	Minimum,
	Maximum
};
enum class EMaterialQueue : std::uint8_t
{
	Opaque,
	Masked,
	Transparent,
	Overlay
};

struct FMaterialStencilFace
{
	EMaterialCompare Compare = EMaterialCompare::Always;
	EMaterialStencilOp Fail = EMaterialStencilOp::Keep;
	EMaterialStencilOp DepthFail = EMaterialStencilOp::Keep;
	EMaterialStencilOp Pass = EMaterialStencilOp::Keep;
	bool operator==(const FMaterialStencilFace&) const = default;
};

struct FMaterialState
{
	EMaterialFill Fill = EMaterialFill::Solid;
	EMaterialCull Cull = EMaterialCull::None;
	bool bFrontCounterClockwise = true;
	std::int32_t DepthBias{};
	float DepthBiasClamp{};
	float SlopeScaledDepthBias{};
	bool bDepthClip = true;
	bool bDepthTest{};
	bool bDepthWrite{};
	EMaterialCompare DepthCompare = EMaterialCompare::LessEqual;
	bool bStencil{};
	FMaterialStencilFace FrontStencil;
	FMaterialStencilFace BackStencil;
	std::uint8_t StencilReadMask = 255;
	std::uint8_t StencilWriteMask = 255;
	bool bBlend{};
	EMaterialBlendFactor SourceRgb = EMaterialBlendFactor::One;
	EMaterialBlendFactor DestinationRgb = EMaterialBlendFactor::Zero;
	EMaterialBlendOp RgbOperation = EMaterialBlendOp::Add;
	EMaterialBlendFactor SourceAlpha = EMaterialBlendFactor::One;
	EMaterialBlendFactor DestinationAlpha = EMaterialBlendFactor::Zero;
	EMaterialBlendOp AlphaOperation = EMaterialBlendOp::Add;
	std::uint8_t ColorWriteMask = 15;
	bool bAlphaToCoverage{};
	std::uint32_t SampleMask = 0xffffffffU;
	bool operator==(const FMaterialState&) const = default;
};

struct FMaterialDynamicState
{
	std::uint32_t StencilReference{};
	std::array<float, 4> BlendConstants{1, 1, 1, 1};
	bool operator==(const FMaterialDynamicState&) const = default;
};

struct FMaterialShaderDefine
{
	std::string Name;
	std::string Value;
	bool operator==(const FMaterialShaderDefine&) const = default;
};

struct FMaterialShader
{
	std::string Path;
	std::string Entry;
	std::vector<FMaterialShaderDefine> Defines;
	bool operator==(const FMaterialShader&) const = default;
};

struct FMaterialPass
{
	std::string Usage = "Forward";
	FMaterialShader Vertex;
	FMaterialShader Pixel;
	FMaterialState State;
	FMaterialDynamicState DynamicState;
	EMaterialQueue Queue = EMaterialQueue::Opaque;
	bool bSrgbTarget{};
	bool bAlphaClip{};
	bool bRequiresConservativeBounds{};
	bool bAllowDynamicOverrides{};
};

enum class EMaterialAvailability : std::uint8_t
{
	Available,
	Unsupported,
	Incompatible
};

struct FMaterialPassContext
{
	std::uint32_t SampleCount = 1;
	std::uint32_t ColorTargetCount = 1;
	bool bHasDepth{};
	bool bHasStencil{};
	bool bUsageScheduled = true;
};

struct FMaterialPassResult
{
	EMaterialAvailability Availability = EMaterialAvailability::Available;
	std::string Reason;
	FMaterialState State;

	bool IsAvailable() const
	{
		return Availability == EMaterialAvailability::Available;
	}
};

FMaterialState NormalizeMaterialState(FMaterialState InState);
FMaterialPassResult ResolveMaterialPass(const FMaterialPass& InPass, const FMaterialPassContext& InContext);
} // namespace Hyperion
