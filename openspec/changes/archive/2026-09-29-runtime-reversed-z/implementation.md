# Runtime reversed-Z implementation evidence

Date: 2026-09-29

## Delivered behavior

- Editor GUI and automation commit one live `FRenderSettings` value through `IRenderSettings`. The next rendered scene or 3D asset preview frame uses that convention; saving remains independent.
- Main rendering captures settings and the view request before Render dispatch. Picking, placement, debug bounds, frozen culling and HUD use the same live value. Frozen projection conversion preserves the original physical frustum even after the browsing camera moves.
- Existing immutable scene/shadow targets, depth-aware caches and GPU fence retirement handle switching. No swapchain recreation, global cache flush or per-switch GPU idle was added.
- All rendered 3D asset tabs receive the current convention, including resumed and reopened windows. Texture preview behavior is unchanged.
- `render.settings.get/set` retain IDs and wire shapes and publish operation version 2. `activeReversedZ` means committed for subsequent frames, not presented. Save and persistent record format remain version 1.

## Regression coverage

- `deferred_rendering`: same session/swapchain, offscreen scene output, both initial conventions and both pipelines, two cycles of 24 switches per pipeline. The first four switches in each cycle compare pixels; subsequent switches do not request readback or GPU idle. Checks include CSM, transparency, instanced draws, Deferred contact/HZB, validation errors, bounded allocations/live materials after settling, and PSO reuse on a stable frame.
- `editor_render_controls`: actual GUI checkbox input, observed rendered convention, no document dirtying, and frozen-frustum preservation after moving the browsing camera.
- `editor_render_acceptance`: operation search/description, version metadata, live toggles in Forward/Deferred, revision checks, invalid/stale candidate rejection, unchanged view/document/file state, explicit save/restart, empty scene and disabled contact-shadow feature.
- `editor_asset_editors`: open sky preview, switch while minimized, resume and reopen with the committed convention; existing window lifetime and dirty-draft checks remain active.
- `editor_acceptance` and `editor_placement`: picking across a toggle during held input and placement under alternating conventions.

## Validation

Environment: Windows, D3D12, NVIDIA GeForce RTX 5080. Builds use repository `debug` and `release` presets.

| Check | Result |
| --- | --- |
| OpenSpec strict validation | Passed |
| Owned filename/include casing and formatting | Passed |
| Semantic naming/local declarations, 20 modified C++ translation units | Passed |
| Module boundaries, 884 source files / 38 modules | Passed |
| Debug full build and final incremental build | Passed |
| Release full build and final incremental build | Passed |
| Debug related regression set, 21 unique tests including fixture setup | Passed across targeted runs and final reruns |
| Release related regression set, 21 tests including fixture setup | 21/21 passed in 116.40 seconds |
| `git diff --check` | Passed |

Related CTest selection:

```text
deferred_rendering|editor_asset_editors|editor_acceptance|editor_placement|
editor_render_acceptance|editor_render_controls|automation_contracts|
automation_scene|automation_capability_parity|plugin_applications|
cascaded_shadow_rendering|contact_shadows|render_controls|sky_rendering|
scene_navigation|depth_conventions|cascaded_shadow_maps|hierarchical_depth
```

CTest also selects `editor_asset_documents`, `model_fixtures` and `native_model_fixtures` as dependencies. GPU/desktop cases run serially.

Local evidence (ignored build outputs):

- `out/RuntimeReversedZTargeted.log`
- `out/RuntimeReversedZDebugTests.log`
- `out/RuntimeReversedZDebugRetest.log`
- `out/RuntimeReversedZDebugFinalBuild.log`
- `out/RuntimeReversedZReleaseBuild.log`
- `out/RuntimeReversedZReleaseFinalBuild.log`
- `out/RuntimeReversedZReleaseTests.log`
- `out/RuntimeReversedZNaming.log`

Initial test runs exposed fixture issues: the new GPU scene needed its ambient parameter and an initialized display attachment, and preview statistics had to be queried on Render. These were corrected; final Debug reruns passed. The combined Editor test initially exceeded its 180-second timeout while a full Release build was running; the unchanged timeout passed on standalone rerun in 131.40 seconds. No timeout or rendering assertion was relaxed.

## Scope and remaining limits

Validation covers the repository's D3D12 backend on the above adapter. Direct callers of swapchain `FrameDepth` retain the creation-time optimized-clear contract; this change targets Editor's offscreen scene/preview depth. First use of a convention may still allocate resources or compile PSOs. No promise of GPU presentation or GPU-failure rollback is added to settings commit.

At implementation completion, the OpenSpec change remained active and uncommitted as requested. The subsequent user-authorized delivery on 2026-09-29 synchronizes the main depth-conventions spec, archives this change and commits the verified scope locally.

Post-archive validation: `openspec validate --all --strict` passed 98/98 specifications; `openspec list --json` reported no active changes. No source code changed during archival, so the completed Debug/Release evidence above remains applicable.

Final diff review confirmed that startup-only depth reads remain only in swapchain initialization and acceptance baselines; live consumers read the committed setting. Exposure mirroring remains consistent with the captured frame settings. The diff is limited to Editor/automation integration, reflected UI metadata, focused tests, documentation and this active change.
