## 1. Baseline and primitives

- [x] 1.1 Inventory all execution-step reads/writes, cross-file routes and legacy report consumers; preserve the source baseline for equivalence review.
- [x] 1.2 Introduce test-private typed state/TransitionTo primitives and completion-returning click input without changing input timing.

## 2. Scenario migration

- [x] 2.1 Migrate interaction, asset, document, view, content, import, capture, log and render-control users of the shared ExerciseStep into independent typed states.
- [x] 2.2 Migrate all remaining scenario-specific and nested execution-step fields, retaining genuine case/sample/frame counters.
- [x] 2.3 Replace numeric stage/window/capture/completion predicates and hidden helper transitions; isolate reporting from execution control.

## 3. Verification and delivery

- [x] 3.1 Review transition destinations, branch/loop behavior, event ordering, waits, readiness checks and unchanged assertions against the preserved baseline; confirm no old numeric execution-step mechanism remains.
- [x] 3.2 Complete format, naming, path, size, boundary, diff and strict OpenSpec checks appropriate to the changed files.
- [x] 3.3 Build test-enabled Editor in Debug and Release and run the existing affected acceptance/regression cases; verify test-disabled compilation/linking using the existing configuration.
- [x] 3.4 Record exact commands, results and any limits, complete tasks and leave the active change unarchived and all changes uncommitted.

## 4. Semantic report follow-up

- [x] 4.1 Remove historical numbers, description wrappers and numeric report generation; share the original interaction-completion predicate between exit control and `interaction_verified`.
- [x] 4.2 Replace the sole existing numeric report assertion, review unchanged event/state behavior, and complete style, naming, boundary, residual and strict OpenSpec checks.
- [x] 4.3 Rebuild Debug/Release test-enabled and test-disabled Editor, run existing affected regressions and report checks, record exact evidence, and leave the change unarchived and uncommitted.

## 5. Scenario-owned input and timing

- [x] 5.1 Preserve the current post-report baseline and inventory generic waits, cadence, initialization guards, capture points and inherited timing.
- [x] 5.2 Introduce focused test-private click, frame-observation and cadence primitives; remove input storage from the generic typed state holder and add scenario-owned contexts.
- [x] 5.3 Migrate interaction and child scenarios, asset operations and remaining owners without borrowing another scenario's timing/input data.
- [x] 5.4 Replace numeric text/key phases and modulo cadence with named phases; replace capture and initialization count reads with semantic observations/actions.
- [x] 5.5 Review eligible-update boundaries and event sequences against the fresh baseline; complete residual, format, naming, boundary, diff and strict OpenSpec checks.
- [x] 5.6 Rebuild test-enabled/test-disabled Debug and Release, run existing Editor regressions and boundary/report checks, record evidence, and leave the change unarchived and uncommitted.

## 6. Permanent maintenance documentation

- [x] 6.1 Audit current documentation for obsolete execution-step, input/timing and report descriptions; correct current contracts while retaining clearly identified historical evidence.
- [x] 6.2 Document scenario extension points, typed transitions/options, single-purpose context fields, helper counting boundaries and completion/report ownership; link the guide from coding, Editor, source-layout, verification and index documents.
- [x] 6.3 Verify documentation against final source, check local links/anchors, strict OpenSpec validation and diff, record documentation-only evidence, and leave the change unarchived and uncommitted.

## 7. Independent quality audit and repair

- [x] 7.1 Freeze the complete session scope and obtain independent, no-context reviews of state transitions, input/timing, reporting, boundaries and documentation.
- [x] 7.2 Independently verify findings and apply minimal in-scope repairs while preserving event/update boundaries, existing assertions and test registrations.
- [x] 7.3 Complete appropriate style, naming, build and existing regression checks, update documentation and preserve exact validation evidence.
- [x] 7.4 Obtain targeted re-review from the original reviewers, record dispositions and remaining limits, and leave the change unarchived and uncommitted.
