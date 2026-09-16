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

void FEditorApplication::ExerciseClick(std::vector<FInputEvent>& InEvents, FVec4 InBounds)
{
	if (++ExerciseWait <= 2 || InBounds.Z <= InBounds.X || InBounds.W <= InBounds.Y)
	{
		return;
	}
	const float X = (InBounds.X + InBounds.Z) * .5f;
	const float Y = (InBounds.Y + InBounds.W) * .5f;
	Move(InEvents, X, Y);
	bExerciseMouseDown = !bExerciseMouseDown;
	Button(InEvents, 0, bExerciseMouseDown, X, Y);
	if (!bExerciseMouseDown)
	{
		++ExerciseStep;
		ExerciseWait = 0;
	}
}

void FEditorApplication::ExerciseMovement(std::vector<FInputEvent>& InEvents, const FSceneCameraPose& InPose, float InX,
                                          float InY)
{
	switch (ExerciseMovementStep)
	{
		case 0:
			ExercisePose = InPose;
			Key(InEvents, true);
			ExerciseWait = 0;
			++ExerciseMovementStep;
			break;
		case 1:
			if (++ExerciseWait < 6)
			{
				break;
			}
			bMovementGateVerified = Length(Subtract(InPose.Eye, ExercisePose.Eye)) < .00001f;
			Move(InEvents, InX, InY);
			Button(InEvents, 1, true, InX, InY);
			ExercisePose = InPose;
			ExerciseWait = 0;
			++ExerciseMovementStep;
			break;
		case 2:
			if (++ExerciseWait < 8)
			{
				break;
			}
			bMovementVerified = Length(Subtract(InPose.Eye, ExercisePose.Eye)) > .01f;
			Button(InEvents, 1, false, InX, InY);
			ExercisePose = InPose;
			ExerciseWait = 0;
			++ExerciseMovementStep;
			break;
		case 3:
			if (++ExerciseWait < 6)
			{
				break;
			}
			bRightReleaseVerified = Length(Subtract(InPose.Eye, ExercisePose.Eye)) < .00001f;
			Key(InEvents, false);
			ExerciseWait = 0;
			ExerciseStep = 6;
			break;
		default:
			break;
	}
}

void FEditorApplication::ExerciseWheel(std::vector<FInputEvent>& InEvents, const FSceneCameraPose& InPose, float InX,
                                       float InY)
{
	const float Speed = Camera.GetMovementSpeed(*Scene);
	switch (ExerciseWheelStep)
	{
		case 0:
			ExercisePose = InPose;
			ExerciseSpeed = Speed;
			Button(InEvents, 1, true, InX, InY);
			Wheel(InEvents, 1);
			++ExerciseWheelStep;
			break;
		case 1:
			bSpeedVerified = std::abs(Speed - ExerciseSpeed * 1.2f) < .0001f &&
			                 Length(Subtract(InPose.Eye, ExercisePose.Eye)) < .00001f &&
			                 Length(Subtract(InPose.Forward, ExercisePose.Forward)) < .00001f;
			Key(InEvents, true);
			ExerciseWait = 0;
			++ExerciseWheelStep;
			break;
		case 2:
			if (++ExerciseWait < 6)
			{
				break;
			}
			bSpeedVerified &= std::abs(Length(Subtract(InPose.Eye, ExercisePose.Eye)) - Speed * 6 / 60) < .001f;
			ExercisePose = InPose;
			Key(InEvents, false);
			Button(InEvents, 1, false, InX, InY);
			Wheel(InEvents, -1);
			++ExerciseWheelStep;
			break;
		case 3:
			bSpeedVerified &= Dot(Subtract(InPose.Eye, ExercisePose.Eye), InPose.Forward) < -.01f &&
			                  std::abs(Speed - ExerciseSpeed * 1.2f) < .0001f;
			ExercisePose = InPose;
			Button(InEvents, 1, true, InX, InY);
			Wheel(InEvents, -1);
			++ExerciseWheelStep;
			break;
		case 4:
			bSpeedVerified &=
			    std::abs(Speed - ExerciseSpeed) < .0001f && Length(Subtract(InPose.Eye, ExercisePose.Eye)) < .00001f;
			Button(InEvents, 1, false, InX, InY);
			ExerciseStep = 7;
			break;
		default:
			break;
	}
}

void FEditorApplication::ExerciseCamera(std::vector<FInputEvent>& InEvents)
{
	FSceneCameraPose Pose;
	const auto Handle = GetSceneNavigationCamera(*Scene);
	if (Handle)
	{
		Scene->GetCameraPose(*Handle, Pose);
	}
	const float X = (ViewportRegion.Bounds.X + ViewportRegion.Bounds.Z) * .5f;
	const float Y = (ViewportRegion.Bounds.Y + ViewportRegion.Bounds.W) * .5f;
	switch (ExerciseStep)
	{
		case 5:
			ExerciseMovement(InEvents, Pose, X, Y);
			break;
		case 6:
			ExerciseWheel(InEvents, Pose, X, Y);
			break;
		case 7:
			ExercisePose = Pose;
			Move(InEvents, X, Y);
			Button(InEvents, 1, true, X, Y);
			++ExerciseStep;
			break;
		case 8:
			Move(InEvents, X + 36, Y + 20);
			++ExerciseStep;
			break;
		case 9:
			Button(InEvents, 1, false, X + 36, Y + 20);
			bLookVerified = Length(Subtract(Pose.Forward, ExercisePose.Forward)) > .01f &&
			                Length(Subtract(Pose.Eye, ExercisePose.Eye)) < .00001f;
			++ExerciseStep;
			break;
		case 10:
			ExercisePose = Pose;
			Wheel(InEvents, 1);
			++ExerciseStep;
			break;
		case 11:
			bDollyVerified = Length(Subtract(Pose.Eye, ExercisePose.Eye)) > .01f;
			ExerciseClick(InEvents, FileMenuBounds);
			break;
		case 13:
			ExercisePose = Pose;
			ExerciseSpeed = Camera.GetMovementSpeed(*Scene);
			Button(InEvents, 1, true, X, Y);
			Key(InEvents, true);
			Wheel(InEvents, 1);
			ExerciseWait = 0;
			++ExerciseStep;
			break;
		case 14:
			if (++ExerciseWait < 6)
			{
				break;
			}
			Key(InEvents, false);
			Button(InEvents, 1, false, X, Y);
			bInputIsolationVerified = Length(Subtract(Pose.Eye, ExercisePose.Eye)) < .00001f &&
			                          Length(Subtract(Pose.Forward, ExercisePose.Forward)) < .00001f &&
			                          Camera.GetMovementSpeed(*Scene) == ExerciseSpeed;
			++ExerciseStep;
			break;
		default:
			break;
	}
}

void FEditorApplication::ExerciseInput(std::vector<FInputEvent>& InEvents)
{
	if (FrameCount < 3)
	{
		return;
	}
	switch (ExerciseStep)
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
		case 20:
			ExerciseClick(InEvents, OpenButtonBounds);
			break;
		case 4:
			if (ReadyFrames > 8)
			{
				ExerciseClick(InEvents, ViewportRegion.Bounds);
			}
			break;
		case 15:
			ExerciseClick(InEvents, CancelButtonBounds);
			break;
		case 16:
			Window->Resize({1440, 900});
			bShowViewport = false;
			++ExerciseStep;
			ExerciseWait = 0;
			break;
		case 17:
			if (++ExerciseWait < 4)
			{
				break;
			}
			bShowViewport = true;
			ExerciseWait = 0;
			++ExerciseStep;
			break;
		default:
			ExerciseCamera(InEvents);
			break;
	}
}

void FEditorApplication::WriteReport()
{
	if (Options.Report.empty())
	{
		return;
	}
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          DeviceStats = Device->Statistics();
	                          }));
	if (!Options.Report.parent_path().empty())
	{
		std::filesystem::create_directories(Options.Report.parent_path());
	}
	std::ofstream Stream(Options.Report);
	Stream << std::boolalpha << "{\n"
	       << "\"scene\": " << std::quoted(CurrentPath) << ",\n"
	       << "\"open_count\": " << OpenCount << ",\n"
	       << "\"ready_frames\": " << ReadyFrames << ",\n"
	       << "\"nodes\": " << Scene->GetNodes().size() << ",\n"
	       << "\"scene_error\": " << std::quoted(Scene->GetStatus().Error) << ",\n"
	       << "\"failed_models\": " << Scene->GetStatus().FailedModels << ",\n"
	       << "\"load_error_observed\": " << bLoadErrorObserved << ",\n"
	       << "\"draws\": " << RenderStats.MainView().Draws << ",\n"
	       << "\"validation_errors\": " << DeviceStats.ValidationErrors << ",\n"
	       << "\"viewport_width\": " << ViewportSize.Width << ",\n"
	       << "\"viewport_height\": " << ViewportSize.Height << ",\n"
	       << "\"exercise_step\": " << ExerciseStep << ",\n"
	       << "\"movement\": " << bMovementVerified << ",\n"
	       << "\"movement_gate\": " << bMovementGateVerified << ",\n"
	       << "\"right_release\": " << bRightReleaseVerified << ",\n"
	       << "\"look\": " << bLookVerified << ",\n"
	       << "\"dolly\": " << bDollyVerified << ",\n"
	       << "\"wheel_speed\": " << bSpeedVerified << ",\n"
	       << "\"movement_speed\": " << Camera.GetMovementSpeed(*Scene) << ",\n"
	       << "\"input_isolation\": " << bInputIsolationVerified << "\n}\n";
	if (!Stream)
	{
		throw std::runtime_error("Could not write editor report");
	}
}
} // namespace Hyperion
