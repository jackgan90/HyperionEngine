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

bool FEditorAcceptanceHarness::ExerciseClick(std::vector<FInputEvent>& InEvents, FVec4 InBounds,
                                             FAcceptanceClick& OutClick, EAcceptanceClickDelay InDelay)
{
	const auto Action = OutClick.Advance(InDelay);
	if (Action == EAcceptanceClickAction::None || InBounds.Z <= InBounds.X || InBounds.W <= InBounds.Y)
	{
		return false;
	}
	const float X = (InBounds.X + InBounds.Z) * .5f;
	const float Y = (InBounds.Y + InBounds.W) * .5f;
	Move(InEvents, X, Y);
	Button(InEvents, 0, Action == EAcceptanceClickAction::Press, X, Y);
	if (Action == EAcceptanceClickAction::Press)
	{
		OutClick.Pressed();
		return false;
	}
	OutClick.Released();
	return true;
}

bool FEditorAcceptanceHarness::ExerciseTextInput(std::vector<FInputEvent>& InEvents, FAcceptanceTextInput& OutInput,
                                                 std::string_view InText)
{
	FInputEvent KeyEvent;
	KeyEvent.Type = EEventType::Key;
	switch (OutInput.Progress.GetState())
	{
		case EAcceptanceTextPhase::SelectAllPress:
			KeyEvent.Key = EKey::A;
			KeyEvent.bDown = true;
			KeyEvent.Modifiers = InputModifiers::Control;
			InEvents.push_back(KeyEvent);
			OutInput.Progress.TransitionTo(EAcceptanceTextPhase::ReleaseAndType);
			return false;
		case EAcceptanceTextPhase::ReleaseAndType:
		{
			KeyEvent.Key = EKey::A;
			InEvents.push_back(KeyEvent);
			FInputEvent Text;
			Text.Type = EEventType::Text;
			Text.Text = InText;
			InEvents.push_back(Text);
			OutInput.Progress.TransitionTo(EAcceptanceTextPhase::AwaitTextCommit);
			return false;
		}
		case EAcceptanceTextPhase::AwaitTextCommit:
			if (!OutInput.TextCommit.Advance())
			{
				return false;
			}
			if (OutInput.Commit == EAcceptanceTextCommit::Enter)
			{
				KeyEvent.Key = EKey::Enter;
				KeyEvent.bDown = true;
				InEvents.push_back(KeyEvent);
				OutInput.Progress.TransitionTo(EAcceptanceTextPhase::ConfirmRelease);
				return false;
			}
			break;
		case EAcceptanceTextPhase::ConfirmRelease:
			KeyEvent.Key = EKey::Enter;
			InEvents.push_back(KeyEvent);
			break;
	}
	OutInput.Progress.TransitionTo(EAcceptanceTextPhase::SelectAllPress);
	OutInput.TextCommit.Restart();
	return true;
}

void FEditorAcceptanceHarness::ExerciseMovement(std::vector<FInputEvent>& InEvents, const FSceneCameraPose& InPose,
                                                float InX, float InY)
{
	switch (Scenario.Movement.Progress.GetState())
	{
		case EMovementState::PressForward:
			Scenario.Movement.Before = InPose;
			Key(InEvents, true);
			Scenario.Movement.GateObservation.Restart();
			Scenario.Movement.Progress.TransitionTo(EMovementState::VerifyMovementGate);
			break;
		case EMovementState::VerifyMovementGate:
			if (!Scenario.Movement.GateObservation.Advance())
			{
				break;
			}
			Scenario.bMovementGateVerified = Length(Subtract(InPose.Eye, Scenario.Movement.Before.Eye)) < .00001f;
			Move(InEvents, InX, InY);
			Button(InEvents, 1, true, InX, InY);
			Scenario.Movement.Before = InPose;
			Scenario.Movement.MovementSample.Restart();
			Scenario.Movement.Progress.TransitionTo(EMovementState::VerifyMovement);
			break;
		case EMovementState::VerifyMovement:
			if (!Scenario.Movement.MovementSample.Advance())
			{
				break;
			}
			Scenario.bMovementVerified = Length(Subtract(InPose.Eye, Scenario.Movement.Before.Eye)) > .01f;
			Button(InEvents, 1, false, InX, InY);
			Scenario.Movement.Before = InPose;
			Scenario.Movement.ReleaseObservation.Restart();
			Scenario.Movement.Progress.TransitionTo(EMovementState::VerifyRightRelease);
			break;
		case EMovementState::VerifyRightRelease:
			if (!Scenario.Movement.ReleaseObservation.Advance())
			{
				break;
			}
			Scenario.bRightReleaseVerified = Length(Subtract(InPose.Eye, Scenario.Movement.Before.Eye)) < .00001f;
			Key(InEvents, false);
			Scenario.Interaction.Progress.TransitionTo(EInteractionState::ExerciseWheel);
			break;
		default:
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseWheel(std::vector<FInputEvent>& InEvents, const FSceneCameraPose& InPose,
                                             float InX, float InY)
{
	const float Speed = Editor.Camera.GetMovementSpeed(Editor.Viewport.ViewCamera);
	switch (Scenario.Wheel.Progress.GetState())
	{
		case EWheelState::IncreaseSpeed:
			Scenario.Wheel.Before = InPose;
			Scenario.Wheel.InitialSpeed = Speed;
			Button(InEvents, 1, true, InX, InY);
			Wheel(InEvents, 1);
			Scenario.Wheel.Progress.TransitionTo(EWheelState::VerifySpeedAndMove);
			break;
		case EWheelState::VerifySpeedAndMove:
			Scenario.bSpeedVerified = std::abs(Speed - Scenario.Wheel.InitialSpeed * 1.2f) < .0001f &&
			                          Length(Subtract(InPose.Eye, Scenario.Wheel.Before.Eye)) < .00001f &&
			                          Length(Subtract(InPose.Forward, Scenario.Wheel.Before.Forward)) < .00001f;
			Key(InEvents, true);
			Scenario.Wheel.MovementSample.Restart();
			Scenario.Wheel.Progress.TransitionTo(EWheelState::DollyBackward);
			break;
		case EWheelState::DollyBackward:
			if (!Scenario.Wheel.MovementSample.Advance())
			{
				break;
			}
			Scenario.bSpeedVerified &= std::abs(Length(Subtract(InPose.Eye, Scenario.Wheel.Before.Eye)) -
			                                    Speed * FWheelAcceptanceContext::MovementSampleFrames / 60) < .001f;
			Scenario.Wheel.Before = InPose;
			Key(InEvents, false);
			Button(InEvents, 1, false, InX, InY);
			Wheel(InEvents, -1);
			Scenario.Interaction.DollyMenuDelay = EAcceptanceClickDelay::Immediate;
			Scenario.Wheel.Progress.TransitionTo(EWheelState::RestoreSpeed);
			break;
		case EWheelState::RestoreSpeed:
			Scenario.bSpeedVerified &= Dot(Subtract(InPose.Eye, Scenario.Wheel.Before.Eye), InPose.Forward) < -.01f &&
			                           std::abs(Speed - Scenario.Wheel.InitialSpeed * 1.2f) < .0001f;
			Scenario.Wheel.Before = InPose;
			Button(InEvents, 1, true, InX, InY);
			Wheel(InEvents, -1);
			Scenario.Wheel.Progress.TransitionTo(EWheelState::VerifyRestoredSpeed);
			break;
		case EWheelState::VerifyRestoredSpeed:
			Scenario.bSpeedVerified &= std::abs(Speed - Scenario.Wheel.InitialSpeed) < .0001f &&
			                           Length(Subtract(InPose.Eye, Scenario.Wheel.Before.Eye)) < .00001f;
			Button(InEvents, 1, false, InX, InY);
			Scenario.Interaction.Progress.TransitionTo(EInteractionState::BeginLook);
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
	switch (Scenario.Interaction.Progress.GetState())
	{
		case EInteractionState::ExerciseMovement:
			ExerciseMovement(InEvents, Pose, X, Y);
			break;
		case EInteractionState::ExerciseWheel:
			ExerciseWheel(InEvents, Pose, X, Y);
			break;
		case EInteractionState::BeginLook:
			Scenario.Interaction.LookBefore = Pose;
			Move(InEvents, X, Y);
			Button(InEvents, 1, true, X, Y);
			Scenario.Interaction.Progress.TransitionTo(EInteractionState::MoveLook);
			break;
		case EInteractionState::MoveLook:
			Move(InEvents, X + 36, Y + 20);
			Scenario.Interaction.Progress.TransitionTo(EInteractionState::VerifyLook);
			break;
		case EInteractionState::VerifyLook:
			Button(InEvents, 1, false, X + 36, Y + 20);
			Scenario.bLookVerified = Length(Subtract(Pose.Forward, Scenario.Interaction.LookBefore.Forward)) > .01f &&
			                         Length(Subtract(Pose.Eye, Scenario.Interaction.LookBefore.Eye)) < .00001f;
			Scenario.Interaction.Progress.TransitionTo(EInteractionState::BeginDolly);
			break;
		case EInteractionState::BeginDolly:
			Scenario.Interaction.DollyBefore = Pose;
			Wheel(InEvents, 1);
			Scenario.Interaction.Progress.TransitionTo(EInteractionState::VerifyDollyAndOpenFileMenu);
			break;
		case EInteractionState::VerifyDollyAndOpenFileMenu:
			Scenario.bDollyVerified = Length(Subtract(Pose.Eye, Scenario.Interaction.DollyBefore.Eye)) > .01f;
			if (ExerciseClick(InEvents, Scenario.FileMenuBounds, Scenario.Interaction.Click,
			                  Scenario.Interaction.DollyMenuDelay))
			{
				Scenario.Interaction.Progress.TransitionTo(EInteractionState::OpenSceneForIsolation);
			}
			break;
		case EInteractionState::BeginIsolatedInput:
			Scenario.Interaction.IsolationBefore = Pose;
			Scenario.Interaction.IsolationSpeed = Editor.Camera.GetMovementSpeed(Editor.Viewport.ViewCamera);
			Button(InEvents, 1, true, X, Y);
			Key(InEvents, true);
			Wheel(InEvents, 1);
			Scenario.Interaction.IsolationObservation.Restart();
			Scenario.Interaction.Progress.TransitionTo(EInteractionState::VerifyInputIsolation);
			break;
		case EInteractionState::VerifyInputIsolation:
			if (!Scenario.Interaction.IsolationObservation.Advance())
			{
				break;
			}
			Key(InEvents, false);
			Button(InEvents, 1, false, X, Y);
			Scenario.bInputIsolationVerified =
			    Length(Subtract(Pose.Eye, Scenario.Interaction.IsolationBefore.Eye)) < .00001f &&
			    Length(Subtract(Pose.Forward, Scenario.Interaction.IsolationBefore.Forward)) < .00001f &&
			    Editor.Camera.GetMovementSpeed(Editor.Viewport.ViewCamera) == Scenario.Interaction.IsolationSpeed;
			Scenario.Interaction.CancelDialogDelay = EAcceptanceClickDelay::Immediate;
			Scenario.Interaction.Progress.TransitionTo(EInteractionState::CancelSceneDialog);
			break;
		default:
			break;
	}
}

void FEditorAcceptanceHarness::RestartInteractionAfterSceneScan()
{
	Scenario.Interaction.Click.RestartDelay();
	Scenario.Interaction.DollyMenuDelay = EAcceptanceClickDelay::Settle;
	Scenario.Interaction.CancelDialogDelay = EAcceptanceClickDelay::Settle;
	switch (Scenario.Interaction.Progress.GetState())
	{
		case EInteractionState::ExerciseMovement:
			switch (Scenario.Movement.Progress.GetState())
			{
				case EMovementState::VerifyMovementGate:
					Scenario.Movement.GateObservation.Restart();
					break;
				case EMovementState::VerifyMovement:
					Scenario.Movement.MovementSample.Restart();
					break;
				case EMovementState::VerifyRightRelease:
					Scenario.Movement.ReleaseObservation.Restart();
					break;
				default:
					break;
			}
			break;
		case EInteractionState::ExerciseWheel:
			if (Scenario.Wheel.Progress.Is(EWheelState::DollyBackward))
			{
				Scenario.Wheel.MovementSample.Restart();
			}
			break;
		case EInteractionState::VerifyInputIsolation:
			Scenario.Interaction.IsolationObservation.Restart();
			break;
		case EInteractionState::AwaitHiddenViewport:
			Scenario.Interaction.HiddenViewportObservation.Restart();
			break;
		default:
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseInput(std::vector<FInputEvent>& InEvents)
{
	if (Editor.bOpenDialog && Editor.Browser->IsScanning())
	{
		RestartInteractionAfterSceneScan();
		return;
	}
	if (Editor.FrameCount < 3)
	{
		return;
	}
	switch (Scenario.Interaction.Progress.GetState())
	{
		case EInteractionState::OpenFileMenu:
		case EInteractionState::ReopenFileMenu:
			if (ExerciseClick(InEvents, Scenario.FileMenuBounds, Scenario.Interaction.Click))
			{
				Scenario.Interaction.Progress.TransitionTo(
				    Scenario.Interaction.Progress.Is(EInteractionState::OpenFileMenu)
				        ? EInteractionState::OpenSceneMenu
				        : EInteractionState::ReopenSceneMenu);
			}
			break;
		case EInteractionState::OpenSceneMenu:
		case EInteractionState::OpenSceneForIsolation:
		case EInteractionState::ReopenSceneMenu:
			if (ExerciseClick(InEvents, Scenario.OpenMenuBounds, Scenario.Interaction.Click))
			{
				Scenario.Interaction.Progress.TransitionTo(
				    Scenario.Interaction.Progress.Is(EInteractionState::OpenSceneMenu) ? EInteractionState::SelectScene
				    : Scenario.Interaction.Progress.Is(EInteractionState::OpenSceneForIsolation)
				        ? EInteractionState::BeginIsolatedInput
				        : EInteractionState::ConfirmReopen);
			}
			break;
		case EInteractionState::SelectScene:
			if (ExerciseClick(InEvents, Scenario.SponzaBounds, Scenario.Interaction.Click))
			{
				Scenario.Interaction.Progress.TransitionTo(EInteractionState::DoubleClickScene);
			}
			break;
		case EInteractionState::DoubleClickScene:
			if (!Scenario.Interaction.Click.IsPressed() && !Editor.bOpenDialog)
			{
				throw std::runtime_error("Single scene click unexpectedly opened the scene");
			}
			if (ExerciseClick(InEvents, Scenario.SponzaBounds, Scenario.Interaction.Click,
			                  EAcceptanceClickDelay::Immediate))
			{
				Scenario.Interaction.Progress.TransitionTo(EInteractionState::AwaitSceneAndFocusViewport);
			}
			break;
		case EInteractionState::ConfirmReopen:
			if (ExerciseClick(InEvents, Scenario.OpenButtonBounds, Scenario.Interaction.Click))
			{
				Scenario.Interaction.Progress.TransitionTo(EInteractionState::AwaitReopenedScene);
			}
			break;
		case EInteractionState::AwaitSceneAndFocusViewport:
			if (Editor.bOpenDialog || !Editor.CurrentPath.ends_with("/Sponza.hasset"))
			{
				throw std::runtime_error("Double scene click did not open the selected scene");
			}
			if (Editor.ReadyFrames > 8)
			{
				if (ExerciseClick(InEvents, Editor.Viewport.ViewportRegion.Bounds, Scenario.Interaction.Click))
				{
					Scenario.Interaction.Progress.TransitionTo(EInteractionState::ExerciseMovement);
				}
			}
			break;
		case EInteractionState::CancelSceneDialog:
			if (ExerciseClick(InEvents, Scenario.CancelButtonBounds, Scenario.Interaction.Click,
			                  Scenario.Interaction.CancelDialogDelay))
			{
				Scenario.Interaction.Progress.TransitionTo(EInteractionState::ResizeAndHideViewport);
			}
			break;
		case EInteractionState::ResizeAndHideViewport:
			Editor.Window->Resize({1440, 900});
			Editor.bShowViewport = false;
			Scenario.Interaction.Progress.TransitionTo(EInteractionState::AwaitHiddenViewport);
			Scenario.Interaction.HiddenViewportObservation.Restart();
			break;
		case EInteractionState::AwaitHiddenViewport:
			if (!Scenario.Interaction.HiddenViewportObservation.Advance())
			{
				break;
			}
			Editor.bShowViewport = true;
			Scenario.Interaction.Progress.TransitionTo(EInteractionState::ReopenFileMenu);
			break;
		default:
			ExerciseCamera(InEvents);
			break;
	}
}

} // namespace Hyperion
