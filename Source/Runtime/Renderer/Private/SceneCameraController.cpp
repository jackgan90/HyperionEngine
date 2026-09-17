#include "Hyperion/Renderer/SceneCameraController.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
namespace
{
constexpr float MinimumFlySpeed = .01f;
constexpr float MaximumFlySpeed = 100000.f;

bool IsMovementKey(EKey InKey)
{
	switch (InKey)
	{
		case EKey::W:
		case EKey::S:
		case EKey::A:
		case EKey::D:
		case EKey::Q:
		case EKey::E:
		case EKey::Left:
		case EKey::Right:
		case EKey::Up:
		case EKey::Down:
		case EKey::PageUp:
		case EKey::PageDown:
			return true;
		default:
			return false;
	}
}
} // namespace

FSceneCameraController::FSceneCameraController(ESceneCameraNavigationMode InMode) : Mode(InMode)
{
}

void FSceneCameraController::Reset()
{
	HeldKeys.clear();
	bDragging = false;
}

void FSceneCameraController::SuspendInput(std::span<const FInputEvent> InEvents)
{
	Reset();
	for (const auto& Event : InEvents)
	{
		if (Event.Type == EEventType::Focus)
		{
			bFocused = Event.bDown;
		}
	}
}

float FSceneCameraController::GetMovementSpeed(const FSceneCameraView& InCamera) const
{
	const float SceneSpeed = std::max(1.f, InCamera.Lens.FocusDistance);
	return Mode == ESceneCameraNavigationMode::Fly
	           ? FlyMovementSpeed.value_or(std::clamp(SceneSpeed, MinimumFlySpeed, MaximumFlySpeed))
	           : SceneSpeed;
}

void FSceneCameraController::Input(FSceneCameraView& InCamera, std::span<const FInputEvent> InEvents,
                                   bool bInMouseCaptured, bool bInKeyboardCaptured)
{
	if (Mode == ESceneCameraNavigationMode::Fly && !FlyMovementSpeed)
	{
		const float Speed = GetMovementSpeed(InCamera);
		if (Speed > 0)
		{
			FlyMovementSpeed = Speed;
		}
	}
	if (bInKeyboardCaptured)
	{
		HeldKeys.clear();
	}
	if (bInMouseCaptured)
	{
		bDragging = false;
	}
	for (const auto& Event : InEvents)
	{
		HandleEvent(InCamera, Event, bInMouseCaptured, bInKeyboardCaptured);
	}
}

void FSceneCameraController::HandleEvent(FSceneCameraView& InCamera, const FInputEvent& InEvent, bool bInMouseCaptured,
                                         bool bInKeyboardCaptured)
{
	if (InEvent.Type == EEventType::Focus)
	{
		bFocused = InEvent.bDown;
		if (!bFocused)
		{
			Reset();
		}
	}
	if (InEvent.Type == EEventType::Key && IsMovementKey(InEvent.Key))
	{
		if (!InEvent.bDown)
		{
			HeldKeys.erase(InEvent.Key);
		}
		else if (bFocused && !bInKeyboardCaptured && !InEvent.bRepeat)
		{
			HeldKeys.insert(InEvent.Key);
		}
	}
	if (InEvent.Type == EEventType::MouseButton && InEvent.Button == 1)
	{
		bDragging = InEvent.bDown && bFocused && !bInMouseCaptured;
		LastMouse = {InEvent.X, InEvent.Y};
	}
	if (InEvent.Type == EEventType::MouseMove)
	{
		if (bDragging)
		{
			const float Yaw = -(InEvent.X - LastMouse.X) * .006f;
			const float Pitch = (InEvent.Y - LastMouse.Y) * .006f;
			if (Mode == ESceneCameraNavigationMode::Fly)
			{
				RotateSceneCamera(InCamera, Yaw, Pitch);
			}
			else
			{
				OrbitSceneCamera(InCamera, Yaw, Pitch);
			}
		}
		LastMouse = {InEvent.X, InEvent.Y};
	}
	if (InEvent.Type == EEventType::MouseWheel && bFocused && !bInMouseCaptured)
	{
		HandleWheel(InCamera, InEvent.Y);
	}
}

void FSceneCameraController::HandleWheel(FSceneCameraView& InCamera, float InDelta)
{
	if (!std::isfinite(InDelta))
	{
		return;
	}
	if (Mode == ESceneCameraNavigationMode::Fly && bDragging)
	{
		if (FlyMovementSpeed)
		{
			const double Speed = *FlyMovementSpeed * std::pow(1.2, double(InDelta));
			FlyMovementSpeed = float(std::clamp(Speed, double(MinimumFlySpeed), double(MaximumFlySpeed)));
		}
		return;
	}
	DollySceneCamera(InCamera, std::pow(.85f, InDelta));
}

void FSceneCameraController::Advance(FSceneCameraView& InCamera, float InDeltaSeconds)
{
	if (HeldKeys.empty() || !bFocused || (Mode == ESceneCameraNavigationMode::Fly && !bDragging) ||
	    !std::isfinite(InDeltaSeconds) || InDeltaSeconds <= 0)
	{
		return;
	}
	const auto Axis = [&](EKey InPositive, EKey InPositiveAlias, EKey InNegative, EKey InNegativeAlias)
	{
		return float(HeldKeys.contains(InPositive) || HeldKeys.contains(InPositiveAlias)) -
		       float(HeldKeys.contains(InNegative) || HeldKeys.contains(InNegativeAlias));
	};
	const auto Pose = ExtractScenePose(InCamera.World);
	const auto Direction = Add(Add(ScaleVector(Pose.Right, Axis(EKey::D, EKey::Right, EKey::A, EKey::Left)),
	                               ScaleVector(Pose.Forward, Axis(EKey::W, EKey::Up, EKey::S, EKey::Down))),
	                           {0, Axis(EKey::E, EKey::PageUp, EKey::Q, EKey::PageDown), 0});
	const float Magnitude = Length(Direction);
	if (Magnitude < .00001f)
	{
		return;
	}
	const float Distance = GetMovementSpeed(InCamera) * std::min(InDeltaSeconds, .1f);
	const auto Eye = Add(Pose.Eye, ScaleVector(Direction, Distance / Magnitude));
	InCamera.World = SceneCameraTransform(Eye, Add(Eye, Pose.Forward), Pose.Up);
}

float FSceneCameraController::GetMovementSpeed(const FSceneInstance& InScene) const
{
	FSceneCameraView View;
	return GetSceneCameraView(InScene, View) ? GetMovementSpeed(View) : 0;
}

void FSceneCameraController::Input(FSceneInstance& InScene, std::span<const FInputEvent> InEvents,
                                   bool bInMouseCaptured, bool bInKeyboardCaptured)
{
	FSceneCameraView View;
	const auto Handle = GetSceneNavigationCamera(InScene);
	if (!Handle || !GetSceneCameraView(InScene, View))
	{
		SuspendInput(InEvents);
		return;
	}
	const auto Before = View;
	Input(View, InEvents, bInMouseCaptured, bInKeyboardCaptured);
	if (View.World.Values != Before.World.Values || View.Lens != Before.Lens)
	{
		InScene.SetCameraView(*Handle, View.World, View.Lens);
	}
}

void FSceneCameraController::Advance(FSceneInstance& InScene, float InDeltaSeconds)
{
	FSceneCameraView View;
	const auto Handle = GetSceneNavigationCamera(InScene);
	if (Handle && GetSceneCameraView(InScene, View))
	{
		const auto Before = View.World;
		Advance(View, InDeltaSeconds);
		if (View.World.Values != Before.Values)
		{
			InScene.SetCameraView(*Handle, View.World, View.Lens);
		}
	}
}
} // namespace Hyperion
