# editor-application-consolidation Specification

## Purpose
Define Editor as the maintained graphics application and preserve reusable functionality and validation when retiring experimental hosts.
## Requirements
### Requirement: Single maintained graphics application
The build SHALL provide Editor as the normal graphics application and SHALL remove obsolete standalone scene/model inspection applications, plugins and build selection. Native asset loading, shared runtime algorithms and independent AssetTool/Automation applications SHALL remain available.

#### Scenario: Fresh build and IDE startup
- **WHEN** the project is configured from clean build metadata
- **THEN** Editor is the default IDE startup application and no removed plugin target is required to compile or link it

### Requirement: Preserve validation coverage
Useful rendering, resource, input, capture, automation, shutdown and benchmark scenarios SHALL execute through Editor or focused test-only runtime consumers. Removing optional experiment plugins SHALL NOT silently switch the project to a reduced unrelated test suite.

#### Scenario: Optional feature disabled
- **WHEN** an optional feature is disabled at build or startup
- **THEN** unrelated tests remain registered and applicable absence, startup failure and shutdown checks run

### Requirement: Current documentation and tools
Active documentation, specifications and executable tool commands SHALL describe supported Editor and runtime workflows. Obsolete application-specific documentation and launch commands SHALL be removed without relabelling historical measurements as new results.

#### Scenario: Follow a documented rendering command
- **WHEN** a developer follows the supported build, capture or profiling workflow
- **THEN** it uses shipped targets and does not require the removed application

### Requirement: Integrated Editor interaction surfaces
The Edit menu SHALL omit Scene settings, Duplicate primary object and Delete primary, keep children. Existing object-oriented scene role controls and Details hierarchy editing SHALL remain available. Shared duplication and keep-children deletion transactions and Automation operations SHALL remain available; their GUI entry points SHALL be deferred until an Outliner or viewport interaction is designed.

#### Scenario: Open the Edit menu
- **WHEN** a user opens Edit with a loaded scene and selected object
- **THEN** the three removed entries are absent and existing Undo/Redo remain available

#### Scenario: Use the retained editing capabilities
- **WHEN** a user edits through existing scene role or hierarchy controls, or an agent duplicates or removes the primary object while retaining children
- **THEN** the operation continues through the existing shared scene document validation and history

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

### Requirement: Owned document transition rules

Editor SHALL own pending document-open, content-root and application-close decisions in its private document transition domain. Transition targets and progress phases SHALL be explicit; callers SHALL use semantic operations and read-only queries rather than directly mutate coordinated state. GUI and existing automation host operations SHALL retain their shared validation, history, persistence and lifecycle contracts. ContentRootService SHALL remain the final authority for root preparation, dirty/busy/generation checks and participant retirement.

#### Scenario: Cancel while admitted save is pending
- **WHEN** a root or close continuation is cancelled by a button, modal dismissal or the existing application-close operation while admitted saves are pending
- **THEN** those saves may finish against their original documents, but their completion does not switch roots or close the application
- **AND** pending decision presentation and continuation state do not reappear on subsequent polling

#### Scenario: Save failure and retry
- **WHEN** save admission fails, a scene save fails asynchronously, or settled saves leave documents dirty
- **THEN** the owner applies the existing target-specific failure/retry decision without losing the current document, history or dirty state
- **AND** application close status preserves the existing idle, saving, failed and closing string contract

#### Scenario: Close overlaps a root decision
- **WHEN** native application close is requested while a root decision remains pending
- **THEN** both intents remain represented for the existing UI and confirmed discard resolves close before root and scene-open replacement
- **AND** a cancelled or completed close cannot later commit an obsolete root continuation

#### Scenario: Prepared root commit and invalidation
- **WHEN** a root becomes ready through clean state, successful saves or explicit discard
- **THEN** the host revalidates and commits through ContentRootService, waits for accepted saves/edits and preserves its controlled failure and content invalidation behavior
- **AND** repeated polling cannot consume the same root or open action twice

#### Scenario: Existing GUI and automation access
- **WHEN** existing document, root and close workflows are discovered and used through GUI, CLI or MCP
- **THEN** operation IDs, schemas, revisions, errors, close reply completion and provider lifetime remain compatible
- **AND** the transition owner can be exercised without GUI rendering or transport-specific domain branches

### Requirement: Explicit Editor interaction policies
Editor SHALL centrally capture named current interaction facts and expose separate admission policies for camera navigation, picking, placement, gizmos, hierarchy gestures, shortcuts, scene-document busy state and auxiliary-window blocking. Policies SHALL preserve existing entry-specific differences, input cancellation, focus recovery and shared document validation. Decisions SHALL observe state changes made earlier in the same frame.

#### Scenario: Modal or gesture changes within a frame
- **WHEN** a modal, transition or gesture begins or ends before another input entry is evaluated in the same frame
- **THEN** that entry uses current facts and performs its existing suspend, cancel, finish or reset behavior without stale admission

#### Scenario: Distinct consumers observe the same facts
- **WHEN** an ordinary popup, preference dialog, pending reparent gesture, inspector edit or close-save state is active
- **THEN** each consumer follows its independently specified existing policy rather than a universal busy flag
- **AND** ordinary popup blocks shortcuts without itself making the scene document busy, while close-save blocks auxiliary windows

#### Scenario: Shared editing remains authoritative
- **WHEN** GUI or an attached agent requests a scene mutation or document transition
- **THEN** shared document identity, revision, idle, history and persistence checks remain in force with unchanged operation schemas and errors

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
