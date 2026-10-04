#include "Hyperion/Materials/MaterialAsset.h"
#include "MaterialPassPolicy.h"

namespace Hyperion
{
namespace
{
void MigratePass(FArchiveNode::FObject& InFields)
{
	FMaterialPass Pass;
	Pass.SilhouettePolicy.reset();
	if (const auto Vertex = InFields.find("vertex"); Vertex != InFields.end())
	{
		Pass.Vertex = ReadValue<FMaterialShader>(Vertex->second);
	}
	if (const auto Pixel = InFields.find("pixel"); Pixel != InFields.end())
	{
		Pass.Pixel = ReadValue<FMaterialShader>(Pixel->second);
	}
	MaterialsPrivate::NormalizeSilhouettePolicy(Pass);
	InFields.insert_or_assign("silhouettePolicy", WriteValue(Pass.SilhouettePolicy));
}
} // namespace

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
	static const auto Type = []
	{
		auto Result = MakeRecord<FMaterialPass>(
		    "hyperion.materialpass",
		    {Member("usage", &FMaterialPass::Usage), Member("vertex", &FMaterialPass::Vertex),
		     Member("pixel", &FMaterialPass::Pixel), Member("state", &FMaterialPass::State),
		     Member("dynamicState", &FMaterialPass::DynamicState), Member("queue", &FMaterialPass::Queue),
		     Member("srgbTarget", &FMaterialPass::bSrgbTarget), Member("alphaClip", &FMaterialPass::bAlphaClip),
		     Member("requiresConservativeBounds", &FMaterialPass::bRequiresConservativeBounds),
		     Member("allowDynamicOverrides", &FMaterialPass::bAllowDynamicOverrides),
		     Member("instanceArrays", &FMaterialPass::InstanceArrays),
		     Member("allowBatchReordering", &FMaterialPass::bAllowBatchReordering),
		     Member("silhouettePolicy", &FMaterialPass::SilhouettePolicy)},
		    2);
		Result.Create = []
		{
			auto Pass = std::make_shared<FMaterialPass>();
			Pass->SilhouettePolicy.reset();
			return Pass;
		};
		Result.Migrations.emplace(1, MigratePass);
		return Result;
	}();
	return Type;
}
} // namespace Hyperion
