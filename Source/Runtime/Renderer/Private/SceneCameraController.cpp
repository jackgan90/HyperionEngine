#include "Hyperion/Renderer/SceneCameraController.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
namespace
{
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

void FSceneCameraController::Reset()
{
	HeldKeys.clear();
	bDragging = false;
}

void FSceneCameraController::Input(FSceneInstance& InScene, std::span<const FInputEvent> InEvents,
                                   bool bInMouseCaptured, bool bInKeyboardCaptured)
{
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
		HandleEvent(InScene, Event, bInMouseCaptured, bInKeyboardCaptured);
	}
}

void FSceneCameraController::HandleEvent(FSceneInstance& InScene, const FInputEvent& InEvent, bool bInMouseCaptured,
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
			OrbitSceneCamera(InScene, -(InEvent.X - LastMouse.X) * .006f, (InEvent.Y - LastMouse.Y) * .006f);
		}
		LastMouse = {InEvent.X, InEvent.Y};
	}
	if (InEvent.Type == EEventType::MouseWheel && bFocused && !bInMouseCaptured)
	{
		DollySceneCamera(InScene, std::pow(.85f, InEvent.Y));
	}
}

void FSceneCameraController::Advance(FSceneInstance& InScene, float InDeltaSeconds)
{
	if (HeldKeys.empty() || !bFocused || !std::isfinite(InDeltaSeconds) || InDeltaSeconds <= 0)
	{
		return;
	}
	const auto Handle = GetSceneNavigationCamera(InScene);
	if (!Handle)
	{
		return;
	}
	const auto Axis = [&](EKey InPositive, EKey InPositiveAlias, EKey InNegative, EKey InNegativeAlias)
	{
		return float(HeldKeys.contains(InPositive) || HeldKeys.contains(InPositiveAlias)) -
		       float(HeldKeys.contains(InNegative) || HeldKeys.contains(InNegativeAlias));
	};
	FSceneCameraPose Pose;
	InScene.GetCameraPose(*Handle, Pose);
	const auto Direction = Add(Add(ScaleVector(Pose.Right, Axis(EKey::D, EKey::Right, EKey::A, EKey::Left)),
	                               ScaleVector(Pose.Forward, Axis(EKey::W, EKey::Up, EKey::S, EKey::Down))),
	                           {0, Axis(EKey::E, EKey::PageUp, EKey::Q, EKey::PageDown), 0});
	const float Magnitude = Length(Direction);
	if (Magnitude < .00001f)
	{
		return;
	}
	const auto Camera = *InScene.FindNode(*Handle)->Camera;
	const float Distance = std::max(1.f, Camera.FocusDistance) * std::min(InDeltaSeconds, .1f);
	const auto Eye = Add(Pose.Eye, ScaleVector(Direction, Distance / Magnitude));
	InScene.SetCameraView(*Handle, SceneCameraTransform(Eye, Add(Eye, Pose.Forward), Pose.Up), Camera);
}
} // namespace Hyperion
