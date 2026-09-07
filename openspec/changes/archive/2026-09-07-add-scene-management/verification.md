# Verification — 2026-09-07

Implementation complete; no git commit and no OpenSpec archive performed.

## Builds and checks

| Check | Result | Evidence |
| --- | --- | --- |
| Ninja/MSVC Debug build | Passed | `out/SceneFinalDebugBuild.log` |
| Debug full CTest, RenderDoc ON | 39/39 passed, 76.53 s | `out/SceneFinalDebugTests.log` |
| Ninja/MSVC Release build | Passed | `out/SceneFinalReleaseBuild.log` |
| Release full CTest, RenderDoc OFF | 34/34 passed, 51.05 s | `out/SceneFinalReleaseTests.log` |
| Formatting and semantic naming | Passed, 89 translation units | `out/SceneFinalNaming.log` |
| Module boundaries | Passed, 145 C++ source/header files in 24 modules | `python tools/CheckBoundaries.py` |
| OpenSpec strict | Passed | `openspec validate add-scene-management --strict` |
| Whitespace | Passed | `git diff --check` |
| VS 2022 solution regeneration | Passed; not a VS compile/test run | `out/SceneSolutionGeneration.log` |

Preserved the existing per-preset RenderDoc settings; they explain the different CTest totals. GPU tests ran on NVIDIA GeForce RTX 5080 with the D3D12 debug layer enabled. Existing rendering, upload/shutdown/fence recovery, ModelViewer, GUI and network-offline acceptance remained in the full suites.

## Scene evidence

- Pure Scene tests compare frustum math against an independent eight-corner clip oracle, and exercise snapshots, acknowledgement, stale/foreign handles, owner-thread checks, manifest validation and reflected round-trip.
- BVH tests compare linear and hierarchical results over 4097 groups with deterministic random updates, removal and unknown-to-known transitions. Render collection tests cover rejected groups, stable order, three items per primitive, unknown bounds, separate views and atomic group removal.
- Resource tests cover bounds becoming available while outside the camera and explicit clip-space fallback. The old item reprojection test now explicitly captures unculled inputs; it no longer assumes a view-culled snapshot contains objects rejected for that view.
- Real GPU scene tests cover complete initial transforms, independent shared instances, identical None/Linear/BVH pixels, duplicate-attachment rejection, reattachment and pending/no-frame removal.
- SceneViewer control readbacks cover hide/show, moving outside/back, duplication/removal, frozen rejection view with a different display camera, Fit and adding after an empty scene.
- SceneViewer CLI acceptance renders 514 instances across three modes. The PNG bytes match exactly. A separate asset failure still leaves 514 valid models rendering the same image; malformed instance IDs fail with a diagnostic. GUI captures contain the scene controls/statistics.

| 514-instance fixture mode | Group tests | BVH node visits | Collect calls | Items / draws |
| --- | ---: | ---: | ---: | ---: |
| None | 0 | 0 | 2053 | 2053 |
| Linear | 514 | 0 | 5 | 5 |
| BVH | 2 | 19 | 5 | 5 |

These are deterministic fixture work counts, not a universal speedup claim. Mostly visible/overlapping scenes can visit much of the tree. Topology changes rebuild; movement refits with cost-triggered rebuild.

## Visual verification and usage

The checked-in 78-instance, two-asset scene was rendered through the Release Viewer for 180 frames with `--verify-model`. All 78 models became ready, zero asset failures and zero RHI validation errors were reported. The default selection is ordered near the initial camera target for immediate manipulation.

- Preview: `out/captures/SceneViewer.png` (visually inspected layout and multi-model rendering).
- Log: `out/SceneShowcase.log`.
- Per-mode screenshots and logs: `out/build/debug/scene-acceptance/` and `out/build/release/scene-acceptance/`.
- Usage/architecture/limits: `docs/SceneManagement.md`.

Run from repository root: `./out/build/release/bin/hyperion_viewer.exe --config experiments/Scene.json`.

No general scene parenting, skeletal deformation, LOD, hardware instancing, PVS or occlusion implementation was added. Spatial/CPU visibility boundaries are provided; GPU visibility will require asynchronous RenderGraph/RHI integration.
