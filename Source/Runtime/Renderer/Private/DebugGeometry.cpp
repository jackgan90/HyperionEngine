#include "Hyperion/Renderer/DebugGeometry.h"
#include <cmath>
#include <numbers>

namespace Hyperion
{
namespace
{
void Ring(std::vector<FDebugLine>& InLines, FVec3 InCenter, FVec3 InRight, FVec3 InUp, float InRadius)
{
	constexpr unsigned Segments = 64;
	const auto Point = [&](unsigned InIndex)
	{
		const float Angle = InIndex * (2 * std::numbers::pi_v<float> / Segments);
		return Add(InCenter, ScaleVector(Add(ScaleVector(InRight, std::cos(Angle)), ScaleVector(InUp, std::sin(Angle))),
		                                 InRadius));
	};
	for (unsigned Index = 0; Index < Segments; ++Index)
	{
		InLines.push_back({Point(Index), Point(Index + 1)});
	}
}

void LightLines(std::vector<FDebugLine>& InLines, const FSceneNodeView& InView)
{
	if (const auto& Light = InView.Node->PointLight())
	{
		const auto Origin = Transform(InView.World, {0, 0, 0, 1});
		const FVec3 Center{Origin.X, Origin.Y, Origin.Z};
		Ring(InLines, Center, {1, 0, 0}, {0, 1, 0}, Light->Range);
		Ring(InLines, Center, {1, 0, 0}, {0, 0, 1}, Light->Range);
		Ring(InLines, Center, {0, 1, 0}, {0, 0, 1}, Light->Range);
	}
	if (const auto& Light = InView.Node->SpotLight())
	{
		const auto Pose = ExtractScenePose(InView.World);
		for (const float Angle : {Light->InnerRadians, Light->OuterRadians})
		{
			const auto Center = Add(Pose.Eye, ScaleVector(Pose.Forward, Light->Range * std::cos(Angle)));
			const float Radius = Light->Range * std::sin(Angle);
			Ring(InLines, Center, Pose.Right, Pose.Up, Radius);
			for (const auto Axis : {Pose.Right, Pose.Up, ScaleVector(Pose.Right, -1), ScaleVector(Pose.Up, -1)})
			{
				InLines.push_back({Pose.Eye, Add(Center, ScaleVector(Axis, Radius))});
			}
		}
	}
}

void ModelLines(std::vector<FDebugLine>& InLines, const FSceneNodeView& InView)
{
	const auto& Model = InView.Node->Model();
	if (!Model || !Model->bVisible || !Model->Data || !IsUsable(Model->Data->Bounds))
	{
		return;
	}
	std::array<FVec3, 8> Corners;
	for (unsigned Index = 0; Index < 8; ++Index)
	{
		const auto Point = BoundsCorner(Model->Data->Bounds, Index);
		const auto World = Transform(InView.World, {Point.X, Point.Y, Point.Z, 1});
		Corners[Index] = {World.X, World.Y, World.Z};
	}
	for (unsigned Index = 0; Index < 8; ++Index)
	{
		for (unsigned Axis = 1; Axis <= 4; Axis *= 2)
		{
			if (!(Index & Axis))
			{
				InLines.push_back({Corners[Index], Corners[Index | Axis], {.4f, .8f, 1, 1}});
			}
		}
	}
}
} // namespace

std::vector<FDebugLine> BuildSceneDebugLines(std::span<const FSceneNodeView> InNodes, bool bInModels, bool bInLights)
{
	std::vector<FDebugLine> Result;
	for (const auto& View : InNodes)
	{
		if (!View.bEffectiveEnabled)
		{
			continue;
		}
		if (bInModels)
		{
			ModelLines(Result, View);
		}
		if (bInLights && (View.Node->PointLight() || View.Node->SpotLight()))
		{
			LightLines(Result, View);
		}
	}
	return Result;
}

std::optional<std::array<FVec2, 2>> ProjectDebugLine(const FDebugLine& InLine, const FMat4& InViewProjection,
                                                     FVec4 InViewport)
{
	const auto A = Transform(InViewProjection, {InLine.A.X, InLine.A.Y, InLine.A.Z, 1});
	const auto B = Transform(InViewProjection, {InLine.B.X, InLine.B.Y, InLine.B.Z, 1});
	const std::array Start{A.X + A.W, A.W - A.X, A.Y + A.W, A.W - A.Y, A.Z, A.W - A.Z};
	const std::array End{B.X + B.W, B.W - B.X, B.Y + B.W, B.W - B.Y, B.Z, B.W - B.Z};
	float Minimum = 0;
	float Maximum = 1;
	for (std::size_t Index = 0; Index < Start.size(); ++Index)
	{
		if (!std::isfinite(Start[Index]) || !std::isfinite(End[Index]) || (Start[Index] < 0 && End[Index] < 0))
		{
			return {};
		}
		if (Start[Index] < 0)
		{
			Minimum = std::max(Minimum, Start[Index] / (Start[Index] - End[Index]));
		}
		else if (End[Index] < 0)
		{
			Maximum = std::min(Maximum, Start[Index] / (Start[Index] - End[Index]));
		}
	}
	if (Minimum > Maximum)
	{
		return {};
	}
	std::array<FVec2, 2> Result;
	for (unsigned Index = 0; Index < 2; ++Index)
	{
		const float T = Index ? Maximum : Minimum;
		const float W = A.W + (B.W - A.W) * T;
		if (W <= .000001f)
		{
			return {};
		}
		Result[Index] = {InViewport.X + ((A.X + (B.X - A.X) * T) / W * .5f + .5f) * (InViewport.Z - InViewport.X),
		                 InViewport.Y + (.5f - (A.Y + (B.Y - A.Y) * T) / W * .5f) * (InViewport.W - InViewport.Y)};
	}
	return Result;
}
} // namespace Hyperion
