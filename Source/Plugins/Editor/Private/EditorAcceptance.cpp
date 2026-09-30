#include "EditorApplication.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include <cmath>
#include <fstream>
#include <iomanip>

namespace Hyperion
{
namespace
{
void Move(std::vector<FInputEvent>& InEvents, float InX, float InY)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseMove;
	Event.X = InX;
	Event.Y = InY;
	InEvents.push_back(Event);
}

void Button(std::vector<FInputEvent>& InEvents, unsigned InButton, bool bInDown, float InX, float InY)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseButton;
	Event.Button = InButton;
	Event.bDown = bInDown;
	Event.X = InX;
	Event.Y = InY;
	InEvents.push_back(Event);
}

void Key(std::vector<FInputEvent>& InEvents, bool bInDown)
{
	FInputEvent Event;
	Event.Type = EEventType::Key;
	Event.Key = EKey::W;
	Event.bDown = bInDown;
	InEvents.push_back(Event);
}

void Wheel(std::vector<FInputEvent>& InEvents, float InDelta)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseWheel;
	Event.Y = InDelta;
	InEvents.push_back(Event);
}
} // namespace

void FEditorPlugin::ExerciseClick(std::vector<FInputEvent>& InEvents, FVec4 InBounds)
{
	if (++Acceptance.ExerciseWait <= 2 || InBounds.Z <= InBounds.X || InBounds.W <= InBounds.Y)
	{
		return;
	}
	const float X = (InBounds.X + InBounds.Z) * .5f;
	const float Y = (InBounds.Y + InBounds.W) * .5f;
	Move(InEvents, X, Y);
	Acceptance.bExerciseMouseDown = !Acceptance.bExerciseMouseDown;
	Button(InEvents, 0, Acceptance.bExerciseMouseDown, X, Y);
	if (!Acceptance.bExerciseMouseDown)
	{
		++Acceptance.ExerciseStep;
		Acceptance.ExerciseWait = 0;
	}
}

void FEditorPlugin::ExerciseMovement(std::vector<FInputEvent>& InEvents, const FSceneCameraPose& InPose, float InX,
                                     float InY)
{
	switch (Acceptance.ExerciseMovementStep)
	{
		case 0:
			Acceptance.ExercisePose = InPose;
			Key(InEvents, true);
			Acceptance.ExerciseWait = 0;
			++Acceptance.ExerciseMovementStep;
			break;
		case 1:
			if (++Acceptance.ExerciseWait < 6)
			{
				break;
			}
			Acceptance.bMovementGateVerified = Length(Subtract(InPose.Eye, Acceptance.ExercisePose.Eye)) < .00001f;
			Move(InEvents, InX, InY);
			Button(InEvents, 1, true, InX, InY);
			Acceptance.ExercisePose = InPose;
			Acceptance.ExerciseWait = 0;
			++Acceptance.ExerciseMovementStep;
			break;
		case 2:
			if (++Acceptance.ExerciseWait < 8)
			{
				break;
			}
			Acceptance.bMovementVerified = Length(Subtract(InPose.Eye, Acceptance.ExercisePose.Eye)) > .01f;
			Button(InEvents, 1, false, InX, InY);
			Acceptance.ExercisePose = InPose;
			Acceptance.ExerciseWait = 0;
			++Acceptance.ExerciseMovementStep;
			break;
		case 3:
			if (++Acceptance.ExerciseWait < 6)
			{
				break;
			}
			Acceptance.bRightReleaseVerified = Length(Subtract(InPose.Eye, Acceptance.ExercisePose.Eye)) < .00001f;
			Key(InEvents, false);
			Acceptance.ExerciseWait = 0;
			Acceptance.ExerciseStep = 6;
			break;
		default:
			break;
	}
}

void FEditorPlugin::ExerciseWheel(std::vector<FInputEvent>& InEvents, const FSceneCameraPose& InPose, float InX,
                                  float InY)
{
	const float Speed = Camera.GetMovementSpeed(Viewport.ViewCamera);
	switch (Acceptance.ExerciseWheelStep)
	{
		case 0:
			Acceptance.ExercisePose = InPose;
			Acceptance.ExerciseSpeed = Speed;
			Button(InEvents, 1, true, InX, InY);
			Wheel(InEvents, 1);
			++Acceptance.ExerciseWheelStep;
			break;
		case 1:
			Acceptance.bSpeedVerified = std::abs(Speed - Acceptance.ExerciseSpeed * 1.2f) < .0001f &&
			                            Length(Subtract(InPose.Eye, Acceptance.ExercisePose.Eye)) < .00001f &&
			                            Length(Subtract(InPose.Forward, Acceptance.ExercisePose.Forward)) < .00001f;
			Key(InEvents, true);
			Acceptance.ExerciseWait = 0;
			++Acceptance.ExerciseWheelStep;
			break;
		case 2:
			if (++Acceptance.ExerciseWait < 6)
			{
				break;
			}
			Acceptance.bSpeedVerified &=
			    std::abs(Length(Subtract(InPose.Eye, Acceptance.ExercisePose.Eye)) - Speed * 6 / 60) < .001f;
			Acceptance.ExercisePose = InPose;
			Key(InEvents, false);
			Button(InEvents, 1, false, InX, InY);
			Wheel(InEvents, -1);
			++Acceptance.ExerciseWheelStep;
			break;
		case 3:
			Acceptance.bSpeedVerified &=
			    Dot(Subtract(InPose.Eye, Acceptance.ExercisePose.Eye), InPose.Forward) < -.01f &&
			    std::abs(Speed - Acceptance.ExerciseSpeed * 1.2f) < .0001f;
			Acceptance.ExercisePose = InPose;
			Button(InEvents, 1, true, InX, InY);
			Wheel(InEvents, -1);
			++Acceptance.ExerciseWheelStep;
			break;
		case 4:
			Acceptance.bSpeedVerified &= std::abs(Speed - Acceptance.ExerciseSpeed) < .0001f &&
			                             Length(Subtract(InPose.Eye, Acceptance.ExercisePose.Eye)) < .00001f;
			Button(InEvents, 1, false, InX, InY);
			Acceptance.ExerciseStep = 7;
			break;
		default:
			break;
	}
}

void FEditorPlugin::ExerciseCamera(std::vector<FInputEvent>& InEvents)
{
	const auto Pose = ExtractScenePose(Viewport.ViewCamera.World);
	const float X = (Viewport.ViewportRegion.Bounds.X + Viewport.ViewportRegion.Bounds.Z) * .5f;
	const float Y = (Viewport.ViewportRegion.Bounds.Y + Viewport.ViewportRegion.Bounds.W) * .5f;
	switch (Acceptance.ExerciseStep)
	{
		case 5:
			ExerciseMovement(InEvents, Pose, X, Y);
			break;
		case 6:
			ExerciseWheel(InEvents, Pose, X, Y);
			break;
		case 7:
			Acceptance.ExercisePose = Pose;
			Move(InEvents, X, Y);
			Button(InEvents, 1, true, X, Y);
			++Acceptance.ExerciseStep;
			break;
		case 8:
			Move(InEvents, X + 36, Y + 20);
			++Acceptance.ExerciseStep;
			break;
		case 9:
			Button(InEvents, 1, false, X + 36, Y + 20);
			Acceptance.bLookVerified = Length(Subtract(Pose.Forward, Acceptance.ExercisePose.Forward)) > .01f &&
			                           Length(Subtract(Pose.Eye, Acceptance.ExercisePose.Eye)) < .00001f;
			++Acceptance.ExerciseStep;
			break;
		case 10:
			Acceptance.ExercisePose = Pose;
			Wheel(InEvents, 1);
			++Acceptance.ExerciseStep;
			break;
		case 11:
			Acceptance.bDollyVerified = Length(Subtract(Pose.Eye, Acceptance.ExercisePose.Eye)) > .01f;
			ExerciseClick(InEvents, FileMenuBounds);
			break;
		case 13:
			Acceptance.ExercisePose = Pose;
			Acceptance.ExerciseSpeed = Camera.GetMovementSpeed(Viewport.ViewCamera);
			Button(InEvents, 1, true, X, Y);
			Key(InEvents, true);
			Wheel(InEvents, 1);
			Acceptance.ExerciseWait = 0;
			++Acceptance.ExerciseStep;
			break;
		case 14:
			if (++Acceptance.ExerciseWait < 6)
			{
				break;
			}
			Key(InEvents, false);
			Button(InEvents, 1, false, X, Y);
			Acceptance.bInputIsolationVerified =
			    Length(Subtract(Pose.Eye, Acceptance.ExercisePose.Eye)) < .00001f &&
			    Length(Subtract(Pose.Forward, Acceptance.ExercisePose.Forward)) < .00001f &&
			    Camera.GetMovementSpeed(Viewport.ViewCamera) == Acceptance.ExerciseSpeed;
			++Acceptance.ExerciseStep;
			break;
		default:
			break;
	}
}

void FEditorPlugin::ExerciseInput(std::vector<FInputEvent>& InEvents)
{
	if (bOpenDialog && Browser->IsScanning())
	{
		Acceptance.ExerciseWait = 0;
		return;
	}
	if (FrameCount < 3)
	{
		return;
	}
	switch (Acceptance.ExerciseStep)
	{
		case 0:
		case 18:
			ExerciseClick(InEvents, FileMenuBounds);
			break;
		case 1:
		case 12:
		case 19:
			ExerciseClick(InEvents, OpenMenuBounds);
			break;
		case 2:
			ExerciseClick(InEvents, SponzaBounds);
			break;
		case 3:
			if (!Acceptance.bExerciseMouseDown && !bOpenDialog)
			{
				throw std::runtime_error("Single scene click unexpectedly opened the scene");
			}
			Acceptance.ExerciseWait = 2;
			ExerciseClick(InEvents, SponzaBounds);
			break;
		case 20:
			ExerciseClick(InEvents, OpenButtonBounds);
			break;
		case 4:
			if (bOpenDialog || !CurrentPath.ends_with("/Sponza.hasset"))
			{
				throw std::runtime_error("Double scene click did not open the selected scene");
			}
			if (ReadyFrames > 8)
			{
				ExerciseClick(InEvents, Viewport.ViewportRegion.Bounds);
			}
			break;
		case 15:
			ExerciseClick(InEvents, CancelButtonBounds);
			break;
		case 16:
			Window->Resize({1440, 900});
			bShowViewport = false;
			++Acceptance.ExerciseStep;
			Acceptance.ExerciseWait = 0;
			break;
		case 17:
			if (++Acceptance.ExerciseWait < 4)
			{
				break;
			}
			bShowViewport = true;
			Acceptance.ExerciseWait = 0;
			++Acceptance.ExerciseStep;
			break;
		default:
			ExerciseCamera(InEvents);
			break;
	}
}

} // namespace Hyperion
