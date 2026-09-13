#pragma once
#include "Hyperion/Math/Math.h"
#include "Hyperion/Platform/Window.h"
#include <set>

namespace Hyperion
{
class FSceneInstance;

// One controller per viewport. Call on Main; the host supplies GUI capture and frame time.
// No scene ownership: the active enabled camera is resolved for each operation.
class FSceneCameraController
{
public:
	void Input(FSceneInstance& InScene, std::span<const FInputEvent> InEvents, bool bInMouseCaptured,
	           bool bInKeyboardCaptured);
	void Advance(FSceneInstance& InScene, float InDeltaSeconds);
	// Call on viewport deactivation/minimization, scene replacement and shutdown.
	void Reset();

private:
	void HandleEvent(FSceneInstance& InScene, const FInputEvent& InEvent, bool bInMouseCaptured,
	                 bool bInKeyboardCaptured);
	std::set<EKey> HeldKeys;
	FVec2 LastMouse;
	bool bFocused = true;
	bool bDragging{};
};
} // namespace Hyperion
