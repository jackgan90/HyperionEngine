# Verification

Date: 2026-09-13. Configuration: Ninja Debug, existing repository preset.

## Implementation

The root cause was fixed-step PanSceneCamera calls per OS key-down event. The new public Renderer FSceneCameraController owns held input and mouse gestures; SceneViewer delegates to it. Viewer supplies delta once before scene publication and clears input on minimized frames. SDL now carries the new keys, repeat metadata and mouse-button position; GUI maps the new keys through its engine adapter.

The reusable controller covers full-view forward/back, strafe, world elevation, normalized combined movement, arrow/Page aliases, focus/capture/reset, RMB orbit and wheel dolly. It owns no scene and requires no SceneViewer or GUI implementation.

## Checks

- `tools/Build.ps1 -Target scene_viewer_tests`: passed after correcting test matrix comparisons to compare Values arrays. Log: `out/CameraBuild.log`.
- `tools/Build.ps1 -Target hyperion_viewer`: passed. Log: `out/CameraViewerBuild.log`. Executable: `out/build/debug/bin/hyperion_viewer.exe`.
- `scene_viewer_controls`: passed, including existing D3D12 rendered editing/frozen-culling/save tests and new direct controller tests. Log: `out/CameraTests.log`.
- New deterministic coverage: one press followed by event-free frames; equal travel at 30/60/120 Hz; pitched forward/back; all six keys; aliases/opposing/diagonal input; orientation/focus preservation; release, GUI capture, focus and repeat suppression; invalid delta, long-frame clamp, reset and disabled camera; RMB pivot-preserving orbit, drag cancellation and wheel capture.
- `python tools/CheckStyle.py`: filenames/include casing (458 files) and formatting passed.
- Repository `CheckStyle.check_naming`: passed for all 9 changed/new C++ translation units using the Debug compilation database, clang-tidy and boolean clang-query checks. Logs: `out/CameraNaming.log` (8 units), `out/CameraBenchmarkNaming.log` (benchmark unit).
- `python tools/CheckBoundaries.py`: 438 source files / 28 modules passed.
- `git diff --check`: passed.
- `openspec validate smooth-scene-viewer-camera --strict`: passed.
- `model_viewer_acceptance` and `scene_viewer_acceptance` plus their 2 fixture setup tests passed. SceneViewer checks 514 instances, identical culling-mode images, GUI and isolated asset failure; rendered runs reported zero D3D12 validation errors. Log: `out/CameraAcceptance.log`.
- `scene_moving_camera` initially failed its unchanged visible-item range: the benchmark synthesized button-down at zero each frame, which now correctly resets the drag origin and accumulated excessive orbit. Updated synthetic button positions to match the previous/current cursor while preserving the original path. Rebuilt Viewer; final test passed in 14.61 seconds, covering ordinary/batched equal pixels and coverage. Log: `out/CameraMovingRetest.log`. No thresholds were relaxed.

## Scope

No manual physical-keyboard feel assessment or Release/full-suite run is claimed. The initial implementation was delivered without a Git commit or archive; the subsequent user request authorizes archive/spec synchronization and a scoped Git commit. The pre-existing user modification to docs/SceneOwnedCamerasAndLightsFollowup.md is excluded from this delivery.
