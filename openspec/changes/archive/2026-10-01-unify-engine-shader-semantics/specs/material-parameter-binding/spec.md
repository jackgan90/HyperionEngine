## ADDED Requirements

### Requirement: Typed builtin semantic identity and immutable catalogs
Builtin declarations and providers SHALL use generated typed semantic identities internally. Frozen catalogs SHALL be shared; mutable extension catalogs SHALL be snapshotted before use. Existing asset semantic names, targets, override rules and custom namespaces SHALL remain compatible.

#### Scenario: Shared standard definitions
- **WHEN** multiple materials and providers use the standard frozen catalog
- **THEN** they share the same immutable metadata without copying the complete builtin registry

#### Scenario: Custom mutation after publication
- **WHEN** a custom mutable catalog is passed to a material and subsequently edited
- **THEN** the published material retains its original frozen contract

#### Scenario: Existing asset round trip
- **WHEN** an existing material with string semantic and target fields is loaded and saved
- **THEN** its stable names, authored values and behavior are preserved

### Requirement: Automatically discovered builtin shader bindings
Builtin engine resource declarations SHALL produce their semantic bindings from reflection and immutable common/selected owner-local metadata without per-material member-name enumeration. Pre-reflection material-authorable PBR inputs and generic explicit targets SHALL remain supported. Effective scope dependencies and immutable GPU ownership SHALL be preserved.

#### Scenario: Builtin PBR feature discovery
- **WHEN** a PBR variant exposes shadow, environment or clustered-light resources
- **THEN** only the exposed resources contribute engine bindings without PBR factory target lists

#### Scenario: Retained frame during input update
- **WHEN** a typed builtin value changes while a previous frame remains retained
- **THEN** the new frame observes the new value and the previous frame keeps its original values and GPU slices

### Requirement: Schema-bound runtime parameter identity
Prepared overrides and resolved parameter values SHALL use schema-bound handles internally. Typed semantic publication SHALL NOT convert builtin identity to an owned name string. Named custom inputs and persisted asset names SHALL remain supported at boundaries. Fullscreen clearing and setting SHALL use prepared handles.

#### Scenario: Typed input retained through publication
- **WHEN** a builtin value is constructed with an enum or generated parameter structure
- **THEN** its runtime identity remains typed through provider lookup and binding

#### Scenario: Stale handle after definition replacement
- **WHEN** an old schema handle is used against a replacement definition
- **THEN** it is rejected while valid overrides are explicitly rebound to the new schema

#### Scenario: Shared cached constants
- **WHEN** compatible parameters from different prepared schemas share constant storage
- **THEN** reuse requires compatible full layout, mapping, scope and value identities and does not rely on a bare parameter index

#### Scenario: Single-field declaration migration
- **WHEN** an existing builtin uniform is migrated to readable field declarations and an inferred equivalent layout
- **THEN** its typed publication, scopes, stable asset semantic names, override behavior and reflected binding targets remain compatible without introducing shader-name strings into runtime setters
