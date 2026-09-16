#pragma once
#include "Hyperion/Math/Math.h"
#include "Hyperion/Platform/Window.h"
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
	// Clear input on viewport deactivation/minimization, scene replacement and shutdown; retain fly speed.
	void Reset();

private:
	ESceneCameraNavigationMode Mode;
	void HandleEvent(FSceneInstance& InScene, const FInputEvent& InEvent, bool bInMouseCaptured,
	                 bool bInKeyboardCaptured);
	void HandleWheel(FSceneInstance& InScene, float InDelta);
	std::optional<float> FlyMovementSpeed;
	std::set<EKey> HeldKeys;
	FVec2 LastMouse;
	bool bFocused = true;
	bool bDragging{};
};
} // namespace Hyperion
