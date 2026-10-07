# editor-acceptance-semantic-targets Specification

## Purpose
Keep Editor acceptance observations and Reparent cases maintainable through shared typed identities and explicit fixture/case definitions, while preserving existing execution, assertion and production-boundary contracts.

## Requirements
### Requirement: Typed acceptance observation targets
Editor acceptance SHALL identify ordinary widgets through the existing typed widget identity and distinguish repeated items through owned stable IDs or authoritative item values. It SHALL represent reflected properties with separate component-type and field-path identities. All writers and consumers of the replaced inspection-bounds map MUST use the shared test-owned key contract without hand-written widget aliases or concatenated component/property keys.

#### Scenario: Ordinary and repeated widget lookup
- **WHEN** a production observation reports a widget, identified item or valued choice
- **THEN** acceptance records and queries the same typed identity without depending on label text or presentation order

#### Scenario: Reflected component and property lookup
- **WHEN** acceptance observes a component header or reflected field
- **THEN** header identity remains distinct from property identity and stored keys own their component/field strings

### Requirement: Preserve observation admission and lifetime
Required bounds queries MUST fail when the target was not observed. Optional and polling queries SHALL preserve existing wait/absence behavior without inserting default map entries. Capture gates and surface clearing SHALL retain their existing behavior across every migrated consumer.

#### Scenario: Control is not drawn yet
- **WHEN** a scenario polls a popup or control before it appears
- **THEN** input does not advance on an empty target and the query does not manufacture an observation

#### Scenario: Selection inspector invalidates bounds
- **WHEN** the existing selection-inspector invalidation condition applies
- **THEN** both widget and property observations are invalidated at that same boundary

### Requirement: Explicit Reparent fixture and case semantics
The existing Reparent acceptance cases SHALL use named case identities and a shared ordered definition for selection, source, target, interruption and expected outcome. Fixture records SHALL group named role, handle, ID and world snapshot. Case indices MAY traverse the definition table but MUST NOT choose behavior through numeric equality or range comparisons. Expected selection size SHALL derive from declared selection roles, and cancellation/rejection/no-op topology SHALL be compared with the case's initial snapshot.

#### Scenario: Mixed model and light selection
- **WHEN** the existing model-and-light case starts a drag
- **THEN** its source, selected roles and expected selection size come from the named case definition rather than case/node numbers

#### Scenario: Interrupted or rejected drag
- **WHEN** an existing cancellation or invalid-target case completes
- **THEN** expected parents retain the captured pre-drag topology without relying on earlier case ordinals

### Requirement: Equivalent test-owned execution
The refactor MUST preserve fixture IDs, case order, production input routes, phase transitions, event ordering, frame budgets, readiness conditions, behavioral assertions, CLI flags, report keys and test registration names. New key/fixture/case helpers SHALL remain test-owned, and production include closures MUST remain independent of test implementation.

#### Scenario: Existing acceptance runs after migration
- **WHEN** existing affected Editor scenarios execute with the new identities and definitions
- **THEN** they exercise the same GUI/domain behavior and retain their assertions and completion/report contracts

#### Scenario: Testing is disabled
- **WHEN** Editor is configured with BUILD_TESTING disabled
- **THEN** no new acceptance helper or scenario is part of the production source/include closure
