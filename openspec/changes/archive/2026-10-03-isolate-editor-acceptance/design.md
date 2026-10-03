## Context

At baseline `f6c202a`, CMake selects the acceptance implementation files only with BUILD_TESTING. However, EditorApplication.h still embeds the driver's full scenario state and declares the scenario methods as members of FEditorPlugin. EditorPanels knows a fixture node ID and individual case flags; EditorFrame checks model-placement assertions and reads outline fixtures/capture paths. Other production code captures test widgets and reads test completion fields for reports.

This follows the completed document-transition ownership change. It is a bounded structural refactor: preserve real operations, acceptance expectations, timing, GUI ordering and public contracts, without treating the audit as a reproduced runtime failure.

## Goals / Non-Goals

**Goals:** isolate scenario methods/state and fixture-specific decisions; keep a small stable driver surface; preserve CLI/report and test-disabled behavior; validate actual compilation/linking and GUI/automation regressions; reduce production header/drawing coupling within the affected scope.

**Non-Goals:** generic testing framework, public instrumentation or automation API, runtime plugins per panel, rewriting scenario semantics or all numeric test steps, changing document/asset/render ownership, discovery isolation repair, new acceptance coverage replacing existing coverage, archive or commit.

## Decisions

1. **Keep the driver as an opaque private boundary.** FEditorAcceptanceDriver owns an incomplete acceptance implementation; only BUILD_TESTING-enabled sources include the scenario state and methods. Move Exercise/Prepare/Check helpers to that implementation and qualify access to the existing Editor operations/state. A narrow friend relationship supports internal fixtures without exposing mutable production state as public API. Keeping Exercise methods on FEditorPlugin or mirroring its entire state into a reference bundle would retain the original coupling and is rejected.
2. **Make production observations describe actual UI semantics.** Use typed identities for fixed controls, with authored node/asset/property identifiers supplied only for dynamic observations. Scenario-specific filtering, fixture IDs, maps and remembered bounds live in the acceptance implementation. Retain genuine production geometry, such as a popup anchor, in its owning GUI code rather than reading a test snapshot. Preserve observation ordering relative to GUI calls.
3. **Separate execution policy and frame effects from cases.** The implementation projects existing fixed-clock, VSync, frame-limit, asset-window wait/layout and capture choices through a named policy or focused hooks. Assertions and test outline-selection overrides run on Main before immutable rendering snapshots cross domains. Ordinary captures, benchmarks, finite-frame runs and reports remain production behavior. No borrowed test state is captured across worker/Render/RHI lifetimes.
4. **Keep report compatibility at the boundary.** Acceptance writes its existing report fields through one format definition; the unavailable implementation supplies default values. Production still owns ordinary scene/performance/output fields. Keys and value types remain unchanged, with no schema or operation migration.
5. **Keep test-disabled linking explicit.** BUILD_TESTING selects the scenario implementation or unavailable driver implementation. Test-disabled builds do not include or allocate scenario snapshots, but parse and reject existing acceptance CLI requests with the current controlled diagnostic. Validate both configuration and executable behavior; an enabled build alone is insufficient evidence.
6. **Split only affected production responsibilities.** Put startup option declarations in their own private header, and extract Outliner drawing from the oversized panel file while replacing observations. New/substantively changed production files/functions follow the 500/100-line guidance. Existing scenario bodies moved without logic changes retain their established steps; do not mechanically rewrite assertions or decompose unrelated scenarios solely because ownership changes.

## Risks / Trade-offs

- Moving member methods can accidentally bind names to the wrong object -> inspect semantic member references, migrate explicit Editor access, compile every selected translation unit and review the diff.
- Capturing widget bounds before/after the wrong GUI item can break realistic input -> preserve call positions and run actual scenario suites rather than only structural checks.
- Synthetic timing and asset-window routing are part of existing acceptance -> keep their exact policy and run input, save, placement, multi-window and capture cases.
- A no-op driver can accidentally remove normal report/capture behavior -> keep production output logic independent and verify finite-frame/report runs with tests disabled.
- Pimpl cleanup can run after production resources retire -> keep the implementation Main-only, preserve admitted task draining in Editor and avoid new asynchronous work or lifetime ownership.
- Acceptance has privileged access to internal Editor fixtures -> confine this access to the private test implementation and enforce source/header/build isolation with focused structural regression checks; do not create a public backdoor.

## Migration Plan

Record the current Release acceptance baseline, create the opaque driver and migrate scenario methods/state, replace production observation and policy branches, then update ownership documentation. Build Debug and Release with tests enabled and a separate test-disabled configuration. Run the existing affected Editor/GUI/automation/absence/shutdown regressions, inspect reports and unavailable diagnostics, and complete format/naming/boundary/size/semantic/spec checks. Record precise coverage and remaining unrelated limitations. Leave the OpenSpec change active and all changes uncommitted.

## Open Questions

None requiring user input; the selected audit item and compatibility constraints define the scope.
