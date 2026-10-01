#include "Hyperion/Materials/ShaderParameters.h"
#include "Hyperion/Renderer/FullscreenPass.h"
#include "Hyperion/Renderer/SelectionOutline.h"
#include "Hyperion/Renderer/ShaderParameters/OutlineParameters.h"
#include <cmath>

namespace Hyperion
{
namespace
{
std::shared_ptr<const FMaterialDefinition> OutlineMaterial(bool bInComposite)
{
	FMaterialDescription Description;
	Description.Name = bInComposite ? "Outline composite" : "Silhouette exterior";
	Description.ShaderContracts = {GetOutlineShaderContracts()};
	FMaterialPass Pass;
	Pass.Vertex = {"Common/Fullscreen.hlsl", "VSMain"};
	Pass.Pixel = {bInComposite ? "Outline/Composite.hlsl" : "Outline/Exterior.hlsl", "PSMain"};
	Pass.bSrgbTarget = bInComposite;
	Pass.State.bBlend = true;
	Pass.State.ColorWriteMask = bInComposite ? 7 : 1;
	Pass.State.SourceRgb = bInComposite ? EMaterialBlendFactor::SourceAlpha : EMaterialBlendFactor::One;
	Pass.State.DestinationRgb = bInComposite ? EMaterialBlendFactor::InverseSourceAlpha : EMaterialBlendFactor::One;
	Pass.State.RgbOperation = bInComposite ? EMaterialBlendOp::Add : EMaterialBlendOp::Maximum;
	Description.Passes.push_back(std::move(Pass));
	return std::make_shared<const FMaterialDefinition>(std::move(Description));
}
} // namespace

void AddSilhouetteOutlinePass(FRenderSession& InSession, FRenderGraph& InGraph, FRenderTargetSource InMask,
                              FRenderTargetSource InOutput, FViewport InViewport, float InWidth, bool bInClear,
                              bool bInDeferPreparation, std::string InName, std::optional<FRect> InScissor)
{
	static const auto Material = OutlineMaterial(false);
	const auto* Mask = InMask.Texture ? InMask.Texture->GetColorTarget() : nullptr;
	const auto* Output = InOutput.Texture ? InOutput.Texture->GetColorTarget() : nullptr;
	if (!Mask || !Output || InMask.Texture == InOutput.Texture || !std::isfinite(InWidth) || InWidth <= 0 ||
	    InWidth > 8 || !Output->Width || !Output->Height || Mask->Width % Output->Width ||
	    Mask->Height % Output->Height || Mask->Width / Output->Width != Mask->Height / Output->Height ||
	    (Mask->Width / Output->Width != 1 && Mask->Width / Output->Width != 2))
	{
		throw std::invalid_argument("Outline masks require matching or 2x dimensions and a width in (0, 8]");
	}
	FFullscreenPassDesc Pass;
	Pass.Material = Material;
	Pass.Lifetime = InOutput.Lifetime;
	Pass.Viewport = InViewport;
	Pass.Scissor = InScissor;
	// The composite samples the whole attachment; scissor still limits the exterior shader's work.
	Pass.bFullTargetViewport = bInClear;
	Pass.Targets.Name = std::move(InName);
	Pass.Targets.Color = FRenderColorTarget{
	    InOutput, {bInClear ? EAttachmentLoad::Clear : EAttachmentLoad::Load}, {}, EGraphColorView::Linear};
	Pass.Targets.Reads = {InMask};
	Pass.Parameters = {{EOutlineSemantic::OutlineObjectMask, FMaterialValue::FromTexture(InMask.Texture)}};
	AppendShaderParameters(Pass.Parameters, FOutlineV1Parameters{{InWidth, float(Mask->Width / Output->Width), 0, 0}});
	AddFullscreenPass(InSession, InGraph, std::move(Pass), bInDeferPreparation);
}

void AddOutlineCompositePass(FRenderSession& InSession, FRenderGraph& InGraph, FRenderTargetSource InOutline,
                             FRenderTargetSource InOutput, FViewport InViewport, FVec4 InColor,
                             bool bInDeferPreparation, std::string InName)
{
	static const auto Material = OutlineMaterial(true);
	FFullscreenPassDesc Pass;
	Pass.Material = Material;
	Pass.Lifetime = InOutline.Lifetime;
	Pass.Viewport = InViewport;
	Pass.Targets.Name = std::move(InName);
	Pass.Targets.Color = FRenderColorTarget{InOutput, {EAttachmentLoad::Load}, {}, EGraphColorView::Srgb};
	Pass.Targets.Reads = {InOutline};
	Pass.Parameters = {{EOutlineSemantic::OutlineMask, FMaterialValue::FromTexture(InOutline.Texture)}};
	AppendShaderParameters(Pass.Parameters, FOutlineColorV1Parameters{InColor});
	AddFullscreenPass(InSession, InGraph, std::move(Pass), bInDeferPreparation);
}
} // namespace Hyperion
