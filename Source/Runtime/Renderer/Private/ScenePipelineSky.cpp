#include "EnvironmentParameters.h"
#include "Hyperion/Materials/ShaderParameters.h"
#include "Hyperion/Renderer/SceneRenderPipeline.h"
#include "Hyperion/Renderer/ShaderParameters/SkyParameters.h"
#include <cmath>

namespace Hyperion
{
namespace
{
std::shared_ptr<const FMaterialDefinition> SkyMaterial(bool bInReversed)
{
	FMaterialDescription Description;
	Description.Name = bInReversed ? "Sky reversed depth" : "Sky standard depth";
	Description.ShaderContracts = {GetSkyShaderContracts()};
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
	if (!Metadata || !Metadata->Lighting.Environment.Handle)
	{
		return;
	}
	const auto It = Metadata->EnvironmentLights.find(*Metadata->Lighting.Environment.Handle);
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
	Pass.Targets.Timing.Category = ERenderPassTimingCategory::Sky;
	Pass.Targets.DepthStencil = DepthTarget(EAttachmentLoad::Load);
	const auto View = Pass.Viewport;
	const auto& Camera = InView.Camera.value();
	// Remove translation before inversion; cancelling large world positions in the pixel shader loses direction.
	const auto SkyViewProjection = Multiply(
	    Perspective(Camera.VerticalRadians, View.Width / View.Height, Camera.Near, Camera.Far, InView.DepthConvention),
	    LookAt({}, Camera.Forward, Camera.Up));
	const float Yaw = EnvironmentYawRadians(Light.YawDegrees);
	FMaterialSampler Sampler;
	Sampler.U = EMaterialAddressMode::Clamp;
	Sampler.V = EMaterialAddressMode::Clamp;
	Sampler.W = EMaterialAddressMode::Clamp;
	FSkyViewV1Parameters Parameters;
	Parameters.InverseSkyViewProjection = Inverse(SkyViewProjection);
	Parameters.SkyViewport = {View.X, View.Y, View.Width, View.Height};
	Parameters.SkyRotationIntensity = {std::cos(Yaw), std::sin(Yaw), Light.Intensity, 0};
	Parameters.SkyTint = {Light.Tint.X, Light.Tint.Y, Light.Tint.Z, 1};
	Pass.Parameters = MakeShaderParameters(Parameters);
	Pass.Parameters.push_back({ESkySemantic::SkyRadiance, FMaterialValue::FromTexture(Light.Data->Textures[0])});
	Pass.Parameters.push_back({ESkySemantic::SkySampler, FMaterialValue::FromSampler(Sampler)});
	AddFullscreenPass(Session, InGraph, std::move(Pass), bInDeferPreparation);
}
} // namespace Hyperion
