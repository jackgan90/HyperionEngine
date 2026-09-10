#include "Hyperion/Renderer/CascadedShadowMap.h"
#include "Hyperion/Core/Profiling.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>

namespace Hyperion
{
namespace
{
// Infinite receiver extrusion towards the light; independent of main-camera visibility.
class FCasterVolume final : public ISceneVisibility
{
public:
	FVec3 Right;
	FVec3 Up;
	FVec3 Light;
	FVec3 Center;
	float Radius{};

	bool Intersects(const FBounds& InBounds) const override
	{
		if (!IsUsable(InBounds))
		{
			return true;
		}
		const auto Mid = ScaleVector(Add(InBounds.Minimum, InBounds.Maximum), .5f);
		const auto Extent = ScaleVector(Subtract(InBounds.Maximum, InBounds.Minimum), .5f);
		const auto Delta = Subtract(Mid, Center);
		const auto Support = [Extent](FVec3 InAxis)
		{
			return std::abs(InAxis.X) * Extent.X + std::abs(InAxis.Y) * Extent.Y + std::abs(InAxis.Z) * Extent.Z;
		};
		return std::abs(Dot(Delta, Right)) <= Radius + Support(Right) &&
		       std::abs(Dot(Delta, Up)) <= Radius + Support(Up) && Dot(Delta, Light) + Support(Light) >= -Radius;
	}
};

FMat4 Projection(FVec3 InRight, FVec3 InUp, FVec3 InLight, FVec3 InCenter, float InRadius, float InNear, float InFar)
{
	FMat4 Result = Identity();
	const FVec3 Depth = ScaleVector(InLight, -1.f / (InFar - InNear));
	const FVec3 X = ScaleVector(InRight, 1.f / InRadius);
	const FVec3 Y = ScaleVector(InUp, 1.f / InRadius);
	Result.Values = {X.X,
	                 Y.X,
	                 Depth.X,
	                 0,
	                 X.Y,
	                 Y.Y,
	                 Depth.Y,
	                 0,
	                 X.Z,
	                 Y.Z,
	                 Depth.Z,
	                 0,
	                 -Dot(X, InCenter),
	                 -Dot(Y, InCenter),
	                 -InNear / (InFar - InNear),
	                 1};
	return Result;
}

bool Valid(const FRenderView& InMain, FVec3 InLight, const FCascadedShadowSettings& InSettings)
{
	if (!InMain.Camera || !IsFinite(InMain.Eye) || !IsFinite(InLight) || !std::isfinite(Length(InLight)) ||
	    Length(InLight) < .0001f || (InSettings.Resolution != 1024 && InSettings.Resolution != 2048))
	{
		return false;
	}
	const auto& Camera = *InMain.Camera;
	return IsFinite(Camera.Forward) && IsFinite(Camera.Up) && Length(Camera.Forward) > .0001f &&
	       std::isfinite(Length(Camera.Forward)) && std::isfinite(Length(Camera.Up)) &&
	       Length(Cross(Camera.Forward, Camera.Up)) > .0001f && std::isfinite(Camera.Near) &&
	       std::isfinite(Camera.Far) && Camera.Near > 0 && Camera.Far > Camera.Near &&
	       std::isfinite(Camera.VerticalRadians) && Camera.VerticalRadians > .01f && Camera.VerticalRadians < 3 &&
	       std::isfinite(InSettings.Distance) && InSettings.Distance > Camera.Near &&
	       std::isfinite(InSettings.SplitLambda) && std::isfinite(InSettings.NormalOffset) &&
	       std::isfinite(InSettings.ReceiverBias) && std::isfinite(InSettings.BlendFraction) &&
	       std::isfinite(InSettings.FadeFraction) && InMain.Width > 0 && InMain.Height > 0;
}

std::array<std::uint64_t, 26> PreparationKey(const FRenderView& InView, FVec3 InLight,
                                             const FCascadedShadowSettings& InSettings,
                                             const std::array<std::uint64_t, 2>& InSceneState)
{
	const auto& Camera = *InView.Camera;
	const std::array Values{InView.Eye.X,
	                        InView.Eye.Y,
	                        InView.Eye.Z,
	                        Camera.Forward.X,
	                        Camera.Forward.Y,
	                        Camera.Forward.Z,
	                        Camera.Up.X,
	                        Camera.Up.Y,
	                        Camera.Up.Z,
	                        Camera.Near,
	                        Camera.Far,
	                        Camera.VerticalRadians,
	                        InLight.X,
	                        InLight.Y,
	                        InLight.Z,
	                        InSettings.Distance,
	                        InSettings.SplitLambda,
	                        InSettings.NormalOffset,
	                        InSettings.ReceiverBias,
	                        InSettings.BlendFraction,
	                        InSettings.FadeFraction};
	std::array<std::uint64_t, 26> Result{InSceneState[0], InSceneState[1], InView.Width, InView.Height,
	                                     InSettings.Resolution};
	for (std::size_t Index = 0; Index < Values.size(); ++Index)
	{
		Result[Index + 5] = std::bit_cast<std::uint32_t>(Values[Index]);
	}
	return Result;
}
} // namespace

FCascadedShadowMap::FCascadedShadowMap()
{
	static std::atomic_uint64_t NextView{1ULL << 48};
	for (auto& Id : ViewIds)
	{
		Id = NextView.fetch_add(1);
	}
}

bool FCascadedShadowMap::Prepare(const FRenderView& InMain, FVec3 InSurfaceToLight,
                                 const FCascadedShadowSettings& InSettings, const FBoundsQuery& InQuery,
                                 std::optional<std::array<std::uint64_t, 2>> InSceneState)
{
	HYP_PERF_SCOPE_C(Render, PrepareShadowCascades);
	bEnabled = InSettings.bEnabled && Valid(InMain, InSurfaceToLight, InSettings);
	if (!bEnabled)
	{
		PreparedKey.reset();
		return false;
	}
	Settings = InSettings;
	Settings.Distance = std::min(Settings.Distance, InMain.Camera->Far);
	Settings.SplitLambda = std::clamp(Settings.SplitLambda, 0.f, 1.f);
	Settings.NormalOffset = std::clamp(Settings.NormalOffset, 0.f, 2.f);
	Settings.ReceiverBias = std::clamp(Settings.ReceiverBias, 0.f, 2.f);
	Settings.BlendFraction = std::clamp(Settings.BlendFraction, .01f, .25f);
	Settings.FadeFraction = std::clamp(Settings.FadeFraction, .01f, .5f);
	const auto Key =
	    InSceneState ? std::optional(PreparationKey(InMain, InSurfaceToLight, Settings, *InSceneState)) : std::nullopt;
	if (Key && Key == PreparedKey)
	{
		HYP_PERF_PLOT(Render, ShadowSetupReuses, 1.0);
		return true;
	}
	PreparedKey.reset();
	HYP_PERF_PLOT(Render, ShadowSetupReuses, 0.0);
	const auto Direction = Normalize(InSurfaceToLight);
	if (Light.X != Direction.X || Light.Y != Direction.Y || Light.Z != Direction.Z)
	{
		Light = Direction;
		auto Right = Subtract(LightRight, ScaleVector(Light, Dot(LightRight, Light)));
		if (Length(Right) < .01f)
		{
			Right = Cross(std::abs(Light.Y) < .9f ? FVec3{0, 1, 0} : FVec3{0, 0, 1}, Light);
		}
		LightRight = Normalize(Right);
		LightUp = Normalize(Cross(Light, LightRight));
	}
	float Near = InMain.Camera->Near;
	for (std::size_t Index = 0; Index < Data.size(); ++Index)
	{
		const float Fraction = float(Index + 1) / Data.size();
		const float Uniform = InMain.Camera->Near + (Settings.Distance - InMain.Camera->Near) * Fraction;
		const float Log = InMain.Camera->Near * std::pow(Settings.Distance / InMain.Camera->Near, Fraction);
		Data[Index].Near = Near;
		Data[Index].Far = Uniform + (Log - Uniform) * Settings.SplitLambda;
		Near = Data[Index].Far;
		PrepareCascade(Index, InMain, InQuery);
		if (!IsFinite(Data[Index].Eye) || !std::isfinite(Data[Index].DepthRange) || Data[Index].DepthRange <= 0 ||
		    !std::isfinite(Data[Index].WorldTexel) || Data[Index].WorldTexel <= 0 ||
		    !std::all_of(Data[Index].ViewProjection.Values.begin(), Data[Index].ViewProjection.Values.end(),
		                 [](float InValue)
		                 {
			                 return std::isfinite(InValue);
		                 }))
		{
			Data = {};
			bEnabled = false;
			return false;
		}
		if (!Textures[Index] || Textures[Index]->GetDepthTarget()->Width != Settings.Resolution)
		{
			Textures[Index] = std::make_shared<const FMaterialTextureSource>(
			    FMaterialDepthTexture{Settings.Resolution, Settings.Resolution, 1});
		}
	}
	PreparedKey = Key;
	return true;
}

void FCascadedShadowMap::PrepareCascade(std::size_t InIndex, const FRenderView& InMain, const FBoundsQuery& InQuery)
{
	auto& Cascade = Data[InIndex];
	const float Near =
	    InIndex ? Cascade.Near - (Cascade.Near - Data[InIndex - 1].Near) * Settings.BlendFraction : Cascade.Near;
	const float Middle = (Near + Cascade.Far) * .5f;
	const float HalfHeight = Cascade.Far * std::tan(InMain.Camera->VerticalRadians * .5f);
	const float HalfWidth = HalfHeight * float(InMain.Width) / InMain.Height;
	const float HalfDepth = (Cascade.Far - Near) * .5f;
	// Rotation-independent receiver sphere with a fixed filter/snapping guard band.
	float Radius =
	    std::ceil(std::sqrt(HalfWidth * HalfWidth + HalfHeight * HalfHeight + HalfDepth * HalfDepth) * 16) / 16;
	Radius *= float(Settings.Resolution) / (Settings.Resolution - 8);
	const float Texel = 2 * Radius / Settings.Resolution;
	auto Center = Add(InMain.Eye, ScaleVector(Normalize(InMain.Camera->Forward), Middle));
	const float X = Dot(Center, LightRight);
	const float Y = Dot(Center, LightUp);
	Center = Add(Center, ScaleVector(LightRight, std::round(X / Texel) * Texel - X));
	Center = Add(Center, ScaleVector(LightUp, std::round(Y / Texel) * Texel - Y));
	FCasterVolume Volume;
	Volume.Right = LightRight;
	Volume.Up = LightUp;
	Volume.Light = Light;
	Volume.Center = Center;
	Volume.Radius = Radius;
	const auto Casters = InQuery(Volume);
	float Minimum = -Dot(Light, Center) - Radius;
	float Maximum = -Dot(Light, Center) + Radius;
	for (const auto& Bounds : Casters)
	{
		if (!IsUsable(Bounds))
		{
			Minimum = std::min(Minimum, -Dot(Light, Center) - 100000.f);
			Maximum = std::max(Maximum, -Dot(Light, Center) + 100000.f);
			continue;
		}
		for (unsigned Corner = 0; Corner < 8; ++Corner)
		{
			Minimum = std::min(Minimum, -Dot(Light, BoundsCorner(Bounds, Corner)));
		}
	}
	// Quantized outward depth bounds suppress sub-texel changes without clipping approaching casters.
	const float DepthStep = std::max(1.f, Radius / 8);
	Minimum = std::floor((Minimum - Texel * 4) / DepthStep) * DepthStep;
	Maximum = std::ceil((Maximum + Texel * 4) / DepthStep) * DepthStep;
	Cascade.ViewProjection = Projection(LightRight, LightUp, Light, Center, Radius, Minimum, Maximum);
	Cascade.Center = Center;
	// The light camera's snapped near-plane origin must not inherit the main camera's unsnapped eye.
	Cascade.Eye = Add(Add(ScaleVector(LightRight, std::round(X / Texel) * Texel),
	                      ScaleVector(LightUp, std::round(Y / Texel) * Texel)),
	                  ScaleVector(Light, -Minimum));
	Cascade.Radius = Radius;
	Cascade.WorldTexel = Texel;
	Cascade.DepthRange = Maximum - Minimum;
	Cascade.CandidateCasters = Casters.size();
}

std::vector<FRenderView> FCascadedShadowMap::Views(const FRenderView& InMain) const
{
	std::vector<FRenderView> Result;
	if (bEnabled)
	{
		for (std::size_t Index = 0; Index < Data.size(); ++Index)
		{
			FRenderView View;
			View.Identity = ViewIds[Index];
			View.ViewProjection = Data[Index].ViewProjection;
			View.Eye = Data[Index].Eye;
			View.Width = Settings.Resolution;
			View.Height = Settings.Resolution;
			View.Usage = "ShadowDepth";
			View.bSkipMissingPass = true;
			View.bInstanceBatching = InMain.bInstanceBatching;
			Result.push_back(std::move(View));
		}
	}
	return Result;
}

std::vector<FRenderPassTargets> FCascadedShadowMap::Targets(std::shared_ptr<const void> InLifetime) const
{
	std::vector<FRenderPassTargets> Result;
	if (bEnabled)
	{
		for (std::size_t Index = 0; Index < Textures.size(); ++Index)
		{
			FRenderPassTargets Pass;
			Pass.Name = "Shadow cascade " + std::to_string(Index);
			Pass.DepthStencil = FRenderDepthTarget{{ERenderTargetKind::Texture, Textures[Index], InLifetime, false},
			                                       ERHIDepthFormat::D32,
			                                       FAttachmentActions{EAttachmentLoad::Clear}};
			Result.push_back(std::move(Pass));
		}
	}
	return Result;
}

const std::array<FShadowCascade, 4>& FCascadedShadowMap::Cascades() const
{
	return Data;
}

std::uint64_t FCascadedShadowMap::TextureBytes() const
{
	return Textures[0]
	           ? std::uint64_t(Textures[0]->GetDepthTarget()->Width) * Textures[0]->GetDepthTarget()->Height * 16
	           : 0;
}

bool FCascadedShadowMap::IsEnabled() const
{
	return bEnabled;
}
} // namespace Hyperion
