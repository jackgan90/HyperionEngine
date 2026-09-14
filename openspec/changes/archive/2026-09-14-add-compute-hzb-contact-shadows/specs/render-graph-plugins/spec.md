## MODIFIED Requirements

### Requirement: Ordered owned deferred preparation
Graph passes SHALL declare resources, attachments and accesses before deferred preparation. Preparation SHALL own frozen frame data and produce only draw batches or compute dispatches for declared passes, preserving attachment and dependency validation. Render primitives and Main mutable objects SHALL NOT be read by RHI preparation. Existing immediate construction SHALL remain supported.

#### Scenario: Mixed immediate and deferred passes
- **WHEN** immediate and owned deferred draws or dispatches are interleaved
- **THEN** declared pass order and dependencies remain valid, callbacks cannot introduce hidden resource accesses, and invalid graphs fail before frame acquisition

#### Scenario: Segmented scene draws
- **WHEN** a logical pass contains multiple target-view-compatible draw segments
- **THEN** their order is preserved, the load operation occurs once and the store operation occurs after the last segment

## ADDED Requirements

### Requirement: Compute and subresource accesses
Graph SHALL support compute passes without attachments, texture mip and storage buffer accesses, compatible subresource transitions and dependent UAV ordering. It SHALL reject overlapping incompatible reads/writes, uninitialized consumption, invalid views and hidden packet accesses. Dispatch declarations SHALL explicitly describe write coverage; empty dispatches SHALL NOT initialize outputs.

#### Scenario: Mip generation chain
- **WHEN** each compute pass samples mip N and writes mip N+1 before a graphics consumer
- **THEN** graph compilation accepts disjoint mip views and emits ordered accesses and valid transitions through the final read

#### Scenario: Undefined or overlapping storage use
- **WHEN** a pass reads undefined storage or binds overlapping SRV/UAV views incompatibly
- **THEN** compilation or packet validation rejects the access before native dispatch

### Requirement: Bounded recording batches
Ordered logical passes SHALL be recordable in bounded native batches without requiring one recording context per pass. Per-pass events/timings and resource ownership SHALL survive batching, and graphics/compute state SHALL be established at valid recording boundaries. Existing joined cancellation and submitted-failure retention SHALL apply to every batch.

#### Scenario: More passes than contexts
- **WHEN** a valid mixed graphics/compute graph contains more logical passes than available recording contexts
- **THEN** it executes in graph order using supported recording batches with distinct valid per-pass timings
