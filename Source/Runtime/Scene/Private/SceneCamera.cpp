#include "Hyperion/Scene/SceneCamera.h"
#include <cmath>
#include <stdexcept>

namespace Hyperion
{
void ValidateSceneCameraView(const FSceneCameraView& InView)
{
	ValidateSceneCamera(InView.Lens);
	ExtractScenePose(InView.World);
}

template<> const FRecordDescriptor& RecordType<FSceneCameraView>()
{
	static const auto Type = MakeRecord<FSceneCameraView>(
	    "hyperion.sceneview",
	    {Member("lens", &FSceneCameraView::Lens, {true}), Member("world", &FSceneCameraView::World, {true})}, 1,
	    ValidateSceneCameraView);
	return Type;
}

void ValidateSceneCamera(const FSceneCamera& InCamera)
{
	if (!std::isfinite(InCamera.VerticalRadians) || InCamera.VerticalRadians <= .01f ||
	    InCamera.VerticalRadians >= 3.f || !std::isfinite(InCamera.Near) || InCamera.Near <= 0 ||
	    !std::isfinite(InCamera.Far) || InCamera.Far <= InCamera.Near || !std::isfinite(InCamera.FocusDistance) ||
	    InCamera.FocusDistance <= 0)
	{
		throw std::invalid_argument("Invalid scene camera lens or focus distance");
	}
}

bool TryExtractScenePose(const FMat4& InWorld, FSceneCameraPose& OutPose)
{
	if (!IsAffine(InWorld))
	{
		return false;
	}
	constexpr float Epsilon = 1e-6f;
	const auto Eye = Transform(InWorld, {0, 0, 0, 1});
	const auto ForwardValue = Transform(InWorld, {0, 0, -1, 0});
	const auto UpValue = Transform(InWorld, {0, 1, 0, 0});
	const FVec3 ForwardRaw{ForwardValue.X, ForwardValue.Y, ForwardValue.Z};
	const FVec3 UpRaw{UpValue.X, UpValue.Y, UpValue.Z};
	const auto ForwardLength = Length(ForwardRaw);
	const auto UpLength = Length(UpRaw);
	if (!std::isfinite(ForwardLength) || !std::isfinite(UpLength) || ForwardLength <= Epsilon || UpLength <= Epsilon)
	{
		return false;
	}
	const auto Forward = ScaleVector(ForwardRaw, 1 / ForwardLength);
	const auto RightRaw = Cross(Forward, UpRaw);
	const auto RightLength = Length(RightRaw);
	if (!std::isfinite(RightLength) || RightLength <= Epsilon)
	{
		return false;
	}
	const auto Right = ScaleVector(RightRaw, 1 / RightLength);
	const auto UpRawOrthogonal = Cross(Right, Forward);
	const auto UpOrthogonalLength = Length(UpRawOrthogonal);
	if (!std::isfinite(UpOrthogonalLength) || UpOrthogonalLength <= Epsilon)
	{
		return false;
	}
	FSceneCameraPose Pose;
	Pose.Eye = {Eye.X, Eye.Y, Eye.Z};
	Pose.Forward = Forward;
	Pose.Right = Right;
	Pose.Up = ScaleVector(UpRawOrthogonal, 1 / UpOrthogonalLength);
	if (!IsFinite(Pose.Eye) || !IsFinite(Pose.Forward) || !IsFinite(Pose.Right) || !IsFinite(Pose.Up))
	{
		return false;
	}
	OutPose = Pose;
	return true;
}

FSceneCameraPose ExtractScenePose(const FMat4& InWorld)
{
	FSceneCameraPose Pose;
	if (!TryExtractScenePose(InWorld, Pose))
	{
		throw std::invalid_argument("Scene transform has a degenerate forward/up basis");
	}
	return Pose;
}

FMat4 SceneCameraTransform(FVec3 InEye, FVec3 InTarget, FVec3 InUp)
{
	constexpr float Epsilon = 1e-6f;
	const auto Direction = Subtract(InTarget, InEye);
	const auto Distance = Length(Direction);
	if (!IsFinite(InEye) || !IsFinite(InTarget) || !IsFinite(InUp) || !std::isfinite(Distance) || Distance <= Epsilon ||
	    Length(Cross(ScaleVector(Direction, 1 / Distance), InUp)) <= Epsilon)
	{
		throw std::invalid_argument("Invalid scene camera eye, target or up");
	}
	// Construct the rigid inverse directly so repeated navigation preserves the supplied eye.
	const auto Forward = ScaleVector(Direction, 1 / Distance);
	const auto Right = Normalize(Cross(Forward, InUp));
	const auto Up = Cross(Right, Forward);
	const FMat4 World{{Right.X, Right.Y, Right.Z, 0, Up.X, Up.Y, Up.Z, 0, -Forward.X, -Forward.Y, -Forward.Z, 0,
	                   InEye.X, InEye.Y, InEye.Z, 1}};
	ExtractScenePose(World);
	return World;
}

template<> const FRecordDescriptor& RecordType<FSceneCamera>()
{
	static const auto Type = []
	{
		auto Result = MakeRecord<FSceneCamera>(
		    "hyperion.scenecamera",
		    {Member("verticalRadians", &FSceneCamera::VerticalRadians,
		            Inspect("Vertical field of view (radians)", .010001, 2.999999)),
		     Member("near", &FSceneCamera::Near, Inspect("Near plane", .000001, {})),
		     Member("far", &FSceneCamera::Far, Inspect("Far plane", .000001, {})),
		     Member("focusDistance", &FSceneCamera::FocusDistance, Inspect("Focus distance", .000001, {}))},
		    1, ValidateSceneCamera);
		Result.bRejectUnknownFields = true;
		return Result;
	}();
	return Type;
}
} // namespace Hyperion
