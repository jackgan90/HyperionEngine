# material-system Specification

## Purpose
Define independent CPU material definitions, instances and immutable snapshots, pass-specific surface state, typed edits, capability queries and the bounded scope of the initial material implementation.

## Requirements
### Requirement: Independent material definitions and instances
The engine SHALL provide renderer-independent CPU material definitions, instances and immutable snapshots in Runtime/Materials. Definitions SHALL contain versioned parameter and pass declarations without Model, Scene, native API or GPU payload ownership. Instances SHALL have independent identity and revision and SHALL be shareable across unrelated geometry and models. Materials and Scene SHALL not acquire direct or transitive RHI/Renderer dependencies.

#### Scenario: Material shared across different models
- **WHEN** unrelated models reference the same material instance
- **THEN** they use the same definition and effective material values without requiring common geometry or a fixed texture count

#### Scenario: Independent override
- **WHEN** one object applies a local parameter override or uses a separately created material instance
- **THEN** other users retain their previous effective state and geometry uploads are unchanged

#### Scenario: CPU-only material use
- **WHEN** a program constructs and queries CPU materials and Scene data without a rendering device
- **THEN** the operations succeed without linking RHI, Renderer or a native backend

#### Scenario: Independent CPU resource inputs
- **WHEN** a custom material is given a texture mip chain, read-only buffer source/view or sampler without a Model
- **THEN** Materials stores validated owned immutable CPU values with stable identity/version, and Renderer can prepare them without borrowing caller storage or model-local texture indices

### Requirement: Pass-specific state composition
A material SHALL declare named usage passes with shader references, static compile options, typed state, resource requirements and input contracts. The Renderer SHALL explicitly select a usage and combine its effective state with geometry, target and draw context to produce the pipeline. Geometry layout, draw ranges and attachments SHALL retain their independent owners. A model SHALL NOT override the selected material's depth or binding requirements.

#### Scenario: Textured model with depth disabled
- **WHEN** a model selects a textured pass that disables depth testing and writing
- **THEN** its draw is valid without a depth attachment and uses the selected state

#### Scenario: Multiple material usages
- **WHEN** a definition declares Forward and a custom second usage and a frame selects one
- **THEN** only that usage is resolved and scheduled with its own shader/state; other usages are not implicitly executed

#### Scenario: Incompatible vertex interface
- **WHEN** a geometry is missing an active required vertex attribute or provides an incompatible format/range
- **THEN** material draw preparation fails with the attribute and pass identified before recording

### Requirement: Descriptions distinguish declaration and availability
CPU material descriptions SHALL expose declared parameters, semantics, queue, usage passes and pass state before GPU readiness. Contextual resolution SHALL separately report effective state and Available, Unsupported or Incompatible with reasons. Properties such as alpha-to-coverage SHALL be derived from the selected pass state. HasPass SHALL NOT imply that a renderer schedules it or implements shadows.

#### Scenario: Alpha to coverage on the current renderer
- **WHEN** a material requests alpha-to-coverage with the single-sample target supported by this change
- **THEN** the declaration remains queryable and execution reports an explicit incompatibility rather than silently disabling the request

#### Scenario: Shadow capability query
- **WHEN** a caller queries shadow support for a definition without ShadowCaster or a renderer without shadow scheduling
- **THEN** pass existence and execution availability report their respective unsupported reasons and do not advertise shadow rendering

### Requirement: Transactional typed material edits
Material edits SHALL validate complete parameter types, static options and override permissions before publishing a new revision. Failed edits SHALL leave prior snapshots unchanged. Parameter handles SHALL identify schema version; definition replacement SHALL revalidate overrides and reject stale handles. Frame readers SHALL only access immutable snapshots.

#### Scenario: Definition layout changes
- **WHEN** an instance selects a new definition version and an old parameter handle or incompatible override is applied
- **THEN** the edit is rejected before publication instead of writing into an old byte offset

#### Scenario: Main edits after frame freeze
- **WHEN** an instance changes after its material snapshot was frozen for a frame
- **THEN** that frame keeps the old values and new values enter only a later accepted snapshot

### Requirement: Explicit surface scheduling policy
Material usages SHALL declare Opaque, Masked, Transparent or Overlay scheduling and a documented stable ordering policy. Shader alpha clipping SHALL be part of the pass contract. Blending state SHALL NOT implicitly replace the declared queue or cause per-model transparency sorting.

#### Scenario: Custom blending state
- **WHEN** a custom material enables a non-default blend equation and explicitly selects a queue
- **THEN** its actual blend state is preserved and scheduling follows the selected queue

### Requirement: Bounded material delivery
The initial implementation SHALL provide the executable feature matrix in design D3 and SHALL reject unsupported required resource kinds, shader stages and target features. It SHALL preserve plugin IDs, CLI/config keys, existing build targets and CPU asset serialization. Full shadow rendering, MSAA/resolve, MRT, arbitrary offscreen graphs, UAV/compute and a material editor SHALL NOT be introduced as implicit dependencies of this change.

#### Scenario: Unsupported graphics resource
- **WHEN** a material requires a Cube texture, writable resource or runtime-sized descriptor array
- **THEN** preparation returns an actionable unsupported-feature diagnostic without allocating or submitting a partial draw

#### Scenario: Comparison sampling unsupported
- **WHEN** a material requests comparison sampling with this change's RGBA8-only sampled texture capability
- **THEN** the requested state is describable but reported Unsupported and rejected before recording, without adding a shadow or sampled-depth pipeline

### Requirement: No-op material publication avoids state copies
Validated writes that do not change effective authored state SHALL retain the existing immutable snapshot and revision without first copying the full parameter snapshot. Invalid writes SHALL still fail validation before publication. Engine session inputs SHALL likewise avoid invalidating unchanged Global and Scene data.

#### Scenario: Repeated equal write
- **WHEN** a caller repeatedly writes the same already stored typed value
- **THEN** the snapshot identity and revision remain unchanged and existing frame readers and caches retain their data

#### Scenario: Rejected equal-looking input
- **WHEN** a write uses a stale handle or incompatible type even if some numeric bytes resemble current data
- **THEN** validation rejects the write and the prior snapshot remains unchanged
