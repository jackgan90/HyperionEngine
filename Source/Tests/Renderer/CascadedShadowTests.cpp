#include "Hyperion/Renderer/CascadedShadowMap.h"
#include "Support/TestSupport.h"
#include <cmath>
#include <iostream>
#include <limits>

using namespace Hyperion;

namespace
{
FRenderView Camera()
{
	FRenderView View;
	View.Eye = {0, 2, 10};
	View.Width = 1280;
	View.Height = 720;
	View.Camera = FRenderCamera{};
	View.ViewProjection = Multiply(Perspective(1, 1280.f / 720, .05f, 500), LookAt(View.Eye, {0, 2, 0}));
	return View;
}

FVec3 Row(const FMat4& InMatrix, unsigned InIndex)
{
	return {InMatrix.Values[InIndex], InMatrix.Values[InIndex + 4], InMatrix.Values[InIndex + 8]};
}

void CheckReceiverCoverage(const FCascadedShadowMap& InShadows, const FRenderView& InView)
{
	const auto Forward = Normalize(InView.Camera->Forward);
	const auto Right = Normalize(Cross(Forward, InView.Camera->Up));
	const auto Up = Cross(Right, Forward);
	float PreviousNear = InView.Camera->Near;
	for (const auto& Cascade : InShadows.Cascades())
	{
		HYP_CHECK(Cascade.Far > Cascade.Near && Cascade.DepthRange > 0);
		for (const auto Distance : {Cascade.Near - (Cascade.Near - PreviousNear) * .1f, Cascade.Far})
		{
			for (const float X : {-1.f, 1.f})
			{
				for (const float Y : {-1.f, 1.f})
				{
					const float Height = Distance * std::tan(InView.Camera->VerticalRadians * .5f);
					const auto Position =
					    Add(InView.Eye, Add(ScaleVector(Forward, Distance),
					                        Add(ScaleVector(Right, X * Height * float(InView.Width) / InView.Height),
					                            ScaleVector(Up, Y * Height))));
					const auto Clip = Transform(Cascade.ViewProjection, {Position.X, Position.Y, Position.Z, 1});
					HYP_CHECK(std::abs(Clip.X) <= 1 && std::abs(Clip.Y) <= 1 && Clip.Z > 0 && Clip.Z < 1);
				}
			}
		}
		PreviousNear = Cascade.Near;
		for (const auto Value : Cascade.ViewProjection.Values)
		{
			HYP_CHECK(std::isfinite(Value));
		}
	}
}

void CheckDirectionsAndStability()
{
	FCascadedShadowMap Shadows;
	const auto Query = [](const ISceneVisibility&)
	{
		return std::vector<FBounds>{};
	};
	const std::array<FVec3, 8> Lights{
	    {{0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, .00001f}, {1, .00001f, 0}}};
	for (const auto Light : Lights)
	{
		HYP_CHECK(Shadows.Prepare(Camera(), Light, {}, Query));
		CheckReceiverCoverage(Shadows, Camera());
	}
	auto View = Camera();
	HYP_CHECK(Shadows.Prepare(View, {.4f, .8f, .3f}, {}, Query));
	const auto Before = Shadows.Cascades();
	const auto InitialViews = Shadows.Views(View);
	const auto InitialTargets = Shadows.Targets(std::make_shared<int>(0));
	HYP_CHECK(InitialViews.size() == 4 && Shadows.TextureBytes() == 64 * 1024 * 1024);
	View.Eye = Add(View.Eye, ScaleVector(Normalize(Row(Before[0].ViewProjection, 0)), Before[0].WorldTexel * .0001f));
	HYP_CHECK(Shadows.Prepare(View, {.4f, .8f, .3f}, {}, Query));
	HYP_CHECK(std::abs(Shadows.Cascades()[0].ViewProjection.Values[12] - Before[0].ViewProjection.Values[12]) < 1e-6f);
	const auto LaterViews = Shadows.Views(View);
	const auto LaterTargets = Shadows.Targets(std::make_shared<int>(0));
	for (unsigned Index = 0; Index < 4; ++Index)
	{
		HYP_CHECK(InitialViews[Index].Identity == LaterViews[Index].Identity);
		HYP_CHECK(InitialTargets[Index].DepthStencil->Source.Texture ==
		          LaterTargets[Index].DepthStencil->Source.Texture);
		HYP_CHECK(LaterViews[Index].Usage == "ShadowDepth" && LaterViews[Index].bSkipMissingPass);
	}
	for (int Step = -20; Step <= 20; ++Step)
	{
		const auto Previous = Normalize(Row(Shadows.Cascades()[0].ViewProjection, 0));
		HYP_CHECK(Shadows.Prepare(View, {float(Step) * .00001f, 1, .00001f}, {}, Query));
		if (Step > -20)
		{
			HYP_CHECK(Dot(Previous, Normalize(Row(Shadows.Cascades()[0].ViewProjection, 0))) > .999f);
		}
	}
}

void CheckOffscreenCasters()
{
	FCascadedShadowMap Shadows;
	const FBounds HighCaster{{-1, 1000, 0}, {1, 1002, 2}, true};
	HYP_CHECK(!FFrustum(Camera().ViewProjection).Intersects(HighCaster));
	unsigned Hits{};
	const auto Query = [&](const ISceneVisibility& InVolume)
	{
		std::vector<FBounds> Result;
		if (InVolume.Intersects(HighCaster))
		{
			++Hits;
			Result.push_back(HighCaster);
		}
		HYP_CHECK(InVolume.Intersects({})); // Unknown bounds always fail open.
		return Result;
	};
	HYP_CHECK(Shadows.Prepare(Camera(), {0, 1, 0}, {}, Query));
	HYP_CHECK(Hits > 0);
	const auto& Cascade = Shadows.Cascades()[0];
	HYP_CHECK(Cascade.CandidateCasters > 0 && Cascade.DepthRange > 1000);
	const auto Clip = Transform(Cascade.ViewProjection, {0, 1001, 1, 1});
	HYP_CHECK(Clip.Z > 0 && Clip.Z < 1);
	CheckReceiverCoverage(Shadows, Camera());
}

void CheckDisableAndInvalid()
{
	FCascadedShadowMap Shadows;
	const auto Query = [](const ISceneVisibility&)
	{
		return std::vector<FBounds>{};
	};
	FCascadedShadowSettings Settings;
	Settings.Resolution = 1024;
	HYP_CHECK(Shadows.Prepare(Camera(), {0, 1, 0}, Settings, Query));
	HYP_CHECK(Shadows.TextureBytes() == 16 * 1024 * 1024);
	Settings.bEnabled = false;
	HYP_CHECK(!Shadows.Prepare(Camera(), {0, 1, 0}, Settings, Query));
	HYP_CHECK(Shadows.Views(Camera()).empty());
	HYP_CHECK(!Shadows.Prepare(Camera(), {}, {}, Query));
	HYP_CHECK(!Shadows.Prepare(Camera(), {0, std::numeric_limits<float>::quiet_NaN(), 0}, {}, Query));
	auto View = Camera();
	View.Camera->Far = View.Camera->Near;
	HYP_CHECK(!Shadows.Prepare(View, {0, 1, 0}, {}, Query));
	View = Camera();
	View.Camera->Forward = View.Camera->Up;
	HYP_CHECK(!Shadows.Prepare(View, {0, 1, 0}, {}, Query));
	Settings = {};
	Settings.NormalOffset = std::numeric_limits<float>::infinity();
	HYP_CHECK(!Shadows.Prepare(Camera(), {0, 1, 0}, Settings, Query));
	Settings = {};
	Settings.Distance = std::numeric_limits<float>::max();
	View = Camera();
	View.Camera->Far = Settings.Distance;
	HYP_CHECK(!Shadows.Prepare(View, {0, 1, 0}, Settings, Query));
	for (const auto& Cascade : Shadows.Cascades())
	{
		for (const auto Value : Cascade.ViewProjection.Values)
		{
			HYP_CHECK(std::isfinite(Value));
		}
	}
}

void CheckRetainedSetup()
{
	FCascadedShadowMap Shadows;
	auto View = Camera();
	FCascadedShadowSettings Settings;
	FVec3 Light{.4f, .8f, .3f};
	std::array<std::uint64_t, 2> State{1, 1};
	unsigned Queries{};
	const auto Query = [&](const ISceneVisibility&)
	{
		++Queries;
		return std::vector<FBounds>{};
	};
	const auto Prepare = [&]
	{
		HYP_CHECK(Shadows.Prepare(View, Light, Settings, Query, State));
	};
	Prepare();
	HYP_CHECK(Queries == 4);
	const auto Before = Shadows.Cascades();
	Prepare();
	HYP_CHECK(Queries == 4 && Before[0].ViewProjection.Values == Shadows.Cascades()[0].ViewProjection.Values);
	++State[0];
	Prepare();
	HYP_CHECK(Queries == 8);
	++State[1];
	Prepare();
	HYP_CHECK(Queries == 12);
	View.Eye.X = 1;
	Prepare();
	HYP_CHECK(Queries == 16);
	View.Camera->Far = 200;
	Prepare();
	HYP_CHECK(Queries == 20);
	Light.X = -.4f;
	Prepare();
	HYP_CHECK(Queries == 24);
	Settings.Resolution = 1024;
	Prepare();
	HYP_CHECK(Queries == 28 && Shadows.TextureBytes() == 16 * 1024 * 1024);
	Settings.bEnabled = false;
	HYP_CHECK(!Shadows.Prepare(View, Light, Settings, Query, State));
	Settings.bEnabled = true;
	Prepare();
	HYP_CHECK(Queries == 32);
	HYP_CHECK(Shadows.Prepare(View, Light, Settings, Query));
	HYP_CHECK(Shadows.Prepare(View, Light, Settings, Query));
	HYP_CHECK(Queries == 40); // Custom/dynamic query clients without a stable scene token always execute.
}

void CheckDepthConventions()
{
	FCascadedShadowMap Shadows;
	auto View = Camera();
	unsigned Queries{};
	const auto Query = [&](const ISceneVisibility&)
	{
		++Queries;
		return std::vector<FBounds>{};
	};
	const std::array<std::uint64_t, 2> SceneState{1, 1};
	HYP_CHECK(Shadows.Prepare(View, {.4f, .8f, .3f}, {}, Query, SceneState));
	const auto Standard = Shadows.Cascades();
	const auto StandardTargets = Shadows.Targets({});
	HYP_CHECK(Queries == 4);
	View.DepthConvention = EDepthConvention::Reversed;
	View.ViewProjection =
	    Multiply(Perspective(1, 1280.f / 720, .05f, 500, View.DepthConvention), LookAt(View.Eye, {0, 2, 0}));
	HYP_CHECK(Shadows.Prepare(View, {.4f, .8f, .3f}, {}, Query, SceneState));
	HYP_CHECK(Queries == 8);
	CheckReceiverCoverage(Shadows, View);
	const auto ReversedTargets = Shadows.Targets({});
	const auto Views = Shadows.Views(View);
	for (unsigned Index = 0; Index < 4; ++Index)
	{
		HYP_CHECK(Views[Index].DepthConvention == EDepthConvention::Reversed);
		HYP_CHECK(Standard[Index].Near == Shadows.Cascades()[Index].Near);
		HYP_CHECK(Standard[Index].Far == Shadows.Cascades()[Index].Far);
		const auto& Target = *ReversedTargets[Index].DepthStencil;
		HYP_CHECK(Target.ClearDepth == 0 && Target.Source.Texture->GetDepthTarget()->ClearDepth == 0);
		HYP_CHECK(Target.Source.Texture != StandardTargets[Index].DepthStencil->Source.Texture);
		const auto Point = Shadows.Cascades()[Index].Center;
		const auto A = Transform(Standard[Index].ViewProjection, {Point.X, Point.Y, Point.Z, 1});
		const auto B = Transform(Views[Index].ViewProjection, {Point.X, Point.Y, Point.Z, 1});
		HYP_CHECK(std::abs(A.Z + B.Z - 1) < .00001f && A.X == B.X && A.Y == B.Y);
	}
	HYP_CHECK(Shadows.Prepare(View, {.4f, .8f, .3f}, {}, Query, SceneState));
	HYP_CHECK(Queries == 8);
	for (const auto& Parameter : DefaultShadowParameters(EDepthConvention::Reversed))
	{
		if (Parameter.Name == "Engine.View.ShadowSampler")
		{
			HYP_CHECK(Parameter.Value.Sampler.Compare == EMaterialSamplerCompare::GreaterEqual);
		}
		if (Parameter.Value.Texture)
		{
			HYP_CHECK(Parameter.Value.Texture->GetDepthTarget()->ClearDepth == 0);
		}
	}
}
} // namespace

int main()
{
	try
	{
		CheckDirectionsAndStability();
		CheckOffscreenCasters();
		CheckDisableAndInvalid();
		CheckRetainedSetup();
		CheckDepthConventions();
		std::cout << "Cascaded shadow projection and caster queries passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
