#include "Hyperion/Materials/MaterialAsset.h"

namespace Hyperion
{
template<> std::span<const TRecordEnumEntry<EMaterialValueKind>> RecordEnumEntries<EMaterialValueKind>()
{
	static constexpr TRecordEnumEntry<EMaterialValueKind> Values[] = {
	    {EMaterialValueKind::Numeric, "Numeric", ""},
	    {EMaterialValueKind::Structure, "Structure", ""},
	    {EMaterialValueKind::Array, "Array", ""},
	    {EMaterialValueKind::Texture2D, "Texture2D", ""},
	    {EMaterialValueKind::ReadBuffer, "ReadBuffer", ""},
	    {EMaterialValueKind::Sampler, "Sampler", ""},
	    {EMaterialValueKind::TextureCube, "TextureCube", ""}};
	return Values;
}

template<> std::span<const TRecordEnumEntry<EMaterialScalar>> RecordEnumEntries<EMaterialScalar>()
{
	static constexpr TRecordEnumEntry<EMaterialScalar> Values[] = {{EMaterialScalar::Bool, "Bool", ""},
	                                                               {EMaterialScalar::Int, "Int", ""},
	                                                               {EMaterialScalar::Uint, "Uint", ""},
	                                                               {EMaterialScalar::Float, "Float", ""}};
	return Values;
}

template<> std::span<const TRecordEnumEntry<EMaterialParameterSource>> RecordEnumEntries<EMaterialParameterSource>()
{
	static constexpr TRecordEnumEntry<EMaterialParameterSource> Values[] = {
	    {EMaterialParameterSource::Manual, "Manual", ""}, {EMaterialParameterSource::Semantic, "Semantic", ""}};
	return Values;
}

template<> std::span<const TRecordEnumEntry<EMaterialOverridePolicy>> RecordEnumEntries<EMaterialOverridePolicy>()
{
	static constexpr TRecordEnumEntry<EMaterialOverridePolicy> Values[] = {
	    {EMaterialOverridePolicy::Locked, "Locked", ""}, {EMaterialOverridePolicy::AllowOverride, "AllowOverride", ""}};
	return Values;
}

template<> std::span<const TRecordEnumEntry<EMaterialAddressMode>> RecordEnumEntries<EMaterialAddressMode>()
{
	static constexpr TRecordEnumEntry<EMaterialAddressMode> Values[] = {
	    {EMaterialAddressMode::Repeat, "Repeat", ""},
	    {EMaterialAddressMode::Clamp, "Clamp", ""},
	    {EMaterialAddressMode::Mirror, "Mirror", ""},
	    {EMaterialAddressMode::Border, "Border", ""},
	    {EMaterialAddressMode::MirrorOnce, "MirrorOnce", ""}};
	return Values;
}

template<> std::span<const TRecordEnumEntry<EMaterialSamplerCompare>> RecordEnumEntries<EMaterialSamplerCompare>()
{
	static constexpr TRecordEnumEntry<EMaterialSamplerCompare> Values[] = {
	    {EMaterialSamplerCompare::Never, "Never", ""},
	    {EMaterialSamplerCompare::Less, "Less", ""},
	    {EMaterialSamplerCompare::Equal, "Equal", ""},
	    {EMaterialSamplerCompare::LessEqual, "LessEqual", ""},
	    {EMaterialSamplerCompare::Greater, "Greater", ""},
	    {EMaterialSamplerCompare::NotEqual, "NotEqual", ""},
	    {EMaterialSamplerCompare::GreaterEqual, "GreaterEqual", ""},
	    {EMaterialSamplerCompare::Always, "Always", ""}};
	return Values;
}

template<> std::span<const TRecordEnumEntry<EMaterialFill>> RecordEnumEntries<EMaterialFill>()
{
	static constexpr TRecordEnumEntry<EMaterialFill> Values[] = {{EMaterialFill::Solid, "Solid", ""},
	                                                             {EMaterialFill::Wireframe, "Wireframe", ""}};
	return Values;
}

template<> std::span<const TRecordEnumEntry<EMaterialCull>> RecordEnumEntries<EMaterialCull>()
{
	static constexpr TRecordEnumEntry<EMaterialCull> Values[] = {
	    {EMaterialCull::None, "None", ""}, {EMaterialCull::Front, "Front", ""}, {EMaterialCull::Back, "Back", ""}};
	return Values;
}

template<> std::span<const TRecordEnumEntry<EMaterialCompare>> RecordEnumEntries<EMaterialCompare>()
{
	static constexpr TRecordEnumEntry<EMaterialCompare> Values[] = {
	    {EMaterialCompare::Never, "Never", ""},
	    {EMaterialCompare::Less, "Less", ""},
	    {EMaterialCompare::Equal, "Equal", ""},
	    {EMaterialCompare::LessEqual, "LessEqual", ""},
	    {EMaterialCompare::Greater, "Greater", ""},
	    {EMaterialCompare::NotEqual, "NotEqual", ""},
	    {EMaterialCompare::GreaterEqual, "GreaterEqual", ""},
	    {EMaterialCompare::Always, "Always", ""}};
	return Values;
}

template<> std::span<const TRecordEnumEntry<EMaterialStencilOp>> RecordEnumEntries<EMaterialStencilOp>()
{
	static constexpr TRecordEnumEntry<EMaterialStencilOp> Values[] = {
	    {EMaterialStencilOp::Keep, "Keep", ""},
	    {EMaterialStencilOp::Zero, "Zero", ""},
	    {EMaterialStencilOp::Replace, "Replace", ""},
	    {EMaterialStencilOp::IncrementClamp, "IncrementClamp", ""},
	    {EMaterialStencilOp::DecrementClamp, "DecrementClamp", ""},
	    {EMaterialStencilOp::Invert, "Invert", ""},
	    {EMaterialStencilOp::IncrementWrap, "IncrementWrap", ""},
	    {EMaterialStencilOp::DecrementWrap, "DecrementWrap", ""}};
	return Values;
}

template<> std::span<const TRecordEnumEntry<EMaterialBlendFactor>> RecordEnumEntries<EMaterialBlendFactor>()
{
	static constexpr TRecordEnumEntry<EMaterialBlendFactor> Values[] = {
	    {EMaterialBlendFactor::Zero, "Zero", ""},
	    {EMaterialBlendFactor::One, "One", ""},
	    {EMaterialBlendFactor::SourceColor, "SourceColor", ""},
	    {EMaterialBlendFactor::InverseSourceColor, "InverseSourceColor", ""},
	    {EMaterialBlendFactor::SourceAlpha, "SourceAlpha", ""},
	    {EMaterialBlendFactor::InverseSourceAlpha, "InverseSourceAlpha", ""},
	    {EMaterialBlendFactor::DestinationAlpha, "DestinationAlpha", ""},
	    {EMaterialBlendFactor::InverseDestinationAlpha, "InverseDestinationAlpha", ""},
	    {EMaterialBlendFactor::DestinationColor, "DestinationColor", ""},
	    {EMaterialBlendFactor::InverseDestinationColor, "InverseDestinationColor", ""},
	    {EMaterialBlendFactor::SourceAlphaSaturate, "SourceAlphaSaturate", ""},
	    {EMaterialBlendFactor::Constant, "Constant", ""},
	    {EMaterialBlendFactor::InverseConstant, "InverseConstant", ""}};
	return Values;
}

template<> std::span<const TRecordEnumEntry<EMaterialBlendOp>> RecordEnumEntries<EMaterialBlendOp>()
{
	static constexpr TRecordEnumEntry<EMaterialBlendOp> Values[] = {
	    {EMaterialBlendOp::Add, "Add", ""},
	    {EMaterialBlendOp::Subtract, "Subtract", ""},
	    {EMaterialBlendOp::ReverseSubtract, "ReverseSubtract", ""},
	    {EMaterialBlendOp::Minimum, "Minimum", ""},
	    {EMaterialBlendOp::Maximum, "Maximum", ""}};
	return Values;
}

template<> std::span<const TRecordEnumEntry<EMaterialQueue>> RecordEnumEntries<EMaterialQueue>()
{
	static constexpr TRecordEnumEntry<EMaterialQueue> Values[] = {{EMaterialQueue::Opaque, "Opaque", ""},
	                                                              {EMaterialQueue::Masked, "Masked", ""},
	                                                              {EMaterialQueue::Transparent, "Transparent", ""},
	                                                              {EMaterialQueue::Overlay, "Overlay", ""}};
	return Values;
}
} // namespace Hyperion
