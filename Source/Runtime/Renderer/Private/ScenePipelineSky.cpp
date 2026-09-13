#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include <cmath>

namespace Hyperion
{
namespace
{
std::shared_ptr<const FMaterialDefinition> SkyMaterial(bool bInReversed)
{
	FMaterialDescription Description;
	Description.Name = bInReversed ? "Sky reversed depth" : "Sky standard depth";
	FMaterialPass Pass;
	Pass.Vertex = {"Common/Sky.hlsl", "VSMain"};
	Pass.Vertex.Defines = {{"HYP_REVERSED_SKY", bInReversed ? "1" : "0"}};
	Pass.Pixel = {"Common/Sky.hlsl", "PSMain"};
	Pass.State.bDepthTest = true;
	Pass.State.bDepthWrite = false;
	Pass.State.bViewRelativeDepth = true;
	Pass.State.DepthCompare = EMaterialCompare::LessEqual;
	Description.Passes.push_back(std::move(Pass));
	return std::make_shared<const FMaterialDefinition>(std::move(Description));
}
} // namespace

void FSceneRenderPipeline::AddSky(FRenderGraph& InGraph, const FRenderView& InView,
                                  const FMaterialFrameContext& InFrame, bool bInDeferPreparation) const
{
	const auto Metadata = InFrame.GetSceneMetadata();
	if (!Metadata || !Metadata->Settings.EnvironmentLight)
	{
		return;
	}
	const auto It = Metadata->EnvironmentLights.find(*Metadata->Settings.EnvironmentLight);
	if (It == Metadata->EnvironmentLights.end() || !It->second.bEnabled)
	{
		return;
	}
	const auto& Light = It->second.Light;
	if (Light.Source != ESceneEnvironmentSource::SkyAsset || !Light.Data || !Light.bVisible)
	{
		return;
	}
	static const auto Standard = SkyMaterial(false);
	static const auto Reversed = SkyMaterial(true);
	FFullscreenPassDesc Pass;
	Pass.Material = InView.DepthConvention == EDepthConvention::Reversed ? Reversed : Standard;
	Pass.DepthConvention = InView.DepthConvention;
	Pass.Lifetime = Lifetime;
	Pass.ParameterLifetime = Light.Data;
	Pass.Statistics = FullscreenStatistics;
	Pass.Viewport = InView.Viewport.value_or(FViewport{0, 0, float(InView.Width), float(InView.Height)});
	Pass.Targets = ColorTargets("Scene/Sky", EAttachmentLoad::Load);
	Pass.Targets.DepthStencil = DepthTarget(EAttachmentLoad::Load);
	const auto View = Pass.Viewport;
	const auto& Camera = InView.Camera.value();
	// Remove translation before inversion; cancelling large world positions in the pixel shader loses direction.
	const auto SkyViewProjection = Multiply(
	    Perspective(Camera.VerticalRadians, View.Width / View.Height, Camera.Near, Camera.Far, InView.DepthConvention),
	    LookAt({}, Camera.Forward, Camera.Up));
	FMaterialSampler Sampler;
	Sampler.U = EMaterialAddressMode::Clamp;
	Sampler.V = EMaterialAddressMode::Clamp;
	Sampler.W = EMaterialAddressMode::Clamp;
	Pass.Parameters = {
	    {"Pixel:SkyViewV1.InverseSkyViewProjection", FMaterialValue::Matrix(Inverse(SkyViewProjection))},
	    {"Pixel:SkyViewV1.SkyViewport", FMaterialValue::Float(FVec4{View.X, View.Y, View.Width, View.Height})},
	    {"Pixel:SkyViewV1.SkyRotationIntensity",
	     FMaterialValue::Float(FVec4{std::cos(Light.YawRadians), std::sin(Light.YawRadians), Light.Intensity, 0})},
	    {"Pixel:SkyRadiance", FMaterialValue::FromTexture(Light.Data->Textures[0])},
	    {"Pixel:SkySampler", FMaterialValue::FromSampler(Sampler)}};
	AddFullscreenPass(Session, InGraph, std::move(Pass), bInDeferPreparation);
}
} // namespace Hyperion
