#include "Hyperion/Renderer/ViewportRay.h"
#include <cmath>
#include <stdexcept>

namespace Hyperion
{
FMat4 SceneCameraViewProjection(const FSceneCameraPose& InPose, const FSceneCamera& InLens, float InAspect,
                                EDepthConvention InDepth)
{
	const auto& Right = InPose.Right;
	const auto& Up = InPose.Up;
	const auto& Forward = InPose.Forward;
	const FMat4 View{{Right.X, Up.X, -Forward.X, 0, Right.Y, Up.Y, -Forward.Y, 0, Right.Z, Up.Z, -Forward.Z, 0,
	                  -Dot(Right, InPose.Eye), -Dot(Up, InPose.Eye), Dot(Forward, InPose.Eye), 1}};
	return Multiply(Perspective(InLens.VerticalRadians, InAspect, InLens.Near, InLens.Far, InDepth), View);
}

std::optional<FRay> MakeViewportRay(const FSceneCameraView& InCamera, FVec2 InPosition, std::uint32_t InWidth,
                                    std::uint32_t InHeight, EDepthConvention InDepth)
{
	if (!InWidth || !InHeight || !std::isfinite(InPosition.X) || !std::isfinite(InPosition.Y) || InPosition.X < 0 ||
	    InPosition.X > 1 || InPosition.Y < 0 || InPosition.Y > 1 ||
	    (InDepth != EDepthConvention::Standard && InDepth != EDepthConvention::Reversed))
	{
		return {};
	}
	try
	{
		ValidateSceneCameraView(InCamera);
		const auto Pose = ExtractScenePose(InCamera.World);
		// Use a zero-origin pose to avoid subtracting large world coordinates during unprojection.
		auto Relative = Pose;
		Relative.Eye = {};
		const auto InverseProjection =
		    Inverse(SceneCameraViewProjection(Relative, InCamera.Lens, float(InWidth) / InHeight, InDepth));
		const float NearDepth = InDepth == EDepthConvention::Reversed ? 1.f : 0.f;
		const auto Point = Transform(InverseProjection, {2 * InPosition.X - 1, 1 - 2 * InPosition.Y, NearDepth, 1});
		const auto Direction = Normalize(FVec3{Point.X / Point.W, Point.Y / Point.W, Point.Z / Point.W});
		const float Forward = Dot(Direction, Pose.Forward);
		const FRay Ray{Pose.Eye, Direction, InCamera.Lens.Near / Forward, InCamera.Lens.Far / Forward};
		return IsUsable(Ray) ? std::optional<FRay>(Ray) : std::nullopt;
	}
	catch (const std::invalid_argument&)
	{
		return {};
	}
}
} // namespace Hyperion
