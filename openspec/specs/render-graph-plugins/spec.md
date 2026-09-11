# render-graph-plugins Specification

## Purpose
Define validated graph execution, explicit plugin execution domains and scene-wide pass organization independent of concrete geometry producers.
## Requirements
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

### Requirement: Threaded graph execution
The engine SHALL apply scene updates and collect owned frame descriptions on Render, prepare graph passes on Render, perform native resource operations and packet materialization on RHI 0, record disjoint contexts on indexed RHI threads and submit them in graph order. RHI work SHALL NOT dereference Main objects or render primitives.

#### Scenario: Triangle frame
- **WHEN** a clear pass and triangle pass are compiled
- **THEN** their command lists produce a triangle over the configured background with valid presentation transitions.

#### Scenario: Frozen scene frame
- **WHEN** Main changes its objects after a scene frame snapshot has been submitted
- **THEN** Render and RHI finish that frame using owned snapshot data and resource references

### Requirement: Configurable rendering plugins
The engine SHALL activate rendering plugins using configuration IDs and use engine wrappers for their math, shaders and graphics work. Scene objects supplied by ModelViewer and Triangle SHALL enter through the generic render primitive registration and collection path. Plugin Main, Render and RHI responsibilities SHALL have explicit execution domains. GUI and non-scene work SHALL retain the standard pass extension path.

#### Scenario: Plugin disabled
- **WHEN** no rendering plugins are requested
- **THEN** the Viewer presents only the configured clear color.

#### Scenario: Two geometry producers
- **WHEN** ModelViewer or Triangle supplies scene geometry
- **THEN** the same Runtime primitive protocol handles registration, collection and removal without a viewer-specific submission dependency

#### Scenario: GUI overlay
- **WHEN** GUI is enabled over scene geometry
- **THEN** its owned draw data renders in its existing overlay pass with clipping and ordering preserved

### Requirement: Failed graph frame cleanup
Graph execution SHALL join all admitted recording tasks and cancel its active frame before propagating a dispatch, recording or end-frame error.

#### Scenario: Retry after a recording error
- **WHEN** an invalid draw causes graph recording to fail and a valid graph is then executed
- **THEN** the valid graph renders without an already-active-frame error

#### Scenario: Peer recorder still runs
- **WHEN** one recording fails while another recording is pending
- **THEN** cancellation occurs only after the pending recorder finishes

### Requirement: Scene-wide pass organization
The Renderer SHALL aggregate scene items by view, target and required rendering state instead of allocating a graph pass per model or primitive. Scene pass organization SHALL initialize depth once before dependent scene depth use and preserve it for subsequent scene draws within the same view. Blended scene items sharing a view and target SHALL be ordered across model boundaries using the documented stable center-depth policy. Material-selected queues and effective state SHALL drive this organization. Compatible draws differing only in pipeline state SHALL remain in one pass; state changes SHALL not cause repeated clears. Different views SHALL have explicit viewport and depth initialization boundaries.

#### Scenario: Many primitives in one scene
- **WHEN** the primitive count exceeds the backend recording-context count but the aggregate pass count remains supported
- **THEN** the scene renders without consuming one recording context per primitive

#### Scenario: Shared scene depth
- **WHEN** independently registered models overlap in a depth-enabled scene
- **THEN** later model draws do not clear earlier scene depth and nearer opaque surfaces occlude farther surfaces

#### Scenario: Mixed depth settings
- **WHEN** compatible scene draws alternate depth-enabled and depth-disabled materials
- **THEN** their own pipeline states control depth behavior without requiring one graph pass per change

#### Scenario: Opaque and masked order preservation
- **WHEN** opaque and masked primitives are interleaved in stable submission order, including equal-depth surfaces
- **THEN** they share the opaque ordering bucket and preserve their original relative order through migration

#### Scenario: Multiple views in a frozen family
- **WHEN** BuildViews prepares multiple views in one frame family
- **THEN** it uses one Render control-message boundary, unique graph pass names and explicit per-view viewport/depth initialization, without mixing material revisions

### Requirement: Limited depth and stencil attachment validation
The graphics graph SHALL validate depth and stencil initialization separately for configured D32 or D32S8 targets and explicit sampled D32 offscreen attachments. Zero through the supported maximum ordered color attachments SHALL be supported, including sampled offscreen color targets with explicit formats. Stencil operations SHALL require a stencil-capable attachment with initialized contents before load. Every color attachment SHALL have independently validated dimensions, format and content operations. This change SHALL NOT imply resolve operations or simultaneous sampled and writable depth use.

#### Scenario: Stencil unavailable
- **WHEN** a pass requires stencil while the configured depth target has no stencil component
- **THEN** validation fails before recording with the target incompatibility

#### Scenario: Undefined stencil load
- **WHEN** the first stencil-using pass attempts to load uncleared stencil contents
- **THEN** graph compilation fails; an explicit first clear followed by later loads is accepted

#### Scenario: Offscreen depth-only pass
- **WHEN** a pass clears and writes an explicit depth texture with no color attachment
- **THEN** the actual texture dimensions, depth format and zero-color pipeline contract are validated without binding or clearing swapchain color

#### Scenario: GBuffer producer and consumer
- **WHEN** a BasePass writes multiple color targets that Lighting samples
- **THEN** all targets participate in RAW/WAR/WAW dependencies and transitions, and undefined, aliased or dimension-incompatible attachments are rejected

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

### Requirement: Centralized RHI frame coordination
The ordinary Viewer frame path SHALL send one owned frame job from Render to the RHI coordinator for deferred packet/GUI preparation, graph validation, frame acquisition, recording and submission. RHI 0 SHALL execute its own recording work inline and join every other admitted executor before submission or cancellation. The Render caller SHALL wait at one frame execution boundary.

#### Scenario: Multi-view scene with GUI
- **WHEN** the Viewer renders shadow views, forward geometry and GUI
- **THEN** preparation and submission use one Render-to-RHI boundary, command lists preserve graph order, and GUI clipping/visibility remain correct

#### Scenario: Single RHI executor
- **WHEN** concurrent recording is unavailable or only one RHI executor exists
- **THEN** all recording runs inline on the coordinator with no self-queue wait

#### Scenario: Deferred preparation or peer recording fails
- **WHEN** deferred preparation, recording dispatch, a peer recorder or EndFrame fails
- **THEN** all admitted peers finish before active-frame cancellation and the next valid frame can render

### Requirement: Ordered owned deferred preparation

Graph passes SHALL declare resources, attachments and accesses before deferred preparation. Preparation SHALL own frozen frame data and produce only draw batches for declared passes, preserving attachment and dependency validation. Render primitives and Main mutable objects SHALL NOT be read by RHI preparation. Existing immediate construction SHALL remain supported.

#### Scenario: Mixed immediate and deferred passes
- **WHEN** immediate and owned deferred draws are interleaved
- **THEN** declared pass order and dependencies remain valid, callbacks cannot introduce hidden resource accesses, and invalid graphs fail before frame acquisition

#### Scenario: Segmented scene draws
- **WHEN** a logical pass contains multiple target-view-compatible draw segments
- **THEN** their order is preserved, the load operation occurs once and the store operation occurs after the last segment

### Requirement: Sampled color target lifetime
Graph color imports SHALL identify immutable physical format, dimensions, initial state and content validity. Exports SHALL establish the requested reusable final state. Recorded lists SHALL retain every color attachment and transition resource through submitted completion including clear-only and failed frames.

#### Scenario: Replaced target generation
- **WHEN** resize or layout replacement occurs while an old color target is retained by submitted work
- **THEN** the old resource is not released until that work completes and the new graph uses only its declared target generation
