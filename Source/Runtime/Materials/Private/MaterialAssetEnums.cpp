#include "Hyperion/Materials/MaterialAsset.h"

namespace Hyperion
{
template<> std::span<const EMaterialValueKind> RecordEnumValues<EMaterialValueKind>()
{
	static constexpr std::array Values{EMaterialValueKind::Numeric,    EMaterialValueKind::Structure,
	                                   EMaterialValueKind::Array,      EMaterialValueKind::Texture2D,
	                                   EMaterialValueKind::ReadBuffer, EMaterialValueKind::Sampler};
	return Values;
}

template<> std::span<const EMaterialScalar> RecordEnumValues<EMaterialScalar>()
{
	static constexpr std::array Values{EMaterialScalar::Bool, EMaterialScalar::Int, EMaterialScalar::Uint,
	                                   EMaterialScalar::Float};
	return Values;
}

template<> std::span<const EMaterialParameterSource> RecordEnumValues<EMaterialParameterSource>()
{
	static constexpr std::array Values{EMaterialParameterSource::Manual, EMaterialParameterSource::Semantic};
	return Values;
}

template<> std::span<const EMaterialOverridePolicy> RecordEnumValues<EMaterialOverridePolicy>()
{
	static constexpr std::array Values{EMaterialOverridePolicy::Locked, EMaterialOverridePolicy::AllowOverride};
	return Values;
}

template<> std::span<const EMaterialAddressMode> RecordEnumValues<EMaterialAddressMode>()
{
	static constexpr std::array Values{EMaterialAddressMode::Repeat, EMaterialAddressMode::Clamp,
	                                   EMaterialAddressMode::Mirror, EMaterialAddressMode::Border,
	                                   EMaterialAddressMode::MirrorOnce};
	return Values;
}

template<> std::span<const EMaterialSamplerCompare> RecordEnumValues<EMaterialSamplerCompare>()
{
	static constexpr std::array Values{EMaterialSamplerCompare::Never,        EMaterialSamplerCompare::Less,
	                                   EMaterialSamplerCompare::Equal,        EMaterialSamplerCompare::LessEqual,
	                                   EMaterialSamplerCompare::Greater,      EMaterialSamplerCompare::NotEqual,
	                                   EMaterialSamplerCompare::GreaterEqual, EMaterialSamplerCompare::Always};
	return Values;
}

template<> std::span<const EMaterialFill> RecordEnumValues<EMaterialFill>()
{
	static constexpr std::array Values{EMaterialFill::Solid, EMaterialFill::Wireframe};
	return Values;
}

template<> std::span<const EMaterialCull> RecordEnumValues<EMaterialCull>()
{
	static constexpr std::array Values{EMaterialCull::None, EMaterialCull::Front, EMaterialCull::Back};
	return Values;
}

template<> std::span<const EMaterialCompare> RecordEnumValues<EMaterialCompare>()
{
	static constexpr std::array Values{EMaterialCompare::Never,        EMaterialCompare::Less,
	                                   EMaterialCompare::Equal,        EMaterialCompare::LessEqual,
	                                   EMaterialCompare::Greater,      EMaterialCompare::NotEqual,
	                                   EMaterialCompare::GreaterEqual, EMaterialCompare::Always};
	return Values;
}

template<> std::span<const EMaterialStencilOp> RecordEnumValues<EMaterialStencilOp>()
{
	static constexpr std::array Values{EMaterialStencilOp::Keep,           EMaterialStencilOp::Zero,
	                                   EMaterialStencilOp::Replace,        EMaterialStencilOp::IncrementClamp,
	                                   EMaterialStencilOp::DecrementClamp, EMaterialStencilOp::Invert,
	                                   EMaterialStencilOp::IncrementWrap,  EMaterialStencilOp::DecrementWrap};
	return Values;
}

template<> std::span<const EMaterialBlendFactor> RecordEnumValues<EMaterialBlendFactor>()
{
	static constexpr std::array Values{EMaterialBlendFactor::Zero,
	                                   EMaterialBlendFactor::One,
	                                   EMaterialBlendFactor::SourceColor,
	                                   EMaterialBlendFactor::InverseSourceColor,
	                                   EMaterialBlendFactor::SourceAlpha,
	                                   EMaterialBlendFactor::InverseSourceAlpha,
	                                   EMaterialBlendFactor::DestinationAlpha,
	                                   EMaterialBlendFactor::InverseDestinationAlpha,
	                                   EMaterialBlendFactor::DestinationColor,
	                                   EMaterialBlendFactor::InverseDestinationColor,
	                                   EMaterialBlendFactor::SourceAlphaSaturate,
	                                   EMaterialBlendFactor::Constant,
	                                   EMaterialBlendFactor::InverseConstant};
	return Values;
}

template<> std::span<const EMaterialBlendOp> RecordEnumValues<EMaterialBlendOp>()
{
	static constexpr std::array Values{EMaterialBlendOp::Add, EMaterialBlendOp::Subtract,
	                                   EMaterialBlendOp::ReverseSubtract, EMaterialBlendOp::Minimum,
	                                   EMaterialBlendOp::Maximum};
	return Values;
}

template<> std::span<const EMaterialQueue> RecordEnumValues<EMaterialQueue>()
{
	static constexpr std::array Values{EMaterialQueue::Opaque, EMaterialQueue::Masked, EMaterialQueue::Transparent,
	                                   EMaterialQueue::Overlay};
	return Values;
}
} // namespace Hyperion
