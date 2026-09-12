#include "Hyperion/Materials/MaterialAsset.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FMaterialStencilFace>()
{
	static const auto Type = MakeRecord<FMaterialStencilFace>(
	    "hyperion.materialstencilface",
	    {Member("compare", &FMaterialStencilFace::Compare), Member("fail", &FMaterialStencilFace::Fail),
	     Member("depthFail", &FMaterialStencilFace::DepthFail), Member("pass", &FMaterialStencilFace::Pass)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FMaterialState>()
{
	static const auto Type = MakeRecord<FMaterialState>(
	    "hyperion.materialstate", {Member("fill", &FMaterialState::Fill),
	                               Member("cull", &FMaterialState::Cull),
	                               Member("frontCounterClockwise", &FMaterialState::bFrontCounterClockwise),
	                               Member("depthBias", &FMaterialState::DepthBias),
	                               Member("depthBiasClamp", &FMaterialState::DepthBiasClamp),
	                               Member("slopeScaledDepthBias", &FMaterialState::SlopeScaledDepthBias),
	                               Member("depthClip", &FMaterialState::bDepthClip),
	                               Member("depthTest", &FMaterialState::bDepthTest),
	                               Member("depthWrite", &FMaterialState::bDepthWrite),
	                               Member("depthCompare", &FMaterialState::DepthCompare),
	                               Member("stencil", &FMaterialState::bStencil),
	                               Member("frontStencil", &FMaterialState::FrontStencil),
	                               Member("backStencil", &FMaterialState::BackStencil),
	                               Member("stencilReadMask", &FMaterialState::StencilReadMask),
	                               Member("stencilWriteMask", &FMaterialState::StencilWriteMask),
	                               Member("blend", &FMaterialState::bBlend),
	                               Member("sourceRgb", &FMaterialState::SourceRgb),
	                               Member("destinationRgb", &FMaterialState::DestinationRgb),
	                               Member("rgbOperation", &FMaterialState::RgbOperation),
	                               Member("sourceAlpha", &FMaterialState::SourceAlpha),
	                               Member("destinationAlpha", &FMaterialState::DestinationAlpha),
	                               Member("alphaOperation", &FMaterialState::AlphaOperation),
	                               Member("colorWriteMask", &FMaterialState::ColorWriteMask),
	                               Member("alphaToCoverage", &FMaterialState::bAlphaToCoverage),
	                               Member("sampleMask", &FMaterialState::SampleMask),
	                               Member("viewRelativeDepth", &FMaterialState::bViewRelativeDepth)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FMaterialDynamicState>()
{
	static const auto Type = MakeRecord<FMaterialDynamicState>(
	    "hyperion.materialdynamicstate", {Member("stencilReference", &FMaterialDynamicState::StencilReference),
	                                      Member("blendConstants", &FMaterialDynamicState::BlendConstants)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FMaterialShaderDefine>()
{
	static const auto Type = MakeRecord<FMaterialShaderDefine>(
	    "hyperion.materialshaderdefine",
	    {Member("name", &FMaterialShaderDefine::Name), Member("value", &FMaterialShaderDefine::Value)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FMaterialShader>()
{
	static const auto Type = MakeRecord<FMaterialShader>(
	    "hyperion.materialshader", {Member("path", &FMaterialShader::Path), Member("entry", &FMaterialShader::Entry),
	                                Member("defines", &FMaterialShader::Defines)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FMaterialInstanceArray>()
{
	static const auto Type = MakeRecord<FMaterialInstanceArray>(
	    "hyperion.materialinstancearray",
	    {Member("block", &FMaterialInstanceArray::Block), Member("member", &FMaterialInstanceArray::Member)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FMaterialPass>()
{
	static const auto Type = MakeRecord<FMaterialPass>(
	    "hyperion.materialpass",
	    {Member("usage", &FMaterialPass::Usage), Member("vertex", &FMaterialPass::Vertex),
	     Member("pixel", &FMaterialPass::Pixel), Member("state", &FMaterialPass::State),
	     Member("dynamicState", &FMaterialPass::DynamicState), Member("queue", &FMaterialPass::Queue),
	     Member("srgbTarget", &FMaterialPass::bSrgbTarget), Member("alphaClip", &FMaterialPass::bAlphaClip),
	     Member("requiresConservativeBounds", &FMaterialPass::bRequiresConservativeBounds),
	     Member("allowDynamicOverrides", &FMaterialPass::bAllowDynamicOverrides),
	     Member("instanceArrays", &FMaterialPass::InstanceArrays),
	     Member("allowBatchReordering", &FMaterialPass::bAllowBatchReordering)});
	return Type;
}
} // namespace Hyperion
