#include "Hyperion/Math/Bounds.h"
#include "Hyperion/Renderer/MaterialPipeline.h"
#include "Hyperion/Renderer/RenderPass.h"
#include "Support/TestSupport.h"
#include <cmath>
#include <iostream>
#include <limits>

using namespace Hyperion;

namespace
{
void Rejects(const std::function<void()>& InAction)
{
	try
	{
		InAction();
	}
	catch (const std::invalid_argument&)
	{
		return;
	}
	throw std::runtime_error("Invalid depth contract accepted");
}

float ProjectDepth(const FMat4& InProjection, float InDistance)
{
	const auto Clip = Transform(InProjection, {0, 0, -InDistance, 1});
	return Clip.Z / Clip.W;
}

void CheckProjection()
{
	for (const auto Convention : {EDepthConvention::Standard, EDepthConvention::Reversed})
	{
		for (const auto Range : {FVec2{.1f, 1000}, FVec2{.0001f, 1000000}, FVec2{1, 2}})
		{
			const auto Projection = Perspective(1, 1.5f, Range.X, Range.Y, Convention);
			const float Clear = GetDepthClearValue(Convention);
			HYP_CHECK(std::abs(ProjectDepth(Projection, Range.X) - (1 - Clear)) < .00001f);
			HYP_CHECK(std::abs(ProjectDepth(Projection, Range.Y) - Clear) < .00001f);
			float Previous = ProjectDepth(Projection, Range.X);
			for (unsigned Index = 1; Index <= 100; ++Index)
			{
				const float Distance = Range.X * std::pow(Range.Y / Range.X, float(Index) / 100);
				const float Depth = ProjectDepth(Projection, Distance);
				HYP_CHECK(std::isfinite(Depth) && (Depth - Previous) * GetDepthDirection(Convention) >= 0);
				Previous = Depth;
			}
		}
		const auto Projection = Perspective(1, 1.5f, .1f, 100, Convention);
		const auto InverseProjection = Inverse(Projection);
		for (const float Distance : {.1f, 1.f, 25.f, 100.f})
		{
			const auto World = Transform(InverseProjection, {.1f, -.2f, ProjectDepth(Projection, Distance), 1});
			HYP_CHECK(std::abs(World.Z / World.W + Distance) < Distance * .001f);
		}
		const auto Targets = FRenderPassTargets::Frame(ERHIDepthFormat::D32, {}, Convention);
		HYP_CHECK(Targets.DepthStencil->ClearDepth == GetDepthClearValue(Convention));
		for (const float Depth : {0.f, .25f, .75f, 1.f})
		{
			const auto Clip = Transform(ClipDepthTransform(Convention), {0, 0, Depth * 2, 2});
			HYP_CHECK(Clip.Z / Clip.W == (Convention == EDepthConvention::Reversed ? 1 - Depth : Depth));
		}
	}
	Rejects(
	    []
	    {
		    Perspective(1, 1, 1, .1f, EDepthConvention::Reversed);
	    });
	Rejects(
	    []
	    {
		    Perspective(1, 1, 0, 100);
	    });
	Rejects(
	    []
	    {
		    Perspective(1, 1, .1f, std::numeric_limits<float>::infinity());
	    });
	Rejects(
	    []
	    {
		    Perspective(1, 1, .1f, 100, static_cast<EDepthConvention>(2));
	    });
}

void CheckClipping()
{
	const FFrustum Standard(Perspective(1, 1, .1f, 100));
	const FFrustum Reversed(Perspective(1, 1, .1f, 100, EDepthConvention::Reversed));
	for (const float Z : {-.05f, -.2f, -1.f, -50.f, -99.f, -101.f, 1.f})
	{
		for (const float X : {0.f, 1.f, 20.f, 100.f})
		{
			const FBounds Bounds{{X - .001f, -.001f, Z - .001f}, {X + .001f, .001f, Z + .001f}, true};
			HYP_CHECK(Standard.Intersects(Bounds) == Reversed.Intersects(Bounds));
			HYP_CHECK(Standard.Contains(Bounds) == Reversed.Contains(Bounds));
			if (X == 0)
			{
				HYP_CHECK(Reversed.Intersects(Bounds) == (Z < -.1f && Z > -100));
			}
		}
	}
}

void CheckMaterialState()
{
	constexpr std::array ReversedCompares{ERHICompare::Never,        ERHICompare::Greater, ERHICompare::Equal,
	                                      ERHICompare::GreaterEqual, ERHICompare::Less,    ERHICompare::NotEqual,
	                                      ERHICompare::LessEqual,    ERHICompare::Always};
	for (unsigned Index = 0; Index < ReversedCompares.size(); ++Index)
	{
		FMaterialState State;
		State.bDepthTest = true;
		State.bDepthWrite = true;
		State.bStencil = true;
		State.FrontStencil.Compare = EMaterialCompare::Less;
		State.DepthCompare = static_cast<EMaterialCompare>(Index);
		State.DepthBias = 3;
		State.SlopeScaledDepthBias = 1.25f;
		State.DepthBiasClamp = .01f;
		const auto Raw = ConvertMaterialState(State);
		HYP_CHECK(ConvertMaterialState(State, false, EDepthConvention::Reversed) == Raw);
		State.bViewRelativeDepth = true;
		HYP_CHECK(ConvertMaterialState(State) == Raw);
		const auto Reverse = ConvertMaterialState(State, false, EDepthConvention::Reversed);
		HYP_CHECK(Reverse.DepthCompare == ReversedCompares[Index]);
		HYP_CHECK(Reverse.DepthBias == -3 && Reverse.SlopeScaledDepthBias == -1.25f && Reverse.DepthBiasClamp == -.01f);
		HYP_CHECK(Reverse.FrontStencil == Raw.FrontStencil && Reverse.BackStencil == Raw.BackStencil);
		HYP_CHECK(Reverse.bDepthTest && Reverse.bDepthWrite);
	}
	FMaterialState Disabled;
	Disabled.bViewRelativeDepth = true;
	const auto State = ConvertMaterialState(Disabled, false, EDepthConvention::Reversed);
	HYP_CHECK(!State.bDepthTest && !State.bDepthWrite && State.DepthCompare == ERHICompare::Always);
	Disabled.DepthBias = std::numeric_limits<std::int32_t>::min();
	Rejects(
	    [&]
	    {
		    ConvertMaterialState(Disabled, false, EDepthConvention::Reversed);
	    });
}
} // namespace

int main()
{
	try
	{
		CheckProjection();
		CheckClipping();
		CheckMaterialState();
		std::cout << "Depth convention contracts passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
