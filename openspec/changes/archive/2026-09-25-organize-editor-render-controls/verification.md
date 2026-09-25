# Verification — 2026-09-25

## Delivered behavior

- Removed the old Editor diagnostics implementation, Window menu entry and runtime-only model Details readback section. The latter was never a serialized model component. Read-only primitive diagnostics remain available through the shared Automation service.
- Edit > Render settings contains pipeline/layout and render switches, persistence and the depth restart hint. Viewport contains Exposure, Lit/six named GBuffer modes, independent status/profiling buttons and a categorized Stats popup.
- Left/right HUDs stay inside the viewport, never capture input, wrap text and use a compact font in narrow regions. Camera selection remains accessible in Viewport options when the toolbar is compact. Excess text ends with an ellipsis; category selection controls displayed volume.
- DirectionalLight version 2 has optional Scene-owned directional/contact settings. Version 1 loads without authoring an override. Native save, generic component editing, discovery and undo/redo share the existing SceneEditing path. Point/spot shadow fields are rejected.
- Renderer resolves immutable main-light settings each frame. Authored settings override compatible session defaults; switching back to an unconfigured light restores the original defaults. Disabled contact rendering preserves component data and ordinary rendering.
- Existing rendering operation IDs remain available. HUD/visualizer options use the existing typed viewport operations. Profiling HUD state and collection masks remain independent, including partially enabled CPU categories.

## Validation matrix

| Configuration / check | Result | Evidence under repository root |
| --- | --- | --- |
| Complete Debug build | Passed | `out/RenderControlsFinalBuild.log`, `out/RenderControlsDebugUiBuild.log` |
| Debug full CTest run | 104/105 initially passed; one GUI test expectation corrected below | `out/RenderControlsFullDebugTests.log` (714.85 s) |
| Final Debug affected tests | 5/5 passed: profiling controls, transform gizmo, GUI data, editor multiselect, editor render controls | `out/RenderControlsDebugFinalTests.log` (42.62 s) |
| Release, Triangle/DebugUI/RenderDoc/Tracy OFF | Full build and final Editor rebuild passed | `out/RenderControlsReleaseBuild.log`, `out/RenderControlsReleaseFinalBuild.log`, `out/RenderControlsReleaseUiBuild.log`, `out/RenderControlsReleaseRotationBuild.log` |
| Release CPU regressions | 5/5 passed: GUI data, archives, scene management, automation scene, render controls | `out/RenderControlsReleaseCpuTests.log` (5.03 s) |
| Final Release GPU/startup regressions | 5/5 passed: plugins, multiselect, legacy rendering, new controls, offline startup | `out/RenderControlsReleaseGpuFinalTests.log` (31.20 s) |
| Profile, Tracy ON / RenderDoc OFF | Editor build passed; 4/4 tests passed, including real Tracy capture/reconnect and new profiling GUI input | `out/RenderControlsProfileBuild.log`, `out/RenderControlsProfileUiBuild.log`, `out/RenderControlsProfileTests.log` (48.43 s) |
| Naming and formatting | All 876 owned source files formatted; semantic checks covered 25 affected translation units with final changed units rechecked | `out/RenderControlsStyle.log`, `out/RenderControlsFinalStyle.log`, `out/RenderControlsUiStyle.log`, `out/RenderControlsRotationStyle.log` |
| Module boundaries | Passed: 840 files / 38 modules | `python tools/CheckBoundaries.py` |
| OpenSpec after synchronization/archive | 92/92 strict validations passed | `out/RenderControlsOpenSpecValidation.log` |
| Whitespace / external assets | `git diff --check` passed; HyperionAssets working tree clean | Local Git checks |

The full-run failure was `editor_multiselect`: its synthetic diagonal drag assumed exactly 90 degrees. The added toolbar changes the viewport center's fractional pixel position, while ImGui floors mouse coordinates. The test now calculates the expected angle from those actual rounded endpoints, retaining its original 0.0001 tolerance and exact undo/redo matrix checks. Runtime gizmo code was unchanged. Both Debug and Release multiselect reruns passed. After the final profiling-menu adjustment, affected GUI/profiling tests were rerun; the complete suite was not redundantly repeated.

## End-to-end coverage and visual evidence

`editor_render_controls` drives actual GUI input to toggle both HUDs, open Render settings, create the light override in Details and undo/redo it. It resizes the window to 1000 × 900 at application scale 2, checks HUD/control bounds and clicks the profiling category menu. In Tracy builds it enables only the Frame CPU category, clicks GPU collection and verifies unrelated CPU bits remain unchanged.

CLI/MCP sessions discover nested light schemas; reject point/spot shadow fields and invalid values without revision changes; exercise all seven visualizers, HUD category masks and exposure bounds; and verify transient view changes leave the scene clean. Authored 1024 shadow maps allocate four 1024² D32 maps, while undo or switching to an unconfigured light restores four 2048² maps. Contact activation follows the same main-light selection. Native save/reopen restores the authored fields, including when contact rendering is disabled. GPU validation errors remain zero.

Screenshots were inspected at normal and enlarged scales. Evidence sets include `out/editor-render-controls/acceptance-bh2479m6` (full Debug) and `out/editor-render-controls/acceptance-g2wgowp_` (Tracy, including Stats.png). Convenient copies are `out/RenderControlsHud.png`, `out/RenderControlsSettings.png`, `out/RenderControlsLight.png`, `out/RenderControlsNarrow.png` and `out/RenderControlsStats.png`.

## Delivery state

No Git commit or push. The existing viewer migration/menu cleanup work remains in the worktree. HEAD remains `1942ee6dc4456cf3a34bba1687c3a4db54e5bdd6`. The unrelated `simplify-reflection-declarations` change is untouched. No HyperionAssets content was edited.
