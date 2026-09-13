#include "SceneViewerInternal.h"
#include <cmath>
#include <numbers>

namespace Hyperion
{
namespace
{
void Line(FGui& InGui, const FMat4& InClip, FVec3 InA, FVec3 InB, std::uint32_t InColor)
{
	const auto A = Transform(InClip, {InA.X, InA.Y, InA.Z, 1});
	const auto B = Transform(InClip, {InB.X, InB.Y, InB.Z, 1});
	if (A.W > .0001f && B.W > .0001f && A.Z >= 0 && B.Z >= 0 && IsFinite({A.X / A.W, A.Y / A.W, B.X / B.W}) &&
	    std::isfinite(B.Y / B.W))
	{
		InGui.OverlayLine({A.X / A.W * .5f + .5f, .5f - A.Y / A.W * .5f},
		                  {B.X / B.W * .5f + .5f, .5f - B.Y / B.W * .5f}, InColor);
	}
}

void Ring(FGui& InGui, const FMat4& InClip, FVec3 InCenter, FVec3 InRight, FVec3 InUp, float InRadius)
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
		Line(InGui, InClip, Point(Index), Point((Index + 1) % Segments), 0xFF66DDFF);
	}
}
} // namespace

void FSceneViewerPlugin::FImpl::DrawLightProperties(FGui& InGui, const FSceneNode& InNode)
{
	// The caller owns this node snapshot; scene edits replace and release the live node storage.
	if (InNode.PointLight)
	{
		auto Light = *InNode.PointLight;
		bool bChanged = InGui.InputVector("Point color", Light.Color);
		bChanged |= InGui.InputFloat("Point intensity", Light.Intensity);
		bChanged |= InGui.InputFloat("Point range (world units)", Light.Range);
		if (bChanged)
		{
			Scene.SetPointLight(Selected, Light);
		}
	}
	if (InNode.SpotLight)
	{
		auto Light = *InNode.SpotLight;
		bool bChanged = InGui.InputVector("Spot color", Light.Color);
		bChanged |= InGui.InputFloat("Spot intensity", Light.Intensity);
		bChanged |= InGui.InputFloat("Spot range (world units)", Light.Range);
		bChanged |= InGui.InputFloat("Inner half angle (radians)", Light.InnerRadians);
		bChanged |= InGui.InputFloat("Outer half angle (radians)", Light.OuterRadians);
		if (bChanged)
		{
			Scene.SetSpotLight(Selected, Light);
		}
	}
}

void FSceneViewerPlugin::FImpl::DrawLightBounds(FGui& InGui) const
{
	if (!LastView.Camera)
	{
		return;
	}
	for (const auto Handle : Scene.GetNodes())
	{
		FSceneNodeView View;
		if (!Scene.GetNodeView(Handle, View) || !View.bEffectiveEnabled)
		{
			continue;
		}
		if (View.Node->PointLight)
		{
			const auto Value = Transform(View.World, {0, 0, 0, 1});
			const FVec3 Position{Value.X, Value.Y, Value.Z};
			const float Radius = View.Node->PointLight->Range;
			Ring(InGui, LastView.ViewProjection, Position, {1, 0, 0}, {0, 1, 0}, Radius);
			Ring(InGui, LastView.ViewProjection, Position, {1, 0, 0}, {0, 0, 1}, Radius);
			Ring(InGui, LastView.ViewProjection, Position, {0, 1, 0}, {0, 0, 1}, Radius);
		}
		if (View.Node->SpotLight)
		{
			const auto Pose = ExtractScenePose(View.World);
			const auto& Light = *View.Node->SpotLight;
			for (const float Angle : {Light.InnerRadians, Light.OuterRadians})
			{
				const auto Center = Add(Pose.Eye, ScaleVector(Pose.Forward, Light.Range * std::cos(Angle)));
				const float Radius = Light.Range * std::sin(Angle);
				Ring(InGui, LastView.ViewProjection, Center, Pose.Right, Pose.Up, Radius);
				for (const auto Axis : {Pose.Right, Pose.Up, ScaleVector(Pose.Right, -1), ScaleVector(Pose.Up, -1)})
				{
					Line(InGui, LastView.ViewProjection, Pose.Eye, Add(Center, ScaleVector(Axis, Radius)), 0xFF66DDFF);
				}
			}
		}
	}
}
} // namespace Hyperion
