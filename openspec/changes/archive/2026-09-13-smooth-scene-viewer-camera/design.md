## Context

SceneViewerControls.cpp calls PanSceneCamera per key-down; SDLWindow discards repeat metadata. Pan projects movement onto the ground plane. Viewer Tick already measures delta and obtains GUI capture before input and scene publication. Camera mutation uses Main-owned SceneInstance APIs.

## Goals / Non-Goals

Goals: continuous six-direction keyboard movement, view-relative forward/strafe, reliable release and interruption, deterministic tests.
Non-goals: replacing mouse orbit with FPS mouse-look, inertia/acceleration, new speed UI, changing ModelViewer or renderer scheduling.

## Decisions

- Platform exposes W/S/D/Q/E and key repeat, GUI translates these engine keys too. Existing enum values remain stable by appending keys.
- Renderer exposes a reusable FSceneCameraController with Input, Advance and Reset, owning a small held-key set plus RMB orbit and wheel handling. It borrows SceneInstance per call and depends publicly on engine Platform input, never on GUI or plugins. SceneViewer delegates navigation and keeps only its editing shortcuts. Input records transitions; AdvanceCamera(delta seconds) runs once after Input and before RenderFrame. This avoids putting time into shared stateless PanSceneCamera or using OS polling in a plugin.
- W/S follow full camera Forward, A/D Right, E/Q world Y. Arrows and Page keys alias these directions, combined aliases do not double speed. Normalize the resulting world vector, with opposing keys cancelling.
- Speed is max(1, FocusDistance) world units/second, preserving useful scene-scale sensitivity without OS-repeat dependence. Clamp positive finite delta to 0.1 seconds; invalid delta does nothing. No inertia gives immediate release response.
- Capture/focus loss/minimization clear held input. Repeat cannot re-arm a cleared key; a fresh press is required. Minimized Viewer explicitly feeds captured input before its early return. Stop clears input.
- Movement edits only the active enabled scene camera through SetCameraView, preserving lens, orientation, focus distance and parent handling. Missing cameras are harmless.

## Risks / Trade-offs

- Long stalls lose movement time intentionally to avoid teleporting.
- GUI capture requires a fresh key press afterward, preventing stuck movement.
- UE-style here describes keyboard direction mapping; existing right-button orbit is preserved and keyboard motion does not require holding RMB.

## Migration Plan

No asset/config migration. Rebuild Viewer and tests. Revert this change to restore event-step behavior.

## Open Questions

None.

## Reuse contract

New editor apps or scene plugins instantiate one controller per viewport, call Input(scene, events, mouseCapture, keyboardCapture), Advance(scene, deltaSeconds) before publication, and Reset on deactivation/scene replacement/shutdown. Existing stateless SceneNavigation operations remain shared. ModelViewer migration is outside this change.

Mouse-button events carry their cursor position so a new drag never starts from stale coordinates. Synthetic benchmark input must supply the same positions as real Platform input; the benchmark now supplies previous/current cursor coordinates without changing its orbit path.
