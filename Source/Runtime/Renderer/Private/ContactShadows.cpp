#include "Hyperion/Renderer/ContactShadows.h"
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

void FContactShadowSettings::Validate() const
{
	if (!std::isfinite(Length) || Length <= 0 || Length > 100 || !std::isfinite(Thickness) || Thickness <= 0 ||
	    Thickness > 10 || !std::isfinite(Bias) || Bias < 0 || Bias > Length || Steps < 8 || Steps > 512 ||
	    DebugMode > 2 || PreviewMip > 16)
	{
		throw std::invalid_argument("Invalid contact shadow length, thickness, bias, steps or preview");
	}
}

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
	static const auto Material = MakeFullscreenMaterial("Contact shadows", "Depth/ContactShadows.hlsl");
	FFullscreenPassDesc Pass;
	Pass.Material = Material;
	Pass.Lifetime = InInputs.Mask.Lifetime;
	Pass.ResourceLifetime = InInputs.Normals.Lifetime;
	Pass.DepthConvention = InInputs.View.DepthConvention;
	Pass.bFullTargetViewport = true;
	Pass.Viewport =
	    InInputs.View.Viewport.value_or(FViewport{0, 0, float(InInputs.View.Width), float(InInputs.View.Height)});
	Pass.Targets.Name = "Deferred/ContactShadowMask";
	Pass.Targets.Color =
	    FRenderColorTarget{InInputs.Mask, {EAttachmentLoad::Clear}, {1, 1, 1, 1}, EGraphColorView::Linear};
	Pass.Targets.Reads = {{ERenderTargetKind::Texture, InInputs.Depth.Texture, InInputs.Depth.Lifetime, false},
	                      InInputs.Normals,
	                      InInputs.Surface};
	Pass.Parameters = {{"Pixel:HierarchicalDepth", FMaterialValue::FromTexture(InInputs.Depth.Texture)},
	                   {"Pixel:SurfaceNormals", FMaterialValue::FromTexture(InInputs.Normals.Texture)},
	                   {"Pixel:SurfaceCoverage", FMaterialValue::FromTexture(InInputs.Surface.Texture)}};
	const auto Set = [&](std::string InName, FMaterialValue InValue)
	{
		Pass.Parameters.push_back({"Pixel:ContactV1." + InName, std::move(InValue)});
	};
	Set("ViewProjection", FMaterialValue::Matrix(InInputs.View.ViewProjection));
	Set("InverseViewProjection", FMaterialValue::Matrix(Inverse(InInputs.View.ViewProjection)));
	Set("Viewport",
	    FMaterialValue::Float(FVec4{Pass.Viewport.X, Pass.Viewport.Y, Pass.Viewport.Width, Pass.Viewport.Height}));
	Set("Eye", FMaterialValue::Float(InInputs.View.Eye));
	Set("LightDirection", FMaterialValue::Float(Normalize(InInputs.LightDirection)));
	Set("RayLength", FMaterialValue::Float(InInputs.Settings.Length));
	Set("Thickness", FMaterialValue::Float(InInputs.Settings.Thickness));
	Set("Bias", FMaterialValue::Float(InInputs.Settings.Bias));
	Set("MaxSteps", FMaterialValue::Uint(InInputs.Settings.Steps));
	Set("bReversed", FMaterialValue::Uint(InInputs.View.DepthConvention == EDepthConvention::Reversed ? 1 : 0));
	return Pass;
}

FFullscreenPassDesc MakeScreenTexturePreview(FRenderTargetSource InSource, FViewport InViewport, std::uint32_t InMip,
                                             bool bInInvert)
{
	static const auto Material = MakeFullscreenMaterial("Screen texture preview", "Depth/ContactDebug.hlsl", true);
	FFullscreenPassDesc Pass;
	Pass.Material = Material;
	Pass.Lifetime = InSource.Lifetime;
	Pass.Viewport = InViewport;
	Pass.Targets = FRenderPassTargets::ColorOnly();
	Pass.Targets.Name = "Debug/ContactDepth";
	Pass.Targets.Reads = {InSource};
	Pass.Parameters = {{"Pixel:Source", FMaterialValue::FromTexture(InSource.Texture)},
	                   {"Pixel:PreviewV1.Mip", FMaterialValue::Uint(InMip)},
	                   {"Pixel:PreviewV1.bInvert", FMaterialValue::Uint(bInInvert ? 1 : 0)},
	                   {"Pixel:PreviewV1.Viewport",
	                    FMaterialValue::Float(FVec4{InViewport.X, InViewport.Y, InViewport.Width, InViewport.Height})}};
	return Pass;
}
} // namespace Hyperion
