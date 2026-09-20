# Validation

Validated on 2026-09-20 using the Debug Ninja build, Windows/D3D12, NVIDIA GeForce RTX 5080 with the debug layer enabled. No commit or push was made.

## Build and static checks

- Full `tools/Build.ps1 -Preset debug` build passed. Final production-source relink: `out/OutlineMountedFinalBuild.log`; final image-test rebuild: `out/OutlineCoverageBuild.log`.
- `python tools/CheckStyle.py`: all owned filenames, include casing and source formatting passed.
- Semantic naming checked all changed C++ translation units, followed by the final material-path/picking changes and final image assertion changes. Logs: `out/OutlineNamingFinal.log`, `out/OutlineMountedNaming.log`, `out/OutlineCoverageNaming.log`.
- `python tools/CheckBoundaries.py`: passed, including Scene remaining CPU-only and no Runtime dependency on Editor.
- `git diff --check` and `openspec validate add-editor-selection-outlines --strict`: passed.

## Runtime and image checks

The affected 22-test set passed, including automatically selected model fixtures. `out/OutlineRegression.log` records material contracts/bindings/rendering, model-material caching, scene publication and dispatch failure, plugin runtime/applications, shaders, Deferred, outlines, instance batching, scene rendering, lifecycle recovery, GUI texture composition, Editor interaction/outlines/placement, and boundary/style checks.

Final follow-up checks after supporting mounted `/Engine/Shaders/Model.hlsl` paths:

- `editor_acceptance`, `editor_outlines`, and `editor_placement` passed again (`out/OutlineMountedRegression.log`). The real Sponza picking exercise now requires nonempty outline items, zero pending items and zero unsupported materials before advancing. Native PBR assets no longer produce unsupported-outline diagnostics; final graphics validation errors were zero.
- `selection_outlines` passed three consecutive runs after replacing whole-frame equality with per-pixel outline-membership comparison (`out/OutlineFinalCoverage.log`). Scene shading can finish asynchronous initialization between frames; the assertion isolates the outline without accepting shifted or leftover edges.

GPU coverage includes overlapping and fully contained projections; union versus per-object pass counts; duplicate handles/groups; section seams; completely occluded objects; alpha textures, material overrides and section cutoff overrides; exposure changes; Forward/Deferred and reversed depth; mirrored/zero scale; disabled, deleted and expired-generation targets; mismatched publication; viewport resizing; invalid camera; feature absence; unsupported custom shader diagnosis/recovery; and GPU validation.

Editor acceptance exercises existing Outliner/viewport selection, gizmo, camera, document/history, reopening and shutdown paths. The synthetic comparison exercises multi-target requests through the real offscreen viewport and GUI composition, checks mode changes do not dirty history, and rejects explicitly requested output when the Editor plugin is disabled.

## Reviewable outputs

`out/outline-comparison/` contains actual Editor captures: `Union.png`, `PerObject.png`, `UnionAgain.png`, `Occluded.png`, `Smooth.png`, `Cleared.png`, and `Report.json`. Union, per-object and fully occluded captures were visually inspected. Run again with:

```powershell
out/build/debug/bin/hyperion_editor.exe --exercise-outlines out/outline-comparison --layout out/outline-comparison/Layout.ini
```

The normal UI remains single-select. The feature accepts multiple object groups, with Union as the default; per-object work scales with object count. Transparent materials use geometric coverage, and custom shader families must explicitly provide a `SilhouetteMask` pass. This change remains unarchived and uncommitted for review.

## Independent audit correction

The independent audit used baseline `8d4ef8ce05da64b685cf321966612b586cbf5681` and the 46-file SHA256 manifest `out/SelectionOutlineAudit/InitialSnapshot.json`. It confirmed SO-001 (P2): deriving a custom silhouette material discarded reflected parameter declarations while retaining overrides, causing synchronous admission failure. The independent probe is recorded in `out/SelectionOutlineAudit/CustomMaterialProbe.log`.

The added production-path GPU regression reproduced the failure before the fix (`RegressionBeforeFix.log`: `InterfaceNotReady: Pixel:CustomV1.ColorOnly`). The fix derives and caches auxiliary definitions from the complete compiled source interface. It retains ordinary-pass-only parameters as inactive inputs and waits for source reflection when needed. Custom binary mask output is now explicit in the public pass contract and documentation.

The complete Debug build passed (`AfterFixBuild.log`). Material contracts, bindings, rendering, model material caching, instance batching and Editor outlines passed (`AfterFixTests.log`). That run exposed the previous unsupported-material test's fixed-frame readiness assumption; the test now waits for asynchronous completion with a bounded deadline. The final `selection_outlines` run passed (`FinalOutlineTests.log`), including reflected material and object overrides, clipping changes and source-only parameters, with zero GPU validation errors. All referenced logs in this section are under `out/SelectionOutlineAudit/`.

The original independent reviewer closed SO-001 after inspecting the fixed 46-file snapshot and rerunning `selection_outlines` successfully (GPU validation zero). See `IndependentRereview.md`, `IndependentRereviewTests.log` and `FixedSnapshot.json` in the same audit directory. Final formatting, naming (`FinalNaming.log`), module boundaries, strict OpenSpec validation and whitespace checks passed. No confirmed finding remains open within this scope. This audit did not repeat the full pre-audit regression suite, real Sponza acceptance or prolonged resource stress. Source files remain identical to the reviewed snapshot; this final evidence paragraph was appended after review. HEAD and the empty index remain unchanged.

## Authorized delivery

After the implementation and audit checkpoints above, the user requested OpenSpec archive followed by a Git commit. On 2026-09-20, all eight tasks and all artifacts were complete; the five requirements were synchronized into `openspec/specs/selection-outlines/spec.md`, and the change was archived as `2026-09-20-add-editor-selection-outlines`. This subsequent authorization supersedes the earlier unarchived/uncommitted review checkpoint. Production code and tests remain identical to the independently reviewed snapshot.
