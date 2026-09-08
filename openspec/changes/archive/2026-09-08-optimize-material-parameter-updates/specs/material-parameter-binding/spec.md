## ADDED Requirements

### Requirement: Incremental engine parameter preparation
Renderer SHALL share immutable unchanged inputs and resolved data and SHALL refresh only parameters and constant blocks affected by their effective dependencies. This SHALL apply to arbitrary materials and all supported scopes without application or builtin shader specialization. Frame, Pass and Draw dependencies SHALL NOT alone force complete material re-resolution. Mixed dependencies, overrides, defaults and required inputs SHALL retain existing semantics.

#### Scenario: Shared view over many objects
- **WHEN** a view changes while object and material inputs remain unchanged
- **THEN** view results are shared, unchanged blocks retain their slices without repeated full content lookup, and no per-draw deep copy of the shared view inputs occurs

#### Scenario: Frame and mixed parameters
- **WHEN** a custom shader consumes Frame time, View camera, Object state and a mixed View/Object block
- **THEN** each dependency updates at its proper boundary and changing Frame time does not re-resolve unrelated static parameters or replace unchanged resources

#### Scenario: Default and resource transition
- **WHEN** a provider disappears, returns, or changes a resource while an override is cleared
- **THEN** effective dependencies and values are recalculated correctly, failed preparation preserves old frozen data, and later valid inputs recover

#### Scenario: Draw identity across primitives
- **WHEN** distinct primitives emit draws with identical inputs and the same primitive-local ordinal, with or without stable local item IDs
- **THEN** their Draw scope keys remain distinct so custom providers observing Draw identity cannot be incorrectly reused across them

### Requirement: Bounded parameter cache history
Engine parameter and constant caches SHALL have explicit retention and capacity policies, including a limit on history retained for a long-lived scope. Eviction SHALL release cache references without invalidating externally retained immutable results or submitted GPU work. Statistics SHALL distinguish entries, retained storage, eviction and actual upload.

#### Scenario: Same live scope with changing values
- **WHEN** a direct cache client continually changes effective values under the same valid scope key and lifetime
- **THEN** results remain distinct and correct while cache-owned history stays within its configured budget and released GPU pages can retire

#### Scenario: Old frame during eviction
- **WHEN** cache pressure evicts records whose slices are still referenced by an earlier frame
- **THEN** that frame still renders its original bytes and storage is recycled only after all required CPU/GPU references finish

#### Scenario: Recurring collection exceeds capacity
- **WHEN** a stable ordered collection exceeds the per-primitive evaluation cache capacity
- **THEN** already admitted items retain reuse across frames rather than every item being evicted by the recurring scan, and later working-set changes can replace cold entries

#### Scenario: Default capacity under continuous updates
- **WHEN** changing inputs under live owners continuously require eviction from a saturated provider or constant candidate cache
- **THEN** selecting each eviction victim does not require traversing every resident entry and capacity accounting remains correct across hits, replacements and collection

## MODIFIED Requirements

### Requirement: Compatible uniform blocks share GPU storage
The Renderer SHALL provide versioned standard block layouts and arbitrary reflected block packing. Storage reuse SHALL require device, target ABI/layout, logical mapping and effective packed value compatibility. General cache callers SHALL validate complete effective dependency identities/versions and current payload contents. The engine's trusted immutable preparation path MAY reuse unchanged value identities across scope revision changes after validating current dependency ownership and immutable program compatibility. Compatible View and Material blocks SHALL reuse actual GPU buffer slices across draws. Arbitrary mixed cbuffers SHALL be packed as whole blocks without claiming automatic splitting.

#### Scenario: Many objects in one view
- **WHEN** multiple objects use compatible standard View blocks and share a material snapshot
- **THEN** they bind the same View buffer identity and offset and the same compatible Material slice, while independent Object data remains separate

#### Scenario: Equal semantics but different layout
- **WHEN** two shaders store a semantic at different offsets or use incompatible matrix/array layouts
- **THEN** values are packed correctly into distinct compatible-layout storage rather than sharing incompatible bytes

#### Scenario: New scope revision with unchanged block values
- **WHEN** trusted immutable preparation observes a new valid scope revision while a compatible block retains all of its effective value identities
- **THEN** the unchanged block may keep its published immutable slice while blocks with changed effective values are prepared for the new data

### Requirement: Uniform allocation lifetime and observable reuse
Constant pages and slices SHALL obey backend alignment/range limits and SHALL not overwrite published data while frames or GPU work retain it. Changes to effective packed values SHALL publish new immutable slices unless compatible storage already contains those exact values; unchanged compatible blocks MAY retain existing slices across scope revision changes through trusted immutable preparation. Retirement SHALL progress without another frame and retain only a bounded idle-page reserve. Statistics SHALL expose per-scope evaluation/packing/upload and storage reuse for deterministic tests.

#### Scenario: Update during a gated GPU frame
- **WHEN** changed parameter values are uploaded while an earlier frame's fence is blocked
- **THEN** the earlier frame retains its original slice and bytes until completion, and both values can retire safely
