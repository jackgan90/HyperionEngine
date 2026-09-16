#include "Hyperion/Renderer/SceneNavigation.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
namespace
{
FVec3 Rotate(FVec3 InValue, FVec3 InAxis, float InAngle)
{
	const float Cosine = std::cos(InAngle);
	return Add(Add(ScaleVector(InValue, Cosine), ScaleVector(Cross(InAxis, InValue), std::sin(InAngle))),
	           ScaleVector(InAxis, Dot(InAxis, InValue) * (1 - Cosine)));
}

FSceneCameraPose RotateCameraPose(FSceneCameraPose InPose, float InYaw, float InPitch)
{
	InPose.Forward = Rotate(InPose.Forward, {0, 1, 0}, InYaw);
	InPose.Up = Rotate(InPose.Up, {0, 1, 0}, InYaw);
	InPose.Right = Normalize(Cross(InPose.Forward, InPose.Up));
	const float Elevation = std::asin(std::clamp(-InPose.Forward.Y, -1.f, 1.f));
	const float Pitch = std::clamp(Elevation + InPitch, -1.5f, 1.5f) - Elevation;
	InPose.Forward = Rotate(InPose.Forward, InPose.Right, -Pitch);
	InPose.Up = Rotate(InPose.Up, InPose.Right, -Pitch);
	return InPose;
}
} // namespace

std::optional<FSceneHandle> GetSceneNavigationCamera(const FSceneInstance& InScene)
{
	const auto Handle = InScene.GetSettings().DefaultCamera;
	FSceneNodeView View;
	if (!Handle || !InScene.GetNodeView(*Handle, View) || !View.Node->Camera || !View.bEffectiveEnabled)
	{
		return {};
	}
	return Handle;
}

FVec3 GetSceneNavigationPivot(const FSceneInstance& InScene)
{
	const auto Handle = GetSceneNavigationCamera(InScene);
	if (!Handle)
	{
		return {};
	}
	FSceneCameraPose Pose;
	InScene.GetCameraPose(*Handle, Pose);
	return Add(Pose.Eye, ScaleVector(Pose.Forward, InScene.FindNode(*Handle)->Camera->FocusDistance));
}

void OrbitSceneCamera(FSceneInstance& InScene, float InYaw, float InPitch)
{
	if (InYaw == 0 && InPitch == 0)
	{
		return;
	}
	const auto Handle = GetSceneNavigationCamera(InScene);
	if (!Handle)
	{
		return;
	}
	FSceneCameraPose Pose;
	InScene.GetCameraPose(*Handle, Pose);
	const auto Camera = *InScene.FindNode(*Handle)->Camera;
	const auto Pivot = Add(Pose.Eye, ScaleVector(Pose.Forward, Camera.FocusDistance));
	const auto Rotated = RotateCameraPose(Pose, InYaw, InPitch);
	InScene.SetCameraView(
	    *Handle,
	    SceneCameraTransform(Subtract(Pivot, ScaleVector(Rotated.Forward, Camera.FocusDistance)), Pivot, Rotated.Up),
	    Camera);
}

void RotateSceneCamera(FSceneInstance& InScene, float InYaw, float InPitch)
{
	if (InYaw == 0 && InPitch == 0)
	{
		return;
	}
	const auto Handle = GetSceneNavigationCamera(InScene);
	if (!Handle)
	{
		return;
	}
	FSceneCameraPose Pose;
	InScene.GetCameraPose(*Handle, Pose);
	const auto Rotated = RotateCameraPose(Pose, InYaw, InPitch);
	const auto Camera = *InScene.FindNode(*Handle)->Camera;
	InScene.SetCameraView(*Handle, SceneCameraTransform(Pose.Eye, Add(Pose.Eye, Rotated.Forward), Rotated.Up), Camera);
}

void DollySceneCamera(FSceneInstance& InScene, float InFactor, float InMinimum, float InMaximum,
                      std::optional<float> InFarPadding)
{
	const auto Handle = GetSceneNavigationCamera(InScene);
	if (!Handle)
	{
		return;
	}
	FSceneCameraPose Pose;
	InScene.GetCameraPose(*Handle, Pose);
	auto Camera = *InScene.FindNode(*Handle)->Camera;
	const auto Pivot = Add(Pose.Eye, ScaleVector(Pose.Forward, Camera.FocusDistance));
	Camera.FocusDistance = std::clamp(Camera.FocusDistance * InFactor, InMinimum, InMaximum);
	if (InFarPadding)
	{
		Camera.Far = Camera.FocusDistance + *InFarPadding;
	}
	InScene.SetCameraView(
	    *Handle, SceneCameraTransform(Subtract(Pivot, ScaleVector(Pose.Forward, Camera.FocusDistance)), Pivot, Pose.Up),
	    Camera);
}

void PanSceneCamera(FSceneInstance& InScene, FVec3 InSteps)
{
	const auto Handle = GetSceneNavigationCamera(InScene);
	if (!Handle)
	{
		return;
	}
	FSceneCameraPose Pose;
	InScene.GetCameraPose(*Handle, Pose);
	const auto Camera = *InScene.FindNode(*Handle)->Camera;
	const auto Right = Normalize(FVec3{Pose.Right.X, 0, Pose.Right.Z});
	const auto Forward = Normalize(FVec3{Pose.Forward.X, 0, Pose.Forward.Z});
	const float Step = std::max(.1f, Camera.FocusDistance * .08f);
	const auto Offset =
	    ScaleVector(Add(Add(ScaleVector(Right, InSteps.X), ScaleVector(Forward, InSteps.Z)), {0, InSteps.Y, 0}), Step);
	const auto Eye = Add(Pose.Eye, Offset);
	InScene.SetCameraView(*Handle, SceneCameraTransform(Eye, Add(Eye, Pose.Forward), Pose.Up), Camera);
}

void FitSceneCamera(FSceneInstance& InScene, float InAspect, bool bInModelViewerLens)
{
	const auto Handle = GetSceneNavigationCamera(InScene);
	if (!Handle)
	{
		return;
	}
	FBounds Bounds;
	for (const auto ModelHandle : InScene.GetNodes(ESceneNodeKind::Model))
	{
		FSceneNodeView View;
		InScene.GetNodeView(ModelHandle, View);
		const auto& Model = *View.Node->Model;
		if (!View.bEffectiveEnabled || !Model.bVisible || !Model.Data)
		{
			continue;
		}
		const auto WorldBounds = TransformBounds(Model.Data->Bounds, View.World);
		if (IsUsable(WorldBounds))
		{
			Bounds = IsUsable(Bounds) ? UnionBounds(Bounds, WorldBounds) : WorldBounds;
		}
	}
	if (!IsUsable(Bounds))
	{
		return;
	}
	FSceneCameraPose Pose;
	InScene.GetCameraPose(*Handle, Pose);
	auto Camera = *InScene.FindNode(*Handle)->Camera;
	const auto Center = ScaleVector(Add(Bounds.Minimum, Bounds.Maximum), .5f);
	const float Radius = std::max(.01f, Length(Subtract(Bounds.Maximum, Center)));
	const float Half = Camera.VerticalRadians * .5f;
	Camera.FocusDistance =
	    Radius / std::sin(std::min(Half, std::atan(std::tan(Half) * std::max(.001f, InAspect)))) * 1.12f;
	if (bInModelViewerLens)
	{
		Camera.Near = std::max(.0001f, Radius * .001f);
		Camera.Far = Camera.FocusDistance + Radius * 10;
	}
	InScene.SetCameraView(
	    *Handle,
	    SceneCameraTransform(Subtract(Center, ScaleVector(Pose.Forward, Camera.FocusDistance)), Center, Pose.Up),
	    Camera);
}
} // namespace Hyperion
