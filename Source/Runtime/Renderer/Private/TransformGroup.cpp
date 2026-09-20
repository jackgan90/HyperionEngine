#include "Hyperion/Renderer/TransformGizmo.h"
#include <cmath>
#include <stdexcept>

namespace Hyperion
{
FMat4 FTransformGizmo::GroupDelta(const FMat4& InEditedLocal) const
{
	if (!IsAffine(InEditedLocal))
	{
		throw std::invalid_argument("Group transform must be finite and affine");
	}
	if (!bParentInvertible)
	{
		throw std::invalid_argument("Group manipulation requires an invertible primary parent");
	}
	FMat4 Result;
	if (Mode == ETransformGizmoMode::Position)
	{
		const auto Before = Multiply(ParentWorld, Initial);
		const auto After = Multiply(ParentWorld, InEditedLocal);
		Result = Translation({After.Values[12] - Before.Values[12], After.Values[13] - Before.Values[13],
		                      After.Values[14] - Before.Values[14]});
	}
	else if (Mode == ETransformGizmoMode::Rotation)
	{
		const unsigned Axis = static_cast<unsigned>(Active) - 1;
		if (Axis >= 3)
		{
			throw std::invalid_argument("Group rotation requires an active rotation axis");
		}
		FVec4 Quaternion{0, 0, 0, std::cos(Angle / 2)};
		const std::array Components{&Quaternion.X, &Quaternion.Y, &Quaternion.Z};
		*Components[Axis] = std::sin(Angle / 2);
		const auto Frame = Multiply(ParentWorld, ComposeAffine({Parts.Position, Parts.Rotation}));
		Result = Multiply(Multiply(Frame, ComposeTRS({}, Quaternion, {1, 1, 1})), Inverse(Frame));
	}
	else
	{
		if (Parts.Scale.X == 0 || Parts.Scale.Y == 0 || Parts.Scale.Z == 0)
		{
			throw std::invalid_argument(
			    "Group scale requires nonzero initial primary scale; use Details to restore it");
		}
		Result = Multiply(Multiply(ParentWorld, InEditedLocal), Multiply(Inverse(Initial), ParentInverse));
	}
	if (!IsAffine(Result))
	{
		throw std::invalid_argument("Group transform produced a nonfinite result");
	}
	if ((Mode != ETransformGizmoMode::Rotation && InEditedLocal.Values == Initial.Values) ||
	    (Mode == ETransformGizmoMode::Rotation && Angle == 0))
	{
		return Identity();
	}
	return Result;
}
} // namespace Hyperion
