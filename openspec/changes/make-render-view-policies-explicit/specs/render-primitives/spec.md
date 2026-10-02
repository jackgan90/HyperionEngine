## ADDED Requirements

### Requirement: Explicit view participation policy

Render views SHALL carry owned values that independently control transient scene-item additions, transient persistent-primitive replacement filtering and runtime statistics classification. Material Usage SHALL remain an open pass selector and SHALL NOT implicitly determine these behaviors. Built-in shadow views SHALL preserve replacement filtering while excluding transient additions.

#### Scenario: Custom shadow usage
- **WHEN** a view selects a custom material Usage with the explicit shadow policy
- **THEN** replacement primitives are removed, transient SceneItems are not added and live statistics classify the view as shadow

#### Scenario: Independent participation flags
- **WHEN** addition and replacement policy values are varied independently
- **THEN** collection performs exactly the enabled effects without registering transient items persistently

### Requirement: Policy-aware retained view snapshots

Retained collection and preparation SHALL compare policy inputs that affect their outputs. Runtime statistics SHALL capture classification by value in immediate and deferred results. Previously queued snapshots SHALL remain immutable, and existing diagnostics wire shapes and family-wide compatibility totals SHALL remain unchanged.

#### Scenario: Policy-only change
- **WHEN** a view keeps its identity, usage and revisions but changes transient policy
- **THEN** incompatible membership/preparation is refreshed, earlier retained snapshots remain unchanged and subsequent unchanged frames resume normal reuse

#### Scenario: Statistics-only change
- **WHEN** only statistics classification changes
- **THEN** current live aggregates use the new classification without changing selected material passes or unnecessarily rebuilding material contents
