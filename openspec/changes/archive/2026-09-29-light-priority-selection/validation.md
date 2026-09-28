# Validation

Completed on 2026-09-28. The change remains active: no archive, commit or push was performed. HyperionAssets has no working-tree changes.

## Checks

- `tools/Build.ps1 -Preset debug`: all configured Debug targets built successfully.
- `python tools/CheckStyle.py`: owned paths/include casing and source formatting passed.
- `openspec validate light-priority-selection --strict`: passed.
- `git diff --check`: passed.
- 36 distinct affected CTest cases passed across targeted runs and corrective reruns. This was not a full CTest run.

CPU/contracts: `automation_scene`, `editor_state`, `reflected_archives`, `material_contracts`, `material_bindings`, `render_controls`, `scene_management`, `scene_dispatch_failure`, `object_placement`, `native_asset_management`, `native_asset_publication`, `model_fixtures`, `native_model_fixtures`, `shaders`, `dependency_boundaries`, `code_style_paths`.

Renderer: `material_rendering`, `scene_runtime_instance`, `scene_navigation`, `sky_rendering`, `contact_shadows`, `deferred_rendering`, `cascaded_shadow_rendering`, `scene_rendering`, `model_rendering`.

Editor/automation: `editor_asset_workspace`, `gui_input_and_data`, `editor_acceptance`, `automation_capability_parity`, `automation_parity_regressions`, `editor_outlines`, `editor_multiselect`, `editor_clipboard`, `editor_reparent`, `editor_placement`, `editor_render_controls`.

## Behavioral evidence

- Signed priorities, integer extremes, stable ID tie resolution, ancestor disablement, deletion, zero-radiance/shadow eligibility and save/reload are covered by Scene tests.
- GPU tests cover additive differently colored directionals in Forward, Deferred and transparency, with clustering enabled/disabled; no shadow candidate; priority source switches without duplicate illumination; unlit surfaces; unchanged descriptor allocation on stable frames.
- Gated frame publication retains each immutable directional buffer and its original radiance after later edits.
- A selected missing sky remains selected, reports its asset error, and produces no fallback sky. Disabling it exposes the lower-priority valid sky.
- Real Inspector input edits both Priority fields to -7, with undo/redo. The old activation/status block is absent in captured component layouts.
- Automation discovery exposes `scene.lighting.get` and signed priority schemas, omits `light.main.get/set`, and verifies component edits, range rejection, stale revisions, history, persistence and shadow source switching. Contact-shadow provider absence remains covered by Editor render-controls acceptance.

Local evidence is under `out/LightPriorityBuild.log`, `out/LightPriorityCpuTests.log`, `out/LightPriorityIntegration.log`, `out/LightPriorityRetest.log` and `out/LightPriorityFinalTests.log`. Earlier logs retain diagnosed intermediate failures; the last result for each listed case is passing. GUI captures are `out/DirectionalPriority.png` and `out/SkyPriority.png`.

## Sky tooltip acceptance refinement

On 2026-09-28, refined Sky Priority help with a green effective status and a red overridden status naming only the winning object's display name, followed by normal-text advice to increase Priority. Disabled-object explanations remain truthful; persistent-ID tie resolution and structured tie diagnostics are unchanged. Generic property presentation supports semantic tooltip lines without adding domain-specific branches to Gui. Directional and other plain tooltips retain their existing presentation.

- All configured Debug targets rebuilt successfully; final incremental build: `out/LightPriorityTooltipFinalBuild.log`.
- `gui_input_and_data`, `reflected_archives`, `automation_scene` and `editor_render_controls`: all four passed (`out/LightPriorityTooltipTests.log`). Existing Editor acceptance exercises Priority editing/history; this run does not provide screenshots of the new hovered color states.
- `python tools/CheckStyle.py`, strict OpenSpec validation and `git diff --check`: passed.
- The change remains active and uncommitted.

## Independent warning hover and directional parity

On 2026-09-28, gave the highest-priority `[!]` marker its own hover target for both light types, separate from `(?)`. Its tooltip states that multiple eligible lights share the highest Priority without exposing internal IDs. Directional status now uses green for the selected shadow source and red for another eligible light, names the winner without its ID, preserves the distinction between direct lighting and shadows, and suggests increasing Priority. Shadow-ineligible and disabled lights keep their neutral exclusion explanations. The shared Scene selector and shadow-eligibility predicate remain authoritative.

- All configured Debug targets rebuilt successfully (`out/LightPriorityWarningBuild.log`).
- `gui_input_and_data`, `reflected_archives`, `automation_scene`, `editor_render_controls`: four passed (`out/LightPriorityWarningTests.log`). These existing regressions cover GUI editing/history and automation; no new hovered-state screenshots were captured.
- Source formatting, strict OpenSpec validation and `git diff --check` passed. The OpenSpec change remains active; no archive or commit.

## Independent quality audit and LP-01 repair

Reviewed HEAD `f528a95ee39b706264a918bedb9df8ae0c07c8a3` plus all 92 uncommitted paths recorded in `out/LightPriorityAuditSnapshot.json`. The independent reviewer read a frozen workspace with no inherited implementation conversation. One in-scope P2 defect, LP-01, was confirmed by both reviewers: Deferred fullscreen directional buffers and binding sets depended on viewport lifetime instead of the scene-input generation, so repeated light edits accumulated GPU cache objects.

The repair sets the fullscreen lighting resource owner to the frame's scene scope, retaining cluster and graph-read owners. It changes no light-selection, user operation, persistence or plugin lifecycle contract. `CheckDirectionalEditLifetime` repeatedly changes a non-selected directional light and checks bounded live GPU resources with clustering off/on and local lights absent/present, in standard and reversed depth modes. It also retains an old graph across those edits and verifies its original lighting image when executed later.

- Pre-fix regression failed: 32 edits increased live material GPU cache objects from 41 to 105 (`out/LightPriorityAuditBeforeFix.log`).
- Post-fix Deferred regression passed: all six combinations increased by only three objects, including the deliberately retained old frame; the first combination was 41 to 44. Retained images matched (`out/LightPriorityAuditAfterFixDetails.log`).
- All Debug targets rebuilt, including Editor (`out/LightPriorityAuditFinalBuild.log`).
- Main-agent audit baseline: six tests passed (`out/LightPriorityAuditTests.log`). After repair: `deferred_rendering`, `sky_rendering`, `contact_shadows`, and `editor_render_controls` passed (`out/LightPriorityAuditAfterFix.log`, `out/LightPriorityAuditFinalTests.log`).
- Style, boundary and diff checks passed. Independent findings and targeted re-review are recorded in `out/LightPriorityIndependentReview.md` and `out/LightPriorityIndependentRereview.md`; the repair source hashes are in `out/LightPriorityAuditRepairSnapshot.json`.
- Scope limits: no full-suite run, alternative-backend validation, or rendered screenshot verification of the new hover states. Independent review established no other actionable defect. The change remains active and uncommitted.
