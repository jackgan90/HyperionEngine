# Validation

Verified on 2026-09-20. Changes remain uncommitted; this change is not archived.

## Build and contracts

- Ninja Debug with RenderDoc ON: build succeeded, including Editor, Viewer and capture/preference tests (`out/EditorCaptureBuild.log`).
- Visual Studio Debug with RenderDoc OFF: Editor, Viewer and preference tests built successfully in the separate `out/build/renderdoc-off` tree (`out/EditorCaptureOffBuild.log`). Generated editor project defines `HYP_ENABLE_RENDERDOC=0` and has no capture/RenderDoc link dependency. Vendor-only warnings remain in the old dependency build.
- Full source formatting and path checks passed (`out/EditorCaptureStyle.log`). Semantic naming passed for all 16 modified/new C++ translation units (`out/EditorCaptureNaming.log`).
- Dependency boundaries, `git diff --check` and strict validation of `editor-renderdoc-preferences` passed.

## Targeted regression

RenderDoc ON: the following ten distinct targeted tests passed across the recorded runs (the full CTest suite was not run):

- `editor_preferences`: defaults, enabled/disabled round trips, malformed/incomplete data and preservation of the old file after replacement failure.
- `editor_capture_ui`: actual menu/checkbox/button input, saved-state restoration across processes, immediate button visibility, explicit disablement, malformed-file recovery, one scene/GUI capture and exact-result replay launch.
- `editor_acceptance`: existing scene/document/view/gizmo/picking interaction regression.
- `plugin_runtime`, `plugin_applications`: lifecycle, missing/disabled providers and empty-host paths.
- `capture_controls`, `capture_runtime`, `cpu_frame_capture`: existing GUI, runtime and frame association contracts.
- `capture_failure_recovery`: existing capture ownership failure cases plus shared-scope exception cancellation and no replay on failure.
- `renderdoc_acceptance`: Triangle, Model and Shadows captures, submitted scene/GUI markers, Present/pipeline evidence, replay, missing runtime and unwritable output.

Logs: `out/EditorCaptureTests.log`, `out/EditorCaptureRegression.log`, `out/EditorCaptureFinalTests.log`, `out/EditorViewerCaptureRetest.log`.

The first `renderdoc_acceptance` run failed because the sample's 720-pixel window clips the capture widget below the diagnostics scroll region at the current 125% scale. `out/ViewerCaptureDiagnostic.png` confirmed the missed GUI target before any capture request. The harness now uses an isolated 1080-pixel-high copy of each sample configuration. The unchanged real click path then passed all capture/replay checks; shipped sample configurations and Viewer UI behavior were not modified.

RenderDoc OFF: `editor_capture_ui`, `editor_preferences` and `plugin_applications` passed (`out/EditorCaptureOffUi.log`, `out/EditorCaptureOffTests.log`). Enabled preference reports build unavailability, the disabled button remains visible, disabling removes it, and persistence works without RenderDoc.

## Visual and GPU evidence

- Inspected `out/EditorPreferencePreview.png`: Edit menu opens the preference modal with the RenderDoc checkbox, status, restart explanation and Close action.
- Inspected `out/EditorCapturePreview.png`: camera icon appears beside the viewport view selector while enabled.
- Actual Editor capture and replay evidence: `out/editor-capture-tests/tmpa90p3q21/capture.log` and `Editor.xml`. The replay-launch path equals the newly saved RDC, XML validates submitted scene and GUI draws plus Present, and native replay succeeds.
- Editor and final graphics shutdown reported zero GPU validation errors.

## Independent quality audit follow-up

An independent reviewer audited the 38-file snapshot against baseline `c6389a22f4aa3433adf9a4a99e2f44954de44028` and confirmed one P2 finding, EC-01: the view selector consumed remaining toolbar width before the capture icon was appended, clipping the new action in a narrow viewport. The main agent independently reproduced this with a 300-pixel viewport and confirmed the cause. No other confirmed defects were reported.

After the initial audit ended, the minimal fix moved the capture icon before the flexible selector. The actual capture acceptance now resizes the editor to 600x960, asserts the button remains within the viewport's horizontal bounds, clicks it, verifies submitted scene/GUI draws and replays the exact new capture. Debug build and `editor_capture_ui` passed; the three changed C++ units passed formatting/naming and module boundary checks. A second screenshot of the same 300-pixel viewport confirms the icon remains visible. Both runs reported zero GPU validation errors.

The original reviewer checked the updated 39-file snapshot, reviewed disabled/unavailable/popup behavior and the regression assertion, inspected before/after images, and closed EC-01 with no new confirmed findings. The reviewer also independently ran `editor_preferences` and `capture_failure_recovery` (2/2) during initial review and revalidated prior Editor XML. No full-suite rerun or new OFF build was performed for this toolbar-order-only fix.

Evidence: `out/EditorCaptureIndependentReview.md`, `out/EditorCaptureIndependentRereview.md`, `out/EditorCaptureAuditSnapshot.json`, `out/EditorCaptureAuditFixedSnapshot.json`, `out/EditorCaptureAuditBuild.log`, `out/EditorCaptureAuditTests.log`, `out/EditorCaptureAuditStyle.log`, `out/EditorCaptureAuditNarrow.png`, `out/EditorCaptureAuditNarrowFixed.png`.

## Usage constraints

RenderDoc must be compiled in and installed. The preference defaults off and is saved at `out/editor/Preferences.ini`; tests use isolated paths. Initial enablement requires restarting so hooks load before device creation. Disabling immediately hides the action while hooks stay loaded until exit. No dynamic plugin loading, automatic restart, user preference rewrite by tests, Git staging or commit was performed.
