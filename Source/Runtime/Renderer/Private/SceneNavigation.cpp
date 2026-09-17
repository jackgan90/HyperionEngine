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
	if (!Handle || !InScene.GetNodeView(*Handle, View) || !View.Node->Camera() || !View.bEffectiveEnabled)
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
	return Add(Pose.Eye, ScaleVector(Pose.Forward, InScene.FindNode(*Handle)->Camera()->FocusDistance));
}

bool GetSceneCameraView(const FSceneInstance& InScene, FSceneCameraView& OutView)
{
	const auto Handle = GetSceneNavigationCamera(InScene);
	FSceneNodeView View;
	if (!Handle || !InScene.GetNodeView(*Handle, View))
	{
		return false;
	}
	OutView = {*View.Node->Camera(), View.World};
	return true;
}

bool CanInitializeSceneBrowsingView(const FSceneInstance& InScene)
{
	const auto& Status = InScene.GetStatus();
	if (!Status.bLoaded || Status.bClosed || !Status.Error.empty())
	{
		return false;
	}
	if (InScene.GetSettings().InitialView)
	{
		return true;
	}
	const auto Models = InScene.GetModels();
	return std::all_of(Models.begin(), Models.end(),
	                   [&](const FSceneInstanceModel& InInstance)
	                   {
		                   const auto* Model = InScene.Find(InInstance.Handle);
		                   return Model && (Model->Data || !InScene.GetError(InInstance.Handle).empty());
	                   });
}

FSceneCameraView MakeSceneBrowsingView(const FSceneInstance& InScene, float InAspect)
{
	if (InScene.GetSettings().InitialView)
	{
		return *InScene.GetSettings().InitialView;
	}
	FSceneCameraView View;
	View.World = SceneCameraTransform({0, 3, 12}, {});
	FitSceneCamera(View, InScene, InAspect, true);
	return View;
}

namespace
{
template<class T> void Navigate(FSceneInstance& InScene, const T& InOperation)
{
	const auto Handle = GetSceneNavigationCamera(InScene);
	FSceneCameraView Camera;
	if (Handle && GetSceneCameraView(InScene, Camera))
	{
		InOperation(Camera);
		InScene.SetCameraView(*Handle, Camera.World, Camera.Lens);
	}
}
} // namespace

void OrbitSceneCamera(FSceneInstance& InScene, float InYaw, float InPitch)
{
	Navigate(InScene,
	         [&](FSceneCameraView& InView)
	         {
		         OrbitSceneCamera(InView, InYaw, InPitch);
	         });
}

void RotateSceneCamera(FSceneInstance& InScene, float InYaw, float InPitch)
{
	Navigate(InScene,
	         [&](FSceneCameraView& InView)
	         {
		         RotateSceneCamera(InView, InYaw, InPitch);
	         });
}

void DollySceneCamera(FSceneInstance& InScene, float InFactor, float InMinimum, float InMaximum,
                      std::optional<float> InFarPadding)
{
	Navigate(InScene,
	         [&](FSceneCameraView& InView)
	         {
		         DollySceneCamera(InView, InFactor, InMinimum, InMaximum, InFarPadding);
	         });
}

void PanSceneCamera(FSceneInstance& InScene, FVec3 InSteps)
{
	Navigate(InScene,
	         [&](FSceneCameraView& InView)
	         {
		         PanSceneCamera(InView, InSteps);
	         });
}

void FitSceneCamera(FSceneInstance& InScene, float InAspect, bool bInModelViewerLens)
{
	Navigate(InScene,
	         [&](FSceneCameraView& InView)
	         {
		         FitSceneCamera(InView, InScene, InAspect, bInModelViewerLens);
	         });
}

void OrbitSceneCamera(FSceneCameraView& InCamera, float InYaw, float InPitch)
{
	if (InYaw == 0 && InPitch == 0)
	{
		return;
	}
	const auto Pose = ExtractScenePose(InCamera.World);
	const auto& Camera = InCamera.Lens;
	const auto Pivot = Add(Pose.Eye, ScaleVector(Pose.Forward, Camera.FocusDistance));
	auto Rotated = RotateCameraPose(Pose, InYaw, InPitch);
	Rotated.Eye = Subtract(Pivot, ScaleVector(Rotated.Forward, Camera.FocusDistance));
	InCamera.World = SceneCameraTransform(Rotated);
}

void RotateSceneCamera(FSceneCameraView& InCamera, float InYaw, float InPitch)
{
	if (InYaw == 0 && InPitch == 0)
	{
		return;
	}
	const auto Pose = ExtractScenePose(InCamera.World);
	const auto Rotated = RotateCameraPose(Pose, InYaw, InPitch);
	InCamera.World = SceneCameraTransform(Rotated);
}

void DollySceneCamera(FSceneCameraView& InCamera, float InFactor, float InMinimum, float InMaximum,
                      std::optional<float> InFarPadding)
{
	const auto Pose = ExtractScenePose(InCamera.World);
	auto& Camera = InCamera.Lens;
	const auto Pivot = Add(Pose.Eye, ScaleVector(Pose.Forward, Camera.FocusDistance));
	Camera.FocusDistance = std::clamp(Camera.FocusDistance * InFactor, InMinimum, InMaximum);
	if (InFarPadding)
	{
		Camera.Far = Camera.FocusDistance + *InFarPadding;
	}
	auto Moved = Pose;
	Moved.Eye = Subtract(Pivot, ScaleVector(Pose.Forward, Camera.FocusDistance));
	InCamera.World = SceneCameraTransform(Moved);
}

void PanSceneCamera(FSceneCameraView& InCamera, FVec3 InSteps)
{
	const auto Pose = ExtractScenePose(InCamera.World);
	const auto& Camera = InCamera.Lens;
	const auto Right = Normalize(FVec3{Pose.Right.X, 0, Pose.Right.Z});
	const auto Forward = Normalize(FVec3{Pose.Forward.X, 0, Pose.Forward.Z});
	const float Step = std::max(.1f, Camera.FocusDistance * .08f);
	const auto Offset =
	    ScaleVector(Add(Add(ScaleVector(Right, InSteps.X), ScaleVector(Forward, InSteps.Z)), {0, InSteps.Y, 0}), Step);
	auto Moved = Pose;
	Moved.Eye = Add(Pose.Eye, Offset);
	InCamera.World = SceneCameraTransform(Moved);
}

void FitSceneCamera(FSceneCameraView& InCamera, const FSceneInstance& InScene, float InAspect, bool bInModelViewerLens)
{
	FBounds Bounds;
	for (const auto& ModelHandle : InScene.GetNodes(ESceneNodeKind::Model))
	{
		FSceneNodeView View;
		InScene.GetNodeView(ModelHandle, View);
		const auto& Model = *View.Node->Model();
		if (!View.bEffectiveEnabled || !Model.bVisible || !Model.Data)
		{
			continue;
		}
		const auto BoundsValue = SceneModelBounds(SceneModelTransfer(*View.Node, View.World, View.bEffectiveEnabled));
		const auto WorldBounds = TransformBounds(BoundsValue, View.World);
		if (IsUsable(WorldBounds))
		{
			Bounds = IsUsable(Bounds) ? UnionBounds(Bounds, WorldBounds) : WorldBounds;
		}
	}
	if (!IsUsable(Bounds))
	{
		return;
	}
	const auto Pose = ExtractScenePose(InCamera.World);
	auto& Camera = InCamera.Lens;
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
	auto Moved = Pose;
	Moved.Eye = Subtract(Center, ScaleVector(Pose.Forward, Camera.FocusDistance));
	InCamera.World = SceneCameraTransform(Moved);
}
} // namespace Hyperion
