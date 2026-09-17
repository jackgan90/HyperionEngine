#pragma once
#include "Hyperion/Math/Math.h"
#include "Hyperion/Platform/Window.h"
#include "Hyperion/Scene/SceneCamera.h"
#include <optional>
#include <set>

namespace Hyperion
{
class FSceneInstance;

enum class ESceneCameraNavigationMode
{
	Orbit,
	Fly
};

// One controller per viewport. Call on Main; the host supplies GUI capture and frame time.
// No scene ownership: the active enabled camera is resolved for each operation.
class FSceneCameraController
{
public:
	explicit FSceneCameraController(ESceneCameraNavigationMode InMode = ESceneCameraNavigationMode::Orbit);
	void Input(FSceneInstance& InScene, std::span<const FInputEvent> InEvents, bool bInMouseCaptured,
	           bool bInKeyboardCaptured);
	void Advance(FSceneInstance& InScene, float InDeltaSeconds);
	// Translation speed in scene units per second; zero when no navigation camera exists.
	float GetMovementSpeed(const FSceneInstance& InScene) const;
	void Input(FSceneCameraView& InCamera, std::span<const FInputEvent> InEvents, bool bInMouseCaptured,
	           bool bInKeyboardCaptured);
	void Advance(FSceneCameraView& InCamera, float InDeltaSeconds);
	float GetMovementSpeed(const FSceneCameraView& InCamera) const;
	// Clear input on viewport deactivation/minimization, scene replacement and shutdown; retain fly speed.
	void Reset();
	// Track window focus while navigation is unavailable, without choosing speed from a placeholder view.
	void SuspendInput(std::span<const FInputEvent> InEvents);

private:
	ESceneCameraNavigationMode Mode;
	void HandleEvent(FSceneCameraView& InCamera, const FInputEvent& InEvent, bool bInMouseCaptured,
	                 bool bInKeyboardCaptured);
	void HandleWheel(FSceneCameraView& InCamera, float InDelta);
	std::optional<float> FlyMovementSpeed;
	std::set<EKey> HeldKeys;
	FVec2 LastMouse;
	bool bFocused = true;
	bool bDragging{};
};
} // namespace Hyperion
