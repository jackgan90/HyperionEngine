## Why

At the change baseline, Editor acceptance scenarios encoded actions, routing and completion through integer steps and implicit increments in input helpers. Reading or changing a scenario required reconstructing numeric ranges and cross-file jumps. This change replaces that representation while preserving the existing acceptance behavior, and records the resulting maintenance contract for future scenarios.

## What Changes

- Replace all Editor acceptance execution-step fields, including scenario-specific and nested progress fields, with named, scenario-specific states and explicit `TransitionTo` calls.
- Separate click/input progress from scenario transitions; callers name the destination when an input action finishes.
- Remove generic shared `Input.Wait` storage. Scenario and subscenario contexts own click gestures, observations, settling delays and input cadence; numeric input phases become named states and capture requests become explicit observations.
- Express stage dispatch, window targeting, capture points and completion through semantic states or authoritative state metadata.
- Preserve synthetic input timing, readiness conditions, behavioral assertions, CLI flags and test-disabled ownership.
- Retire the obsolete numeric `exercise_step` report field and all historical state-number metadata. Replace its sole existing test consumer with `interaction_verified`, derived from the same interaction-completion predicate used by the harness.
- Correct obsolete current documentation and publish the scenario-maintenance guide and semantic-field rules, including source entry points, typed options, timing boundaries and report completion. Update current specification text without archiving the change; historical records retain their original baseline context.
- Run the existing builds and regression cases. Add no features or test cases; leave the change active and uncommitted.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `editor-application-consolidation`: Specify explicit typed transitions, scenario-owned input/timing, semantic report completion and the maintained development guide while retaining behavioral compatibility.

## Impact

Affected code is the Editor test-only acceptance implementation and its private report boundary. The existing Editor acceptance assertion is updated to read semantic completion instead of a historical step number; no test case or registration is added. The report retires `exercise_step` and exposes `interaction_verified`; other report fields stay compatible. No public Runtime or Automation APIs, plugin selection, dependencies or production domain behavior change.

Permanent documentation is maintained in `docs/EditorAcceptance.md`, linked from the coding standard, Editor, source-layout, verification and documentation index. Current OpenSpec contracts describe the implemented state/input/report model; this synchronization does not archive or commit the change.
