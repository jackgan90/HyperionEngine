#include "Hyperion/Renderer/TransformGizmo.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace Hyperion
{
namespace
{
constexpr float Pi = std::numbers::pi_v<float>;
constexpr std::array<FVec3, 3> UnitAxes{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};

FVec3 Direction(const FMat4& InMatrix, FVec3 InDirection)
{
	const auto Value = Transform(InMatrix, {InDirection.X, InDirection.Y, InDirection.Z, 0});
	return {Value.X, Value.Y, Value.Z};
}

float DistanceSquared(FVec2 InA, FVec2 InB)
{
	return (InA.X - InB.X) * (InA.X - InB.X) + (InA.Y - InB.Y) * (InA.Y - InB.Y);
}

float SegmentDistance(FVec2 InPoint, FVec2 InA, FVec2 InB)
{
	const float LengthSquared = DistanceSquared(InA, InB);
	const float T = LengthSquared > 1e-6f
	                    ? std::clamp(((InPoint.X - InA.X) * (InB.X - InA.X) + (InPoint.Y - InA.Y) * (InB.Y - InA.Y)) /
	                                     LengthSquared,
	                                 0.f, 1.f)
	                    : 0;
	return DistanceSquared(InPoint, {InA.X + T * (InB.X - InA.X), InA.Y + T * (InB.Y - InA.Y)});
}
} // namespace

bool FTransformGizmo::Configure(const FSceneCameraView& InCamera, FVec4 InViewport, const FMat4& InLocal,
                                const FMat4& InParent, ETransformGizmoMode InMode, float InUiScale)
{
	if (IsDragging())
	{
		return bValid;
	}
	bValid = false;
	if (!IsAffine(InLocal) || !IsAffine(InParent) || !TryExtractScenePose(InCamera.World, Camera) ||
	    InViewport.Z - InViewport.X < 2 || InViewport.W - InViewport.Y < 2 || !std::isfinite(InUiScale) ||
	    InUiScale <= 0)
	{
		return false;
	}
	Initial = InLocal;
	try
	{
		Parts = DecomposeAffine(InLocal);
		Rotation = ComposeAffine({{}, Parts.Rotation});
	}
	catch (const std::exception&)
	{
		return false;
	}
	Mode = InMode;
	Viewport = InViewport;
	UiScale = InUiScale;
	Focal = (Viewport.W - Viewport.Y) / (2 * std::tan(InCamera.Lens.VerticalRadians / 2));
	const auto World = Multiply(InParent, InLocal);
	Pivot = {World.Values[12], World.Values[13], World.Values[14]};
	const float Depth = Dot(Subtract(Pivot, Camera.Eye), Camera.Forward);
	if (!std::isfinite(Depth) || Depth <= InCamera.Lens.Near || Depth >= InCamera.Lens.Far || !std::isfinite(Focal) ||
	    Focal <= 0 || !Project(Pivot, Center))
	{
		return false;
	}
	Radius = 85 * UiScale * Depth / Focal;
	bParentInvertible = false;
	try
	{
		ParentInverse = Inverse(InParent);
		bParentInvertible = IsAffine(ParentInverse);
	}
	catch (const std::exception&)
	{
		// A collapsed parent has no unique world-to-local position mapping.
		ParentInverse = Identity();
	}
	const auto Basis = Multiply(InParent, Rotation);
	if (!IsAffine(Basis))
	{
		return false;
	}
	for (unsigned Index = 0; Index < 3; ++Index)
	{
		const auto Axis = Mode == ETransformGizmoMode::Position ? UnitAxes[Index] : Direction(Basis, UnitAxes[Index]);
		const float AxisLength = std::hypot(Axis.X, Axis.Y, Axis.Z);
		Axes[Index] = AxisLength > 1e-7f ? ScaleVector(Axis, 1 / AxisLength) : FVec3{};
		Tips[Index] = Center;
		Project(Add(Pivot, ScaleVector(Axes[Index], Radius)), Tips[Index]);
	}
	bValid = std::isfinite(Radius) && Radius > 0;
	return bValid;
}

bool FTransformGizmo::Project(FVec3 InPoint, FVec2& OutPoint) const
{
	const auto Offset = Subtract(InPoint, Camera.Eye);
	const float Depth = Dot(Offset, Camera.Forward);
	if (Depth <= 1e-6f)
	{
		return false;
	}
	OutPoint = {(Viewport.X + Viewport.Z) / 2 + Focal * Dot(Offset, Camera.Right) / Depth,
	            (Viewport.Y + Viewport.W) / 2 - Focal * Dot(Offset, Camera.Up) / Depth};
	return std::isfinite(OutPoint.X) && std::isfinite(OutPoint.Y);
}

FVec3 FTransformGizmo::RingPoint(unsigned InAxis, float InAngle) const
{
	return Add(Pivot, ScaleVector(Add(ScaleVector(Axes[(InAxis + 1) % 3], std::cos(InAngle)),
	                                  ScaleVector(Axes[(InAxis + 2) % 3], std::sin(InAngle))),
	                              Radius));
}

float FTransformGizmo::RingAngle(FVec2 InPointer, unsigned InAxis) const
{
	const auto Ray =
	    Add(Camera.Forward, Add(ScaleVector(Camera.Right, (InPointer.X - (Viewport.X + Viewport.Z) / 2) / Focal),
	                            ScaleVector(Camera.Up, -(InPointer.Y - (Viewport.Y + Viewport.W) / 2) / Focal)));
	const auto U = Axes[(InAxis + 1) % 3];
	const auto V = Axes[(InAxis + 2) % 3];
	const auto Normal = Cross(U, V);
	const float Denominator = Dot(Normal, Ray);
	const float Correlation = Dot(U, V);
	const float Gram = 1 - Correlation * Correlation;
	if (std::abs(Denominator) > 1e-5f && Gram > 1e-5f)
	{
		const float Distance = Dot(Normal, Subtract(Pivot, Camera.Eye)) / Denominator;
		const auto Offset = Subtract(Add(Camera.Eye, ScaleVector(Ray, Distance)), Pivot);
		const float X = (Dot(Offset, U) - Correlation * Dot(Offset, V)) / Gram;
		const float Y = (Dot(Offset, V) - Correlation * Dot(Offset, U)) / Gram;
		if (Distance > 0 && X * X + Y * Y > 1e-10f)
		{
			return std::atan2(Y, X);
		}
	}
	// The ring becomes a line at grazing angles. Find the initial visible screen tangent.
	float Best = std::numeric_limits<float>::max();
	float Result{};
	for (unsigned Index = 0; Index < 720; ++Index)
	{
		const float Candidate = Index * (2 * Pi / 720);
		FVec2 Point;
		if (Project(RingPoint(InAxis, Candidate), Point))
		{
			const float Distance = DistanceSquared(InPointer, Point);
			if (Distance < Best)
			{
				Best = Distance;
				Result = Candidate;
			}
		}
	}
	return Result;
}

ETransformGizmoHandle FTransformGizmo::HitTest(FVec2 InPointer) const
{
	if (!bValid || InPointer.X < Viewport.X || InPointer.X > Viewport.Z || InPointer.Y < Viewport.Y ||
	    InPointer.Y > Viewport.W || (Mode == ETransformGizmoMode::Position && !bParentInvertible))
	{
		return ETransformGizmoHandle::None;
	}
	if (Mode != ETransformGizmoMode::Rotation && DistanceSquared(InPointer, Center) < 100 * UiScale * UiScale)
	{
		return ETransformGizmoHandle::Screen;
	}
	float Best = 64 * UiScale * UiScale;
	ETransformGizmoHandle Result{};
	for (const auto& Stroke : Geometry())
	{
		for (std::size_t Index = 1; Index < Stroke.Points.size(); ++Index)
		{
			const float Distance = SegmentDistance(InPointer, Stroke.Points[Index - 1], Stroke.Points[Index]);
			if (Distance < Best)
			{
				Best = Distance;
				Result = Stroke.Handle;
			}
		}
	}
	return Result;
}

bool FTransformGizmo::Begin(FVec2 InPointer)
{
	Active = HitTest(InPointer);
	Start = InPointer;
	Angle = 0;
	if (Mode == ETransformGizmoMode::Rotation && IsDragging())
	{
		PreviousAngle = RingAngle(InPointer, static_cast<unsigned>(Active) - 1);
		const unsigned Axis = static_cast<unsigned>(Active) - 1;
		const auto Normal = Cross(Axes[(Axis + 1) % 3], Axes[(Axis + 2) % 3]);
		bLinearRotation = std::abs(Dot(Normal, Normalize(Subtract(Camera.Eye, Pivot)))) < .15f;
		FVec2 Before;
		FVec2 After;
		Project(RingPoint(Axis, PreviousAngle - .01f), Before);
		Project(RingPoint(Axis, PreviousAngle + .01f), After);
		RotationTangent = {(After.X - Before.X) / .02f, (After.Y - Before.Y) / .02f};
		if (bLinearRotation && DistanceSquared(RotationTangent, {}) < 64 * UiScale * UiScale)
		{
			End();
		}
	}
	return IsDragging();
}

bool FTransformGizmo::Drag(FVec2 InPointer, FMat4& OutLocal)
{
	if (!IsDragging() || !std::isfinite(InPointer.X) || !std::isfinite(InPointer.Y))
	{
		return false;
	}
	const unsigned Axis = Active == ETransformGizmoHandle::Screen ? 0 : static_cast<unsigned>(Active) - 1;
	const FVec2 Delta{InPointer.X - Start.X, InPointer.Y - Start.Y};
	const FVec2 Line{Tips[Axis].X - Center.X, Tips[Axis].Y - Center.Y};
	const float Denominator = Line.X * Line.X + Line.Y * Line.Y;
	const float Amount = Active == ETransformGizmoHandle::Screen ? (Delta.X - Delta.Y) / (85 * UiScale)
	                     : Denominator > 64 * UiScale * UiScale  ? (Delta.X * Line.X + Delta.Y * Line.Y) / Denominator
	                                                             : 0;
	try
	{
		auto Edited = Parts;
		if (Mode == ETransformGizmoMode::Position)
		{
			const auto Offset =
			    Active == ETransformGizmoHandle::Screen
			        ? ScaleVector(Add(ScaleVector(Camera.Right, Delta.X), ScaleVector(Camera.Up, -Delta.Y)),
			                      Radius / (85 * UiScale))
			        : ScaleVector(Axes[Axis], Amount * Radius);
			OutLocal = Initial;
			const auto Position = Add(Parts.Position, Direction(ParentInverse, Offset));
			OutLocal.Values[12] = Position.X;
			OutLocal.Values[13] = Position.Y;
			OutLocal.Values[14] = Position.Z;
		}
		else if (Mode == ETransformGizmoMode::Scale)
		{
			if (Active == ETransformGizmoHandle::Screen)
			{
				// Shear stores raw stretch-matrix entries, so uniform scaling must include them.
				Edited.Shear = ScaleVector(Parts.Shear, 1 + Amount);
			}
			std::array<float*, 3> Values{&Edited.Scale.X, &Edited.Scale.Y, &Edited.Scale.Z};
			for (unsigned Index = 0; Index < 3; ++Index)
			{
				if (Active == ETransformGizmoHandle::Screen || Index == Axis)
				{
					// Snapshot-relative signed factor crosses zero; a zero axis restarts from unit sensitivity.
					*Values[Index] += (*Values[Index] == 0 ? 1 : *Values[Index]) * Amount;
				}
			}
			OutLocal = Amount == 0 ? Initial : ComposeAffine(Edited);
		}
		else
		{
			if (DistanceSquared(InPointer, Center) < 64 * UiScale * UiScale)
			{
				return false;
			}
			const float Current = RingAngle(InPointer, Axis);
			Angle += std::remainder(Current - PreviousAngle, 2 * Pi);
			PreviousAngle = Current;
			if (bLinearRotation)
			{
				Angle =
				    (Delta.X * RotationTangent.X + Delta.Y * RotationTangent.Y) / DistanceSquared(RotationTangent, {});
			}
			const auto Vector = ScaleVector(UnitAxes[Axis], std::sin(Angle / 2));
			const auto Turn = ComposeTRS({}, {Vector.X, Vector.Y, Vector.Z, std::cos(Angle / 2)}, {1, 1, 1});
			Edited.Rotation = DecomposeAffine(Multiply(Rotation, Turn)).Rotation;
			OutLocal = Angle == 0 ? Initial : ComposeAffine(Edited);
		}
		return IsAffine(OutLocal);
	}
	catch (const std::exception&)
	{
		return false;
	}
}

void FTransformGizmo::End()
{
	Active = ETransformGizmoHandle::None;
}
} // namespace Hyperion
