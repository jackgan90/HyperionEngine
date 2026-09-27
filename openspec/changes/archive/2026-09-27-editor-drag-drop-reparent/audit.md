# Quality audit

Date: 2026-09-27. Baseline: `2eec626ffb4699844ab0776808f73fa572af7e0a`.

Reviewed the complete uncommitted reparent change, including tracked and new files: 39 files recorded in `out/ReparentAudit/Before.json`. Scope covered Outliner-only drag sources, synchronized viewport selection, multi-selection, KeepWorld, atomic history/persistence, GUI adapters, Automation and regression coverage. No unrelated pre-existing changes were identified.

A fresh reviewer with no inherited conversation context performed the initial audit. Source files remained frozen during review. The main agent independently read each affected call chain and reran the reproduction probes before accepting the findings. The same reviewer then reviewed the fixes against `out/ReparentAudit/After.json` and confirmed all 39 hashes matched. This report and the verification-summary update were added after source review.

## Confirmed findings and disposition

### RP-01 — P2 — Valid affine parent rejected

- Trigger: parent scale `(0.05, 0.7, 1.3)` and an ordinary source transform.
- Evidence: generic matrix inversion produces bottom-right value `0.99999994`; `IsAffine` requires exactly `1`. The determinant is `0.0454999991`, so the parent is invertible. The shared batch operation rejected the request and left revision/history at `2 -> 2`.
- Main-agent review: confirmed by rerunning the numeric probe and checking the `PrepareReparent -> Inverse -> IsAffine` path. The same pattern existed in the old single-node path; the new batch path inherited it. This was not a new defect in the underlying math library.
- Fix: a private SceneEditing `ReparentInverse` validates the input affine matrix, retains the existing finite/singular inverse checks, then restores the mathematically known affine bottom row. Single-node KeepWorld and batch reparent share it; KeepLocal and public schemas remain unchanged.
- Regression: `AffineReparentRounding` covers the original scale for both APIs, world preservation, affine local matrices, one history entry and undo. Existing singular/cyclic/stale batch rejection tests still pass.
- Independent re-review: the original domain probe, relinked against the fixed library, now succeeds with revision/history `2 -> 3`. Resolved.

### RP-02 — P2 — Filtered Outliner keyboard activation ignored

- Trigger: focus a search-result row through keyboard navigation and activate it with Space.
- Evidence: the GUI returns `Selectable=true` with `IsItemPressed=false`. The changed search branch discarded the Selectable result and routed only mouse presses, so scene selection did not change. The original tree branch was already mouse-only and is not counted as a regression.
- Main-agent review: confirmed by rerunning the FInputEvent probe and comparing the old and new search-result branches.
- Fix: pass Selectable activation into row routing; handle non-mouse activation directly while keeping mouse press/release and active drags under the gesture state machine. This avoids processing a mouse release twice.
- Regression: actual Editor input clicks one filtered row, navigates to the next using Down, activates it with Space, and checks the selected object, absence of a drag payload and unchanged history. Existing multi-selection and drag tests pass.
- Independent re-review: confirmed the mouse and keyboard branches are separated and the final acceptance passed. Resolved.

## Validation and evidence

- Debug Editor and `automation_scene_tests` built successfully: `out/ReparentAudit/Build.log`.
- Passed `automation_scene`, `automation_attachment` and `editor_multiselect`: `out/ReparentAudit/Tests.log`.
- Passed final `editor_reparent` in 13.12 seconds: `out/ReparentAudit/Keyboard.log`. The first keyboard fixture used Tab but did not reach its expected row in the Editor's navigation order; that failed run remains in `Tests.log`. The final fixture uses row-to-row Down navigation and Space activation.
- Full formatting, filename/include casing, module boundaries and `git diff --check` passed. Semantic naming checks passed for all five changed C++ translation units; the final test edit was rechecked. Strict OpenSpec change validation passed.
- Initial probes: `out/ReparentAudit/Probe.cpp`, `Probe.log`, `DomainProbe.cpp`, `DomainProbe.log`. Fixed domain result: `DomainProbeAfter.log`.

Both confirmed findings are fixed and independently re-reviewed. No unresolved confirmed findings remain within this scope. No Release build, manual interactive-window acceptance, large-scene performance measurement or exhaustive ill-conditioned-matrix analysis is claimed. No OpenSpec archive or Git commit was performed.
