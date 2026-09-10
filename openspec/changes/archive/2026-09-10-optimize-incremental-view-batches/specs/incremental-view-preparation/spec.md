## ADDED Requirements

### Requirement: Camera parameters and visible membership have separate reuse boundaries

The Renderer SHALL distinguish current view parameters from the ordered visible membership of static published sources. It SHALL reuse valid item storage and local preparation when exact current visibility preserves membership, while updating view-dependent values and preserving independent snapshots.

#### Scenario: A changed camera keeps the same visible opaque items
- **WHEN** the camera or shadow projection changes but exact visibility and ordering select the same eligible static items under the same scene/resource publications
- **THEN** the Renderer reuses their ordered storage and local preparation without repeating full collection materialization and reports membership reuse

#### Scenario: An item crosses the view boundary
- **WHEN** a static source enters or leaves a view
- **THEN** that view includes or excludes it in the current frame, reports the membership change, and retains valid preparation for unaffected items

#### Scenario: Visibility requires the general path
- **WHEN** transparent depth order, view-dependent custom collection, invalid publication provenance or unready resources prevent a stable visibility proof
- **THEN** the Renderer executes the complete current visibility and preparation path without reusing obsolete sources

### Requirement: Eligible batch updates are local to affected compatible groups

The built-in batch planner SHALL retain bounded compatible groups, stable source identities and independent batch content proofs for eligible reorderable static inputs. A member insertion, removal or local content change SHALL preserve unaffected batch data without shifting all later capacity boundaries. Complete compatibility and current shared values SHALL determine grouping.

#### Scenario: A first middle or last member is replaced
- **WHEN** one visible member is replaced within an eligible group while the other members retain their local proofs
- **THEN** only affected blocks are rebuilt, unaffected blocks retain their content identities, and unchanged members do not repeat complete input/candidate preparation

#### Scenario: Shared values split a formerly compatible group
- **WHEN** current shared constants, resources or overrides no longer preserve compatibility within a retained group
- **THEN** the planner updates grouping or uses the complete planning path before any incompatible members are submitted together

#### Scenario: The incremental proof cannot cover the input
- **WHEN** a custom strategy, ordering barrier, duplicate/unstable identity, unsupported capability, capacity limit or preparation failure invalidates incremental admission
- **THEN** the complete applicable strategy path covers every source exactly once as a draw or explicit failure

### Requirement: Shared updates and batch submission retain current frame semantics

The Renderer SHALL reuse valid shared binding group metadata and batch-level draw admission independently of unrelated membership changes. It SHALL bind current shared/instance values, preserve per-source result semantics, and validate relevant resource publications before reuse.

#### Scenario: An unrelated batch changes membership
- **WHEN** one batch changes while another batch retains its valid local contents under the same resource publication
- **THEN** the unaffected batch reuses its draw admission while receiving current view constants and current source receipts

#### Scenario: A retained shared binding becomes invalid
- **WHEN** input availability, dependency shape or resource values invalidate a cached shared binding group
- **THEN** complete material evaluation and failure handling establish the current result before drawing

#### Scenario: Cascades and the main view have different membership
- **WHEN** a camera moves with directional shadows enabled
- **THEN** each cascade independently updates its own visible casters and projection, including valid casters outside the main camera view

### Requirement: Incremental caches preserve bounded ownership and immutable frames

Incremental caches SHALL have explicit capacity and retirement behavior, avoid extending unrelated source leases, and publish new immutable batch/parameter state before replacing old state. Cache maintenance SHALL preserve source invalidation and existing GPU fence ownership rules.

#### Scenario: An old graph survives source removal or replacement
- **WHEN** a source is removed or its generation/resources change while an earlier graph remains retained
- **THEN** new frames exclude obsolete data, old frames retain their original values, and caches release invalid planning ownership through the existing invalidation/retirement lifecycle

#### Scenario: An incremental view stops being requested
- **WHEN** an incremental view is no longer requested, its sources are removed, and externally retained old graphs release their original snapshots
- **THEN** persistent group metadata does not keep compiled material definitions or their default CPU resource payloads alive, and the normal source retirement path can release those payloads while other views continue

#### Scenario: Visibility churn exceeds retained capacity
- **WHEN** repeated entry and exit exceeds configured item or block capacity
- **THEN** cache usage remains bounded and evicted data follows the correct complete preparation path when needed again

### Requirement: Delivery includes reproducible local-update and frame-performance evidence

The change SHALL record the actual membership delta, prepared inputs, affected blocks, retained draw admission and cache usage alongside frozen-baseline Debug/Release frame comparisons. Validation SHALL include static motion, small orbit, large orbit, fixed-eye rotation, first/middle/last changes, shadow views, source coverage, image correctness and native validation.

#### Scenario: Optimized performance is reported
- **WHEN** implementation is delivered
- **THEN** the evidence identifies binaries and commands, separates profiling runs from normal timing, contains complete ready-scene sample coverage, and reports regressions or remaining limitations without reducing validation or shadow quality

#### Scenario: Capacity split diagnostics survive incremental reuse
- **WHEN** a retained compatible group spans multiple nonempty capacity blocks or loses complete blocks
- **THEN** CapacitySplits reports its current additional nonempty blocks beyond the first, retains that count on unchanged-layout reuse, and decreases when those blocks disappear
