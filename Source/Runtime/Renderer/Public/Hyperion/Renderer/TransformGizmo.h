#pragma once
#include "Hyperion/Math/AffineTransform.h"
#include "Hyperion/Scene/SceneCamera.h"
#include <vector>

namespace Hyperion
{
enum class ETransformGizmoMode
{
	Position,
	Rotation,
	Scale
};
enum class ETransformGizmoHandle
{
	None,
	X,
	Y,
	Z,
	Screen
};

// Logical viewport coordinates. A host may draw these in any overlay renderer.
struct FTransformGizmoStroke
{
	std::vector<FVec2> Points;
	FVec4 Color;
	float Thickness = 2;
	bool bFilled{};
	ETransformGizmoHandle Handle{};
};

// Main-thread, per-viewport controller. No scene ownership, GUI or native backend dependency.
// Position uses world axes; rotation and scale use the decomposed local rotation axes.
class FTransformGizmo
{
public:
	bool Configure(const FSceneCameraView& InCamera, FVec4 InViewport, const FMat4& InLocal, const FMat4& InParent,
	               ETransformGizmoMode InMode, float InUiScale = 1);
	std::vector<FTransformGizmoStroke> Geometry(ETransformGizmoHandle InHovered = ETransformGizmoHandle::None) const;
	ETransformGizmoHandle HitTest(FVec2 InPointer) const;
	bool Begin(FVec2 InPointer);
	bool Drag(FVec2 InPointer, FMat4& OutLocal);
	// Common world operation matching the primary's single-object result. Rotation does not invert stretch.
	// Throws for undefined group scale ratios or singular required parent mappings.
	FMat4 GroupDelta(const FMat4& InEditedLocal) const;
	void End();

	bool IsDragging() const
	{
		return Active != ETransformGizmoHandle::None;
	}

	ETransformGizmoHandle ActiveHandle() const
	{
		return Active;
	}

	const FMat4& InitialLocal() const
	{
		return Initial;
	}

private:
	bool Project(FVec3 InPoint, FVec2& OutPoint) const;
	FVec3 RingPoint(unsigned InAxis, float InAngle) const;
	void AddAxes(std::vector<FTransformGizmoStroke>& OutStrokes) const;
	void AddRings(std::vector<FTransformGizmoStroke>& OutStrokes) const;
	float RingAngle(FVec2 InPointer, unsigned InAxis) const;
	FMat4 Initial = Identity();
	FMat4 ParentInverse = Identity();
	FMat4 ParentWorld = Identity();
	FMat4 Rotation = Identity();
	FAffineTransform Parts;
	FSceneCameraPose Camera;
	FVec4 Viewport;
	FVec3 Pivot;
	std::array<FVec3, 3> Axes;
	std::array<FVec2, 3> Tips;
	FVec2 Center;
	FVec2 Start;
	FVec2 RotationTangent;
	float Focal{};
	float Radius{};
	float UiScale = 1;
	float PreviousAngle{};
	float Angle{};
	ETransformGizmoMode Mode{};
	ETransformGizmoHandle Active{};
	bool bValid{};
	bool bParentInvertible{};
	bool bLinearRotation{};
};
} // namespace Hyperion
