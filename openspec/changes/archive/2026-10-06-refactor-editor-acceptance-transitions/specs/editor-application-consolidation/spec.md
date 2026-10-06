## MODIFIED Requirements

### Requirement: Cohesive Editor ownership and isolated acceptance driver
Editor SHALL keep feature lifecycle and typed service registration in its plugin while assigning viewport behavior and pending document transitions to focused private owners. Acceptance scenario methods, states, assertions, fixture identifiers and snapshots SHALL belong to a private acceptance implementation behind an opaque driver. Acceptance execution states SHALL use scenario-specific typed semantic identities and explicit `TransitionTo` destinations; input helpers SHALL report progress without implicitly advancing scenario state. Dispatch, input targeting, capture and completion SHALL use semantic states or explicit scenario-owned mappings rather than numeric execution-step arithmetic or ranges. Actual case indices and timing/sample counters SHALL remain distinct from execution state. Production input, drawing and frame routing SHALL use semantic observations or explicit execution/capture policy rather than inspect scenario state or numeric step ranges. A BUILD_TESTING-disabled Editor SHALL compile and link without acceptance implementation sources or scenario-state storage. Acceptance state metadata SHALL contain no historical execution-step numbers. Reports SHALL omit the obsolete `exercise_step` key and provide boolean `interaction_verified` from the same authoritative interaction-completion predicate used by exit control; reports SHALL emit false when tests or the interaction scenario are disabled. Existing acceptance CLI flags and other report keys/value types SHALL remain compatible. Acceptance consumers SHALL assert semantic completion and observed behavior rather than historical execution-step numbers. Structural refactors SHALL preserve synthetic input timing, readiness conditions, behavioral coverage and engine behavior.

#### Scenario: Test-disabled build
- **WHEN** Editor is configured with BUILD_TESTING disabled
- **THEN** it builds and links without acceptance implementation and reports requested unavailable acceptance behavior as a controlled diagnostic
- **AND** normal finite-frame execution, reports, captures and benchmark behavior remain available, with `interaction_verified` false and no `exercise_step` report key

#### Scenario: Test-enabled lifecycle
- **WHEN** existing Editor acceptance scenarios run with tests enabled, including optional provider absence and shutdown
- **THEN** their input targets, useful coverage, provider lifetime and work-drain behavior remain intact
- **AND** existing behavioral assertions and synthetic input timing retain their meaning, while interaction completion is reported semantically without historical numeric state metadata

#### Scenario: Scenario-owned input progress and timing
- **WHEN** an acceptance scenario or subscenario advances synthetic input or observes a frame boundary
- **THEN** its context owns the relevant input phases, observation counts and settling budgets without borrowing a different scenario's generic wait variable
- **AND** generic typed state holders contain no input counters, numeric action milestones and modulo input phases use named phases, and capture/initialization consumers use semantic observations
- **AND** eligible-update counting, readiness guards, same-frame event ordering, double-click spacing, inherited immediate-click behavior and one-update capture requests preserve their existing meaning

#### Scenario: Pending transition and rendering
- **WHEN** a dirty document transition is confirmed or cancelled while viewport work exists
- **THEN** existing busy/discard decisions, document identity, frame snapshot ownership and GPU retirement behavior remain unchanged

#### Scenario: Production observations and scenario ownership
- **WHEN** an Outliner row, content tile, property or other observed control is drawn, or a frame/capture completes
- **THEN** production supplies semantic observations or owned effect inputs without recognizing fixture IDs, individual case phases or assertion rules
- **AND** the acceptance implementation retains the data needed to exercise and verify the existing behavior

## ADDED Requirements

### Requirement: Maintainable acceptance contexts and development guide
Editor acceptance development SHALL have a maintained source-grounded guide linked from the coding standard, Editor, source-layout, verification and documentation index. The guide SHALL describe state declarations, input/context ownership, helper counting boundaries, dispatch/window routing, capture, completion/report semantics and scenario extension points. Each mutable field SHALL have one semantic responsibility in its owning scenario or operation context. Execution states, input phases, frame budgets, observation counts, case indices and one-shot facts SHALL remain distinct. Finite stages and policies SHALL use named typed options; integer comparisons, ordinal arithmetic or modulo SHALL NOT encode implicit phases. Genuine numeric counts SHALL have explicit units, ownership and reset/advance boundaries. Current documentation SHALL describe current contracts, while historical verification evidence SHALL retain its dated baseline context.

#### Scenario: Add or maintain an acceptance scenario
- **WHEN** a developer adds or changes an acceptance scenario or input helper
- **THEN** the guide identifies its declaration, context, implementation and registration entry points, with relevant validation guidance
- **AND** input progress and waits belong to their scenario or operation, use semantic options and explicit transitions, and do not borrow an unrelated scenario's generic counter
- **AND** changes to timing, capture or completion contracts are reflected in the guide and their consumers

#### Scenario: Review a timing or progress field
- **WHEN** a field participates in state, input, waiting or observation behavior
- **THEN** its type, name and owning context identify its sole role and eligible-update/reset boundaries
- **AND** renaming a shared counter or naming its numeric thresholds does not satisfy the ownership and semantic-role requirements
