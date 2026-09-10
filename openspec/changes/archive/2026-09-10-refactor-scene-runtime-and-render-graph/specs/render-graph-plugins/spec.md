## MODIFIED Requirements

### Requirement: Validated color graph

The engine SHALL validate pass identity, graph-owned resource handles, attachment formats/aspects/dimensions, dependencies, attachment-content initialization and command-context capacity before recording GPU work. Graphics passes SHALL explicitly declare color/depth/stencil attachments, independent Load/Clear/Discard and Store/Discard operations, clear values, render regions and sampled reads. Missing attachments SHALL NOT select implicit swapchain targets.

#### Scenario: Invalid graph
- **WHEN** a graph loads undefined attachment contents, references a foreign handle or contains cyclic dependencies
- **THEN** compilation fails before GPU submission

#### Scenario: Discarded contents
- **WHEN** a resource is discarded and a later pass loads or samples it without a sufficient intervening initialization
- **THEN** compilation rejects the undefined access

#### Scenario: Regional clear
- **WHEN** only a region is cleared before whole-texture sampling
- **THEN** the graph does not treat the complete texture as initialized

### Requirement: Explicit sampled depth dependencies

The graph SHALL derive RAW/WAR/WAW dependencies from explicit attachment and sampled accesses, produce compatible resource transitions and reject undefined or conflicting accesses. It SHALL preserve deterministic ordering without forcing every pass to depend on its predecessor. Imports SHALL declare initial state/content validity, and exports SHALL declare required final state. Native lists SHALL retain attachments and barrier resources through fence completion, including empty draws and failure cleanup.

#### Scenario: Shadow producer and consumer
- **WHEN** several depth-only passes are followed by a forward pass sampling their targets
- **THEN** writes execute before reads with correct barriers and no simultaneous incompatible binding

#### Scenario: Cancelled clear-only frame
- **WHEN** a frame containing a retained offscreen clear is cancelled or fails after submission
- **THEN** resource lifetime follows submission completion and the next valid frame can initialize and sample the target safely

#### Scenario: Independent passes
- **WHEN** two passes access disjoint resources and explicit dependencies require their reordering
- **THEN** the graph accepts an acyclic ordering without an artificial previous-pass dependency

### Requirement: Ordered owned deferred preparation

Graph passes SHALL declare resources, attachments and accesses before deferred preparation. Preparation SHALL own frozen frame data and produce only draw batches for declared passes, preserving attachment and dependency validation. Render primitives and Main mutable objects SHALL NOT be read by RHI preparation. Existing immediate construction SHALL remain supported.

#### Scenario: Mixed immediate and deferred passes
- **WHEN** immediate and owned deferred draws are interleaved
- **THEN** declared pass order and dependencies remain valid, callbacks cannot introduce hidden resource accesses, and invalid graphs fail before frame acquisition

#### Scenario: Segmented scene draws
- **WHEN** a logical pass contains multiple target-view-compatible draw segments
- **THEN** their order is preserved, the load operation occurs once and the store operation occurs after the last segment
