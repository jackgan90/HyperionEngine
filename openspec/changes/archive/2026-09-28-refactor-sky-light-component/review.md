# Sky Light implementation review — 2026-09-28

## Scope and method

Reviewed the staged, unstaged and added implementation against this change's proposal, design and five delta specifications. The baseline is commit `34f443877791fd1dfcf70b03638e18a85c5ed565`; the review target is the uncommitted working tree. Existing implementation changes were preserved. No archive, commit or push was performed.

Two fresh independent reviewers covered (1) Reflection, Gui, Editor, SceneEditing and Automation and (2) Scene records, migration, persistence, Renderer and shaders. The primary agent checked each finding against its actual callers before applying bounded fixes. Both reviewers subsequently reviewed the relevant fixes. No outstanding confirmed finding remains in this scope.

## Overall assessment

The implementation follows the proposal's architecture and completes its principal workflows: ordinary Sky Light placement, first-light activation, explicit replacement, shared history, native reference persistence, conditional inspection and consistent environment tint. GUI and automation use the same scene document operations. Scene remains CPU-only; application composition and plugin lifecycle were not expanded. The existing source/reference loading keys preserve resource generations when only tint, intensity or yaw changes.

The initial implementation's weaknesses were reusable GUI edge cases and numeric migration boundaries. Existing successful tests did not exercise the new picker and recursive visibility directly. The review adds real input regressions and strengthens rendering/migration coverage rather than treating the original passing suite as proof of those behaviors.

## Confirmed findings and repairs

All four findings are P2 and were introduced by this change.

| ID | Trigger and impact | Repair and regression |
| --- | --- | --- |
| DG-01 | A conditional record nested inside another inspected record ignored `VisibleWhen`. Multi-selection also failed to hide a field when its nested controller was mixed. The top-level Sky Light inspector was unaffected. | Single-object recursion evaluates the current record's fields. Selection recursion resolves the controller relative to its containing record. `GuiVisibilityTests.cpp` opens real sections and compares single/multi-selection layout, including mixed controllers. |
| DG-02 | A sky reference retained an old path or pinned revision while the asset index offered a moved/republished reference with the same ID. Both combo selection and drop were ignored because identity equality was treated as full reference equality. A failed sky could not be repaired by selecting its current candidate directly. | Keep identity-based selection highlighting, but compare the complete `FAssetRef` when committing an explicit selection. Real drag tests verify path/revision replacement, a single changed interaction, and mixed selection even when the primary reference already equals the candidate. |
| DG-03 | Dropping a compatible reference onto a disabled picker still changed its value and reported an edit. ImGui drop acceptance does not itself enforce disabled state. Reflected read-only persistence remained protected, but the draft could still change; direct callers could receive changed values. | The picker skips drop acceptance while disabled. The regression verifies unchanged value, zero changes and no changed interaction. Incompatible drops are covered separately. |
| SKY-RD-01 | Very large finite degree yaw overflowed during `float * pi / 180`, producing invalid rotations. Legacy finite radian values could overflow their float degree representation or lose direction even before overflow. | Background and IBL share a double-precision degree reduction before conversion. Migration preserves single-turn authored values and reduces larger radian angles through sine/cosine before converting to float degrees. Tests compare legacy directions at `1e8`, `-1e20` and float extremes; GPU tests compare huge degree yaw with equivalent ordinary yaw for background, diffuse and specular in Forward/Deferred and both depth conventions. |

The picker findings were independently reproduced with actual mouse input before repair. The new formal picker regression also failed against the original implementation before passing with the fixes. The pre-repair probe remains under `out/review-domain-gui/PickerProbe.cpp`.

## Additional coverage

- Tint rendering now exercises the `EditNode` path used by shared scene authoring, checks retained sky data and ready state, and compares non-white tint between Forward and Deferred.
- Legacy migration preserves a nonempty pinned sky reference.
- Save As/reopen checks non-default tint and degree yaw as well as the rendered result.
- Image comparisons check matching sizes and finite pixels so NaN cannot silently pass a maximum-error calculation.
- The Gui test executable links Scene to supply the existing scene-defined vector reflection used by generic inspection. Runtime dependencies are unchanged by the review.

## Verification boundaries

Final verification on the repaired working tree:

- `tools/Build.ps1 -Preset debug`: all targets built successfully. Log: `out/SkyLightReviewFinalBuild.log`.
- Selected CTest regressions: **25/25 passed**, 194.01 seconds, including two automatically added content fixtures. Log: `out/SkyLightReviewFinalTests.log`.
- Tested workflows include `gui_input_and_data`, `scene_management`, `reflected_archives`, `sky_rendering`, `deferred_rendering`, `scene_runtime_instance`, `environment_preprocessing`, `automation_scene`, `automation_contracts`, `automation_attachment`, both automation parity suites, `editor_placement`, `editor_multiselect`, `editor_clipboard`, object placement, native/shared publication, plugin runtime/applications, graphics shutdown, style paths and module boundaries.
- `python tools/CheckStyle.py`: filenames/include casing and full source formatting passed (914 owned source files). Log: `out/SkyLightReviewStyle.log`.
- Semantic naming and boolean declaration checks passed for all 11 C++ translation units changed by this review. Log: `out/SkyLightReviewNaming.log`.
- Module boundary check passed (878 source files, 38 modules), `git diff --check HEAD` passed, and `openspec validate refactor-sky-light-component --strict` passed.

The initial sandbox build could not update existing build artifacts because of filesystem permissions. The same authorized build and desktop tests were then run successfully with normal user permissions; this was an environment limitation rather than a code failure.

The review uses Windows Debug and the D3D12 backend. Combo-click repair is supported by direct source review; automated picker input coverage uses drag/drop. Recursive GUI coverage directly tests nested records, not every possible container shape. Existing transparent rendering is exercised, while newly added non-white tint comparisons focus on opaque dielectric/metallic surfaces and background. No new full-suite claim is made from the earlier implementation's 112-test run.

The original verification text overstated direct picker/conditional-inspection coverage; `verification.md` now distinguishes the original surrounding workflows from these new regressions.
