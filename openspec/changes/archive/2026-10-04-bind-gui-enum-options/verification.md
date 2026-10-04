# Verification — explicit GUI choices

Baseline: `05d572cce5dd9ceb43848a86c132ee91f604dcd5`. Scope covers this change and its companion shadow/key refactors, validated on 2026-10-04.

## Contract coverage

- `FPropertyChoice` carries the exact archive value separately from the display label. Sparse signed values, reordered choices, duplicate labels, unknown values and signed/unsigned identity are covered in `InspectionTests.cpp`.
- `GuiChoiceTests.cpp` drives actual ordinary and mixed controls, reversed presentation and read-only cases. The fixture's initial click landed in popup padding; correcting the coordinate preserved every assertion and required no production change.
- `MaterialChoiceTests.cpp` checks reversed sampler option mappings and existing display labels against Materials enum declarations. Editor asset and multiselect acceptance tests exercise persistence and history paths.

## Validation

Debug and Release full builds passed. All 32 selected Debug regression tests passed (`out/TypedMaintenanceDebugTests.log`), including the Debug-only cache-allocation failure test.
The selected Release suite covered 31 tests: 30 passed initially, and `gui_input_and_data` passed after its fixture correction. Logs: `out/TypedMaintenanceReleaseTests.log` and `out/TypedMaintenanceGuiReleaseRetest.log`. Both configurations rebuilt the final GUI target.

Full source formatting/path checks, semantic naming on all 26 changed C++ translation units, module boundaries and `git diff --check` passed. Logs include `out/TypedMaintenanceNaming.log`; strict OpenSpec validation passed for all three active changes.

## Independent review and delivery

A reviewer with no inherited conversation context inspected the complete diff and direct consumers, then independently ran the corrected Release GUI suite. Finding TM-R1 (missing new test dependencies in `hyperion_check`) was confirmed, minimally fixed and reviewed as closed; both generated build graphs include the two executables. No unresolved confirmed findings remain.

Implementation and review were completed without archive or commit. Following acceptance, the user authorized spec synchronization, archive and a local Git commit on 2026-10-04. No push is included in this delivery.
