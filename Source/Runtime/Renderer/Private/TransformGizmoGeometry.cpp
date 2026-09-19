#include "Hyperion/Renderer/TransformGizmo.h"
#include <cmath>
#include <numbers>

namespace Hyperion
{
namespace
{
constexpr std::array<FVec4, 3> Colors{{{.95f, .15f, .12f, 1}, {.28f, .85f, .15f, 1}, {.15f, .48f, 1, 1}}};
constexpr float Pi = std::numbers::pi_v<float>;
} // namespace

void FTransformGizmo::AddAxes(std::vector<FTransformGizmoStroke>& OutStrokes) const
{
	for (unsigned Axis = 0; Axis < 3; ++Axis)
	{
		const auto Handle = static_cast<ETransformGizmoHandle>(Axis + 1);
		const FVec2 Line{Tips[Axis].X - Center.X, Tips[Axis].Y - Center.Y};
		const float Size = std::hypot(Line.X, Line.Y);
		// An axis aimed directly at the camera has no defined screen drag direction.
		if (Size < 8 * UiScale)
		{
			continue;
		}
		const FVec2 Direction{Line.X / Size, Line.Y / Size};
		const FVec2 Base{Tips[Axis].X - Direction.X * 14 * UiScale, Tips[Axis].Y - Direction.Y * 14 * UiScale};
		OutStrokes.push_back(
		    {{{Center.X + Direction.X * 12 * UiScale, Center.Y + Direction.Y * 12 * UiScale}, Tips[Axis]},
		     Colors[Axis],
		     3 * UiScale,
		     false,
		     Handle});
		if (Mode == ETransformGizmoMode::Position)
		{
			OutStrokes.push_back({{Tips[Axis],
			                       {Base.X - Direction.Y * 6 * UiScale, Base.Y + Direction.X * 6 * UiScale},
			                       {Base.X + Direction.Y * 6 * UiScale, Base.Y - Direction.X * 6 * UiScale}},
			                      Colors[Axis],
			                      1,
			                      true,
			                      Handle});
		}
		else
		{
			// Project a small cube whose orientation remains independent of signed object scale.
			std::array<FVec2, 8> Corners;
			bool bProjected = true;
			for (unsigned Corner = 0; Corner < 8; ++Corner)
			{
				auto Point = Add(Pivot, ScaleVector(Axes[Axis], Radius));
				for (unsigned Dimension = 0; Dimension < 3; ++Dimension)
				{
					Point = Add(Point,
					            ScaleVector(Axes[Dimension], Radius * .065f * ((Corner & (1u << Dimension)) ? 1 : -1)));
				}
				bProjected &= Project(Point, Corners[Corner]);
			}
			if (bProjected)
			{
				constexpr std::array<std::array<unsigned, 4>, 6> Faces{
				    {{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1}, {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 5, 7, 3}}};
				for (const auto& Face : Faces)
				{
					OutStrokes.push_back({{Corners[Face[0]], Corners[Face[1]], Corners[Face[2]], Corners[Face[3]]},
					                      Colors[Axis],
					                      1,
					                      true,
					                      Handle});
				}
			}
		}
	}
	const float Half = 6 * UiScale;
	OutStrokes.push_back({{{Center.X - Half, Center.Y - Half},
	                       {Center.X + Half, Center.Y - Half},
	                       {Center.X + Half, Center.Y + Half},
	                       {Center.X - Half, Center.Y + Half},
	                       {Center.X - Half, Center.Y - Half}},
	                      {.95f, .95f, .95f, 1},
	                      2 * UiScale,
	                      Mode == ETransformGizmoMode::Scale,
	                      ETransformGizmoHandle::Screen});
}

void FTransformGizmo::AddRings(std::vector<FTransformGizmoStroke>& OutStrokes) const
{
	// Back halves remain visible but subdued; the gizmo is an always-visible scene overlay.
	for (unsigned Axis = 0; Axis < 3; ++Axis)
	{
		if (Length(Cross(Axes[(Axis + 1) % 3], Axes[(Axis + 2) % 3])) < .01f)
		{
			continue;
		}
		for (unsigned Segment = 0; Segment < 96; ++Segment)
		{
			const auto First = RingPoint(Axis, Segment * 2 * Pi / 96);
			const auto Second = RingPoint(Axis, (Segment + 1) * 2 * Pi / 96);
			FVec2 A;
			FVec2 B;
			if (Project(First, A) && Project(Second, B))
			{
				auto Color = Colors[Axis];
				Color.W = Dot(Subtract(First, Pivot), Subtract(Camera.Eye, Pivot)) < 0 ? .35f : 1;
				OutStrokes.push_back(
				    {{A, B}, Color, 2.5f * UiScale, false, static_cast<ETransformGizmoHandle>(Axis + 1)});
			}
		}
	}
	FTransformGizmoStroke Outline;
	Outline.Color = {.85f, .85f, .85f, .65f};
	Outline.Thickness = UiScale;
	for (unsigned Segment = 0; Segment <= 96; ++Segment)
	{
		const float OutlineAngle = Segment * 2 * Pi / 96;
		Outline.Points.push_back(
		    {Center.X + std::cos(OutlineAngle) * 98 * UiScale, Center.Y + std::sin(OutlineAngle) * 98 * UiScale});
	}
	OutStrokes.push_back(std::move(Outline));
}

std::vector<FTransformGizmoStroke> FTransformGizmo::Geometry(ETransformGizmoHandle InHovered) const
{
	std::vector<FTransformGizmoStroke> Result;
	if (!bValid)
	{
		return Result;
	}
	if (Mode == ETransformGizmoMode::Rotation)
	{
		AddRings(Result);
	}
	else
	{
		AddAxes(Result);
	}
	for (auto& Stroke : Result)
	{
		if (Stroke.Handle != ETransformGizmoHandle::None && Stroke.Handle == (IsDragging() ? Active : InHovered))
		{
			Stroke.Color = {1, .8f, .1f, 1};
		}
		if (Mode == ETransformGizmoMode::Position && !bParentInvertible)
		{
			Stroke.Color = {.5f, .5f, .5f, .5f};
		}
	}
	return Result;
}
} // namespace Hyperion
