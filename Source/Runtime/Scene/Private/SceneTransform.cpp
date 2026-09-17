#include "Hyperion/Math/AffineTransform.h"
#include "Hyperion/Scene/SceneNode.h"
#include <cmath>
#include <numbers>

namespace Hyperion
{
// This record is an editing view only; FSceneTransform::Local remains the authored value.
struct FTransformDisplay
{
	FVec3 Position;
	FVec3 Rotation;
	FVec3 Scale{1, 1, 1};
	std::string Parent;
};

namespace
{
constexpr float DegreesPerRadian = 180.f / std::numbers::pi_v<float>;

FRecordMemberOptions VectorRow(std::string InLabel, std::string InUnit, std::string InTooltip)
{
	return {.Inspector = FPropertyPresentation{.Label = std::move(InLabel),
	                                           .Widget = EPropertyWidget::Vector3,
	                                           .Unit = std::move(InUnit),
	                                           .Tooltip = std::move(InTooltip)}};
}

bool Same(FVec3 InA, FVec3 InB)
{
	return InA.X == InB.X && InA.Y == InB.Y && InA.Z == InB.Z;
}

FTransformDisplay ProjectTransform(const FSceneTransform& InSource)
{
	const auto Affine = DecomposeAffine(InSource.Local);
	return {Affine.Position, ScaleVector(Affine.Rotation, DegreesPerRadian), Affine.Scale, InSource.Parent};
}

void ApplyTransform(FSceneTransform& InCandidate, const FTransformDisplay& InEdited,
                    const FTransformDisplay& InOriginal)
{
	if (!Same(InEdited.Rotation, InOriginal.Rotation) || !Same(InEdited.Scale, InOriginal.Scale))
	{
		auto Affine = DecomposeAffine(InCandidate.Local);
		Affine.Rotation = ScaleVector(InEdited.Rotation, 1 / DegreesPerRadian);
		Affine.Scale = InEdited.Scale;
		InCandidate.Local = ComposeAffine(Affine);
	}
	// Do not round-trip the linear matrix for a parent-only or translation-only edit.
	if (!Same(InEdited.Position, InOriginal.Position))
	{
		InCandidate.Local.Values[12] = InEdited.Position.X;
		InCandidate.Local.Values[13] = InEdited.Position.Y;
		InCandidate.Local.Values[14] = InEdited.Position.Z;
	}
	InCandidate.Parent = InEdited.Parent;
}
} // namespace

template<> const FRecordDescriptor& RecordType<FTransformDisplay>()
{
	static const auto Type = MakeRecord<FTransformDisplay>(
	    "hyperion.transformdisplay",
	    {Member("position", &FTransformDisplay::Position,
	            VectorRow("Position", {}, "Local position relative to the parent origin.")),
	     Member("rotation", &FTransformDisplay::Rotation,
	            VectorRow("Rotation", "deg",
	                      "Local Euler angles in degrees. Right-handed fixed axes: X, then Y, then Z (Rz * Ry * Rx). "
	                      "Shear is retained.")),
	     Member(
	         "scale", &FTransformDisplay::Scale,
	         VectorRow("Scale", {},
	                   "Local signed scale. Reflections use a canonical signed X axis; existing shear is retained.")),
	     Member("parent", &FTransformDisplay::Parent, Inspect("Parent ID (local)"))},
	    1,
	    [](const FTransformDisplay& InValue)
	    {
		    for (const auto Vector : {InValue.Position, InValue.Rotation, InValue.Scale})
		    {
			    if (!std::isfinite(Vector.X) || !std::isfinite(Vector.Y) || !std::isfinite(Vector.Z))
			    {
				    throw std::invalid_argument("Transform display values must be finite");
			    }
		    }
	    });
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneTransform>()
{
	static const auto Type = []
	{
		auto Result = MakeRecord<FSceneTransform>(
		    "hyperion.scenetransform",
		    {Member("parent", &FSceneTransform::Parent), Member("local", &FSceneTransform::Local)}, 1,
		    [](const FSceneTransform& InValue)
		    {
			    if (!IsAffine(InValue.Local))
			    {
				    throw std::invalid_argument("Transform must be finite and affine");
			    }
		    });
		Result.DisplayLayout =
		    MakeRecordDisplayLayout<FSceneTransform, FTransformDisplay>(ProjectTransform, ApplyTransform);
		return Result;
	}();
	return Type;
}
} // namespace Hyperion
