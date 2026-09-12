#include "Hyperion/Materials/PbrMaterial.h"
#include "Hyperion/Materials/MaterialBlocks.h"

namespace Hyperion
{
namespace
{
FMaterialPass PbrPass(EMaterialQueue InQueue, bool bInDoubleSided)
{
	FMaterialPass Pass;
	Pass.Vertex = {"Model.hlsl", "VSMain"};
	Pass.Pixel = {"Model.hlsl", "PSMain"};
	Pass.InstanceArrays = {{"HyperionObjectV1", "ObjectInstances"}, {"HyperionMaterialV1", "SurfaceInstances"}};
	Pass.bAllowBatchReordering = InQueue != EMaterialQueue::Transparent;
	Pass.bSrgbTarget = true;
	Pass.bAlphaClip = InQueue == EMaterialQueue::Masked;
	Pass.Queue = InQueue;
	Pass.State.bDepthTest = true;
	Pass.State.bViewRelativeDepth = true;
	Pass.State.bDepthWrite = InQueue != EMaterialQueue::Transparent;
	Pass.State.bBlend = InQueue == EMaterialQueue::Transparent;
	Pass.State.SourceRgb = EMaterialBlendFactor::SourceAlpha;
	Pass.State.DestinationRgb = EMaterialBlendFactor::InverseSourceAlpha;
	Pass.State.DestinationAlpha = EMaterialBlendFactor::InverseSourceAlpha;
	Pass.State.Cull = bInDoubleSided ? EMaterialCull::None : EMaterialCull::Back;
	return Pass;
}

FMaterialPass ShadowPass(EMaterialQueue InQueue, bool bInDoubleSided)
{
	auto Pass = PbrPass(InQueue, bInDoubleSided);
	Pass.Usage = "ShadowDepth";
	Pass.bSrgbTarget = false;
	Pass.State.ColorWriteMask = 0;
	Pass.State.DepthBias = 1;
	Pass.State.SlopeScaledDepthBias = 1.25f;
	Pass.State.DepthBiasClamp = .001f;
	Pass.Vertex.Defines = {{"HYP_SHADOW_CASTER", "1"}};
	if (Pass.bAlphaClip)
	{
		Pass.Pixel.Defines = Pass.Vertex.Defines;
	}
	else
	{
		Pass.Pixel = {};
		Pass.InstanceArrays = {{"HyperionObjectV1", "ObjectInstances"}};
	}
	return Pass;
}

void AddParameters(FMaterialDescription& InDescription)
{
	const auto Semantics = GetStandardMaterialSemantics();
	InDescription.Parameters = GetStandardMaterialBlockParameters("HyperionMaterialV1", *Semantics);
	const auto AddShadow = [&](std::string InName)
	{
		auto Parameter = DeclareMaterialSemantic("Engine.View." + InName, "Engine.View." + InName, *Semantics);
		Parameter.Targets = {Parameter.Type.Kind == EMaterialValueKind::Numeric ? "ShadowViewV1." + InName : InName};
		InDescription.Parameters.push_back(std::move(Parameter));
	};
	for (const auto* Name : {"ShadowSplits", "ShadowTexels", "ShadowRanges", "ShadowCamera", "ShadowFilter",
	                         "ShadowControl", "ShadowSampler"})
	{
		AddShadow(Name);
	}
	for (unsigned Index = 0; Index < 4; ++Index)
	{
		AddShadow("ShadowMatrix" + std::to_string(Index));
		AddShadow("ShadowDepth" + std::to_string(Index));
	}
	for (const std::string Role : {"BaseColor", "MetallicRoughness", "Normal", "Occlusion", "Emissive"})
	{
		for (const auto* Kind : {"Texture", "Sampler"})
		{
			auto Parameter = DeclareMaterialSemantic(Role + Kind, "Pbr." + Role + Kind, *Semantics);
			Parameter.Targets = {Role + Kind};
			InDescription.Parameters.push_back(std::move(Parameter));
		}
	}
}
} // namespace

FMaterialAsset MakePbrMaterialAsset(std::string InName, EMaterialQueue InQueue, bool bInDoubleSided, bool bInUnlit)
{
	if (InQueue == EMaterialQueue::Overlay)
	{
		throw std::invalid_argument("PBR import supports opaque, masked or transparent queues");
	}
	FMaterialDescription Description;
	Description.Name = InName.empty() ? "Builtin PBR" : std::move(InName);
	Description.Passes.push_back(PbrPass(InQueue, bInDoubleSided));
	auto Hdr = PbrPass(InQueue, bInDoubleSided);
	Hdr.bSrgbTarget = false;
	Hdr.Pixel.Defines = {{"HYP_FORWARD_HDR", "1"}};
	Hdr.Usage = InQueue == EMaterialQueue::Transparent ? "HdrTransparent" : "HdrForwardOpaque";
	Description.Passes.push_back(Hdr);
	if (InQueue != EMaterialQueue::Transparent)
	{
		if (bInUnlit)
		{
			Hdr.Usage = "HdrCompatibility";
			Description.Passes.push_back(Hdr);
		}
		else
		{
			auto Base = PbrPass(InQueue, bInDoubleSided);
			Base.Usage = "DeferredBase";
			Base.bSrgbTarget = false;
			Base.Pixel.Defines = {{"HYP_DEFERRED_BASE", "1"}};
			Description.Passes.push_back(std::move(Base));
		}
		Description.Passes.push_back(ShadowPass(InQueue, bInDoubleSided));
	}
	AddParameters(Description);
	return PersistMaterialDescription(Description);
}
} // namespace Hyperion
