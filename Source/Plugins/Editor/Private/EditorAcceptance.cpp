#include "EditorAcceptanceHarness.h"
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

void FEditorAcceptanceHarness::ExerciseClick(std::vector<FInputEvent>& InEvents, FVec4 InBounds)
{
	if (++Scenario.ExerciseWait <= 2 || InBounds.Z <= InBounds.X || InBounds.W <= InBounds.Y)
	{
		return;
	}
	const float X = (InBounds.X + InBounds.Z) * .5f;
	const float Y = (InBounds.Y + InBounds.W) * .5f;
	Move(InEvents, X, Y);
	Scenario.bExerciseMouseDown = !Scenario.bExerciseMouseDown;
	Button(InEvents, 0, Scenario.bExerciseMouseDown, X, Y);
	if (!Scenario.bExerciseMouseDown)
	{
		++Scenario.ExerciseStep;
		Scenario.ExerciseWait = 0;
	}
}

void FEditorAcceptanceHarness::ExerciseMovement(std::vector<FInputEvent>& InEvents, const FSceneCameraPose& InPose,
                                                float InX, float InY)
{
	switch (Scenario.ExerciseMovementStep)
	{
		case 0:
			Scenario.ExercisePose = InPose;
			Key(InEvents, true);
			Scenario.ExerciseWait = 0;
			++Scenario.ExerciseMovementStep;
			break;
		case 1:
			if (++Scenario.ExerciseWait < 6)
			{
				break;
			}
			Scenario.bMovementGateVerified = Length(Subtract(InPose.Eye, Scenario.ExercisePose.Eye)) < .00001f;
			Move(InEvents, InX, InY);
			Button(InEvents, 1, true, InX, InY);
			Scenario.ExercisePose = InPose;
			Scenario.ExerciseWait = 0;
			++Scenario.ExerciseMovementStep;
			break;
		case 2:
			if (++Scenario.ExerciseWait < 8)
			{
				break;
			}
			Scenario.bMovementVerified = Length(Subtract(InPose.Eye, Scenario.ExercisePose.Eye)) > .01f;
			Button(InEvents, 1, false, InX, InY);
			Scenario.ExercisePose = InPose;
			Scenario.ExerciseWait = 0;
			++Scenario.ExerciseMovementStep;
			break;
		case 3:
			if (++Scenario.ExerciseWait < 6)
			{
				break;
			}
			Scenario.bRightReleaseVerified = Length(Subtract(InPose.Eye, Scenario.ExercisePose.Eye)) < .00001f;
			Key(InEvents, false);
			Scenario.ExerciseWait = 0;
			Scenario.ExerciseStep = 6;
			break;
		default:
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseWheel(std::vector<FInputEvent>& InEvents, const FSceneCameraPose& InPose,
                                             float InX, float InY)
{
	const float Speed = Editor.Camera.GetMovementSpeed(Editor.Viewport.ViewCamera);
	switch (Scenario.ExerciseWheelStep)
	{
		case 0:
			Scenario.ExercisePose = InPose;
			Scenario.ExerciseSpeed = Speed;
			Button(InEvents, 1, true, InX, InY);
			Wheel(InEvents, 1);
			++Scenario.ExerciseWheelStep;
			break;
		case 1:
			Scenario.bSpeedVerified = std::abs(Speed - Scenario.ExerciseSpeed * 1.2f) < .0001f &&
			                          Length(Subtract(InPose.Eye, Scenario.ExercisePose.Eye)) < .00001f &&
			                          Length(Subtract(InPose.Forward, Scenario.ExercisePose.Forward)) < .00001f;
			Key(InEvents, true);
			Scenario.ExerciseWait = 0;
			++Scenario.ExerciseWheelStep;
			break;
		case 2:
			if (++Scenario.ExerciseWait < 6)
			{
				break;
			}
			Scenario.bSpeedVerified &=
			    std::abs(Length(Subtract(InPose.Eye, Scenario.ExercisePose.Eye)) - Speed * 6 / 60) < .001f;
			Scenario.ExercisePose = InPose;
			Key(InEvents, false);
			Button(InEvents, 1, false, InX, InY);
			Wheel(InEvents, -1);
			++Scenario.ExerciseWheelStep;
			break;
		case 3:
			Scenario.bSpeedVerified &= Dot(Subtract(InPose.Eye, Scenario.ExercisePose.Eye), InPose.Forward) < -.01f &&
			                           std::abs(Speed - Scenario.ExerciseSpeed * 1.2f) < .0001f;
			Scenario.ExercisePose = InPose;
			Button(InEvents, 1, true, InX, InY);
			Wheel(InEvents, -1);
			++Scenario.ExerciseWheelStep;
			break;
		case 4:
			Scenario.bSpeedVerified &= std::abs(Speed - Scenario.ExerciseSpeed) < .0001f &&
			                           Length(Subtract(InPose.Eye, Scenario.ExercisePose.Eye)) < .00001f;
			Button(InEvents, 1, false, InX, InY);
			Scenario.ExerciseStep = 7;
			break;
		default:
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseCamera(std::vector<FInputEvent>& InEvents)
{
	const auto Pose = ExtractScenePose(Editor.Viewport.ViewCamera.World);
	const float X = (Editor.Viewport.ViewportRegion.Bounds.X + Editor.Viewport.ViewportRegion.Bounds.Z) * .5f;
	const float Y = (Editor.Viewport.ViewportRegion.Bounds.Y + Editor.Viewport.ViewportRegion.Bounds.W) * .5f;
	switch (Scenario.ExerciseStep)
	{
		case 5:
			ExerciseMovement(InEvents, Pose, X, Y);
			break;
		case 6:
			ExerciseWheel(InEvents, Pose, X, Y);
			break;
		case 7:
			Scenario.ExercisePose = Pose;
			Move(InEvents, X, Y);
			Button(InEvents, 1, true, X, Y);
			++Scenario.ExerciseStep;
			break;
		case 8:
			Move(InEvents, X + 36, Y + 20);
			++Scenario.ExerciseStep;
			break;
		case 9:
			Button(InEvents, 1, false, X + 36, Y + 20);
			Scenario.bLookVerified = Length(Subtract(Pose.Forward, Scenario.ExercisePose.Forward)) > .01f &&
			                         Length(Subtract(Pose.Eye, Scenario.ExercisePose.Eye)) < .00001f;
			++Scenario.ExerciseStep;
			break;
		case 10:
			Scenario.ExercisePose = Pose;
			Wheel(InEvents, 1);
			++Scenario.ExerciseStep;
			break;
		case 11:
			Scenario.bDollyVerified = Length(Subtract(Pose.Eye, Scenario.ExercisePose.Eye)) > .01f;
			ExerciseClick(InEvents, Scenario.FileMenuBounds);
			break;
		case 13:
			Scenario.ExercisePose = Pose;
			Scenario.ExerciseSpeed = Editor.Camera.GetMovementSpeed(Editor.Viewport.ViewCamera);
			Button(InEvents, 1, true, X, Y);
			Key(InEvents, true);
			Wheel(InEvents, 1);
			Scenario.ExerciseWait = 0;
			++Scenario.ExerciseStep;
			break;
		case 14:
			if (++Scenario.ExerciseWait < 6)
			{
				break;
			}
			Key(InEvents, false);
			Button(InEvents, 1, false, X, Y);
			Scenario.bInputIsolationVerified =
			    Length(Subtract(Pose.Eye, Scenario.ExercisePose.Eye)) < .00001f &&
			    Length(Subtract(Pose.Forward, Scenario.ExercisePose.Forward)) < .00001f &&
			    Editor.Camera.GetMovementSpeed(Editor.Viewport.ViewCamera) == Scenario.ExerciseSpeed;
			++Scenario.ExerciseStep;
			break;
		default:
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseInput(std::vector<FInputEvent>& InEvents)
{
	if (Editor.bOpenDialog && Editor.Browser->IsScanning())
	{
		Scenario.ExerciseWait = 0;
		return;
	}
	if (Editor.FrameCount < 3)
	{
		return;
	}
	switch (Scenario.ExerciseStep)
	{
		case 0:
		case 18:
			ExerciseClick(InEvents, Scenario.FileMenuBounds);
			break;
		case 1:
		case 12:
		case 19:
			ExerciseClick(InEvents, Scenario.OpenMenuBounds);
			break;
		case 2:
			ExerciseClick(InEvents, Scenario.SponzaBounds);
			break;
		case 3:
			if (!Scenario.bExerciseMouseDown && !Editor.bOpenDialog)
			{
				throw std::runtime_error("Single scene click unexpectedly opened the scene");
			}
			Scenario.ExerciseWait = 2;
			ExerciseClick(InEvents, Scenario.SponzaBounds);
			break;
		case 20:
			ExerciseClick(InEvents, Scenario.OpenButtonBounds);
			break;
		case 4:
			if (Editor.bOpenDialog || !Editor.CurrentPath.ends_with("/Sponza.hasset"))
			{
				throw std::runtime_error("Double scene click did not open the selected scene");
			}
			if (Editor.ReadyFrames > 8)
			{
				ExerciseClick(InEvents, Editor.Viewport.ViewportRegion.Bounds);
			}
			break;
		case 15:
			ExerciseClick(InEvents, Scenario.CancelButtonBounds);
			break;
		case 16:
			Editor.Window->Resize({1440, 900});
			Editor.bShowViewport = false;
			++Scenario.ExerciseStep;
			Scenario.ExerciseWait = 0;
			break;
		case 17:
			if (++Scenario.ExerciseWait < 4)
			{
				break;
			}
			Editor.bShowViewport = true;
			Scenario.ExerciseWait = 0;
			++Scenario.ExerciseStep;
			break;
		default:
			ExerciseCamera(InEvents);
			break;
	}
}

} // namespace Hyperion
