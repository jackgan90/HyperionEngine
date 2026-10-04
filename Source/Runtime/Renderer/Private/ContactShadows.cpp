#include "Hyperion/Renderer/ContactShadows.h"
#include "Hyperion/Materials/ShaderParameters.h"
#include "Hyperion/Renderer/ShaderParameters/ContactShadowParameters.h"
#include "Hyperion/Renderer/ShaderParameters/DepthPreviewParameters.h"
#include <cmath>

namespace Hyperion
{
namespace
{
bool IsMatchingColor(const FRenderTargetSource& InSource, const FRenderView& InView)
{
	const auto* Color = InSource.Texture ? InSource.Texture->GetColorTarget() : nullptr;
	return InSource.Kind == ERenderTargetKind::Texture && InSource.Lifetime && Color && Color->Width == InView.Width &&
	       Color->Height == InView.Height;
}
} // namespace

FFullscreenPassDesc MakeContactShadowPass(const FRenderGraph& InGraph, const FContactShadowInputs& InInputs)
{
	InInputs.Settings.Validate();
	InInputs.Depth.Validate(InGraph, InInputs.View);
	const auto* Storage = InInputs.Depth.Texture ? InInputs.Depth.Texture->GetStorage() : nullptr;
	if (!Storage || Storage->Width != InInputs.View.Width || Storage->Height != InInputs.View.Height ||
	    Storage->Format != EMaterialColorFormat::R32Float || Storage->MipCount != InInputs.Depth.MipSizes.size() ||
	    !InInputs.Depth.Lifetime || !IsMatchingColor(InInputs.Mask, InInputs.View) ||
	    !IsMatchingColor(InInputs.Normals, InInputs.View) || !IsMatchingColor(InInputs.Surface, InInputs.View) ||
	    InInputs.Mask.Texture->GetColorTarget()->Format != EMaterialColorFormat::R8Unorm)
	{
		throw std::invalid_argument("Contact shadow inputs must match the view and R32F hierarchy/R8 mask formats");
	}
	if (!InInputs.Depth.Texture || InInputs.Depth.Reduction != EDepthReduction::Nearest ||
	    InInputs.Depth.Convention != InInputs.View.DepthConvention || !InInputs.Mask.Texture ||
	    !InInputs.Mask.Lifetime || !InInputs.Normals.Texture || !InInputs.Surface.Texture ||
	    !std::isfinite(Length(InInputs.LightDirection)) || Length(InInputs.LightDirection) < 1e-6f)
	{
		throw std::invalid_argument("Contact shadow requires matching nearest HZB, GBuffer, mask and light");
	}
	static const auto Material = MakeFullscreenMaterial("Contact shadows", "Depth/ContactShadows.hlsl", false,
	                                                    {GetContactShadowShaderContracts()});
	FFullscreenPassDesc Pass;
	Pass.Material = Material;
	Pass.Lifetime = InInputs.Mask.Lifetime;
	Pass.ResourceLifetime = InInputs.Normals.Lifetime;
	Pass.DepthConvention = InInputs.View.DepthConvention;
	Pass.bFullTargetViewport = true;
	Pass.Viewport =
	    InInputs.View.Viewport.value_or(FViewport{0, 0, float(InInputs.View.Width), float(InInputs.View.Height)});
	Pass.Targets.Name = "Deferred/ContactShadowMask";
	Pass.Targets.Timing.Category = ERenderPassTimingCategory::ContactShadow;
	Pass.Targets.Color =
	    FRenderColorTarget{InInputs.Mask, {EAttachmentLoad::Clear}, {1, 1, 1, 1}, EGraphColorView::Linear};
	Pass.Targets.Reads = {{ERenderTargetKind::Texture, InInputs.Depth.Texture, InInputs.Depth.Lifetime, false},
	                      InInputs.Normals,
	                      InInputs.Surface};
	Pass.Parameters = {
	    {EContactShadowSemantic::HierarchicalDepth, FMaterialValue::FromTexture(InInputs.Depth.Texture)},
	    {EContactShadowSemantic::SurfaceNormals, FMaterialValue::FromTexture(InInputs.Normals.Texture)},
	    {EContactShadowSemantic::SurfaceCoverage, FMaterialValue::FromTexture(InInputs.Surface.Texture)}};
	FContactV1Parameters Parameters;
	Parameters.ViewProjection = InInputs.View.ViewProjection;
	Parameters.InverseViewProjection = Inverse(InInputs.View.ViewProjection);
	Parameters.Viewport = {Pass.Viewport.X, Pass.Viewport.Y, Pass.Viewport.Width, Pass.Viewport.Height};
	Parameters.Eye = InInputs.View.Eye;
	Parameters.LightDirection = Normalize(InInputs.LightDirection);
	Parameters.RayLength = InInputs.Settings.Length;
	Parameters.Thickness = InInputs.Settings.Thickness;
	Parameters.Bias = InInputs.Settings.Bias;
	Parameters.MaxSteps = InInputs.Settings.Steps;
	Parameters.bReversed = InInputs.View.DepthConvention == EDepthConvention::Reversed;
	AppendShaderParameters(Pass.Parameters, Parameters);
	return Pass;
}

FFullscreenPassDesc MakeScreenTexturePreview(FRenderTargetSource InSource, FViewport InViewport, std::uint32_t InMip,
                                             bool bInInvert)
{
	static const auto Material = MakeFullscreenMaterial("Screen texture preview", "Depth/ContactDebug.hlsl", true,
	                                                    {GetDepthPreviewShaderContracts()});
	FFullscreenPassDesc Pass;
	Pass.Material = Material;
	Pass.Lifetime = InSource.Lifetime;
	Pass.Viewport = InViewport;
	Pass.Targets = FRenderPassTargets::ColorOnly();
	Pass.Targets.Name = "Debug/ContactDepth";
	Pass.Targets.Reads = {InSource};
	Pass.Parameters = {{EDepthPreviewSemantic::PreviewSource, FMaterialValue::FromTexture(InSource.Texture)}};
	AppendShaderParameters(
	    Pass.Parameters,
	    FPreviewV1Parameters{InMip, bInInvert, {InViewport.X, InViewport.Y, InViewport.Width, InViewport.Height}});
	return Pass;
}
} // namespace Hyperion
