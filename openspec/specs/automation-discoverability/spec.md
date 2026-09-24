# automation-discoverability Specification

## Purpose
Define self-describing automation contracts, semantic values, content and target discovery, and consistent host state so agents can discover and use engine capabilities without reading implementation source.
## Requirements
### Requirement: Consistent document response state
Asset automation SHALL return workspace-owned active state consistently in query and mutation responses.

#### Scenario: Active document mutation and undo
- **WHEN** an active workspace asset is edited or undone through automation
- **THEN** its response SHALL agree with the subsequent asset.info for active state, generation and dirty state

### Requirement: Transitively discoverable reflected contracts
Every persistent record type referenced by an operation schema SHALL be queryable by its advertised stable type ID, including nested container types and recursive records.

#### Scenario: Nested material type
- **WHEN** a client queries a material parameter type ID obtained from a schema
- **THEN** types.describe SHALL return that type without requiring the parent asset schema

### Requirement: Semantic values and metadata
Automation SHALL expose reusable enum labels and field semantics without changing existing numeric wire values, and SHALL support numeric material edits without client bit manipulation through shared domain transactions.

#### Scenario: Numeric value roundtrip
- **WHEN** a client sets a finite material scalar/vector using semantic numbers
- **THEN** readback SHALL report the stored numeric values and one undo SHALL restore the prior material state

#### Scenario: Invalid numeric input
- **WHEN** a numeric edit contains an invalid dimension, out-of-range scalar or fractional integer
- **THEN** the edit SHALL fail without partial mutation or history changes

#### Scenario: Partial enum labels and aliases
- **WHEN** enum metadata labels only some legal values or includes multiple names for one value
- **THEN** schema and wire validation SHALL accept the same legal value set, with exactly one schema alternative per distinct value

### Requirement: Discoverable content candidates
Automation SHALL provide bounded paged mounted-directory discovery including directories, indexed assets, unindexed files and access diagnostics, using the filesystem service shared with GUI consumers.

#### Scenario: Unknown invalid native file
- **WHEN** a mounted directory contains a native file absent from the typed asset index
- **THEN** discovery SHALL return its logical path so a client can request authoritative load diagnostics

#### Scenario: Unsafe or stale directory query
- **WHEN** a query escapes mounted roots or supplies an outdated root generation
- **THEN** it SHALL fail without accessing content outside the allowed mount

### Requirement: Explicit host policy and compact health
Automation SHALL describe whether asset workspace entries retain failures and provide compact host readiness/error information independent of full rendering statistics.

#### Scenario: Host without retained failed entries
- **WHEN** a standalone client queries workspace policy
- **THEN** it SHALL learn that failed opens are reported by jobs and not retained as failed tabs

#### Scenario: Failed model host
- **WHEN** a model host fails loading its scene
- **THEN** compact health SHALL report not-ready and the load error without requiring full render statistics

### Requirement: Identifiable targets and bounded probes
Target discovery SHALL support additive human-readable host mode/label metadata and explicit bounded candidate probes through the existing transport abstraction.

#### Scenario: Stale target candidate
- **WHEN** a client probes a stale candidate
- **THEN** it SHALL receive an advisory failure and SHALL NOT fall back to standalone or create a lasting domain connection

### Requirement: Search and client-visible constraints
Search SHALL include operation descriptions, and bootstrap descriptions SHALL state important limits even when clients omit numeric schema constraints.

#### Scenario: Search by descriptive term
- **WHEN** a query matches words present in an operation description
- **THEN** deterministic paged search SHALL include the operation
