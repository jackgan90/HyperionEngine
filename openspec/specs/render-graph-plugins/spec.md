# render-graph-plugins Specification

## Purpose
Define validated graph execution, explicit plugin execution domains and scene-wide pass organization independent of concrete geometry producers.

## Requirements
### Requirement: Validated color graph
The engine SHALL validate pass identity, dependencies, color-content initialization and command-context capacity before recording GPU work.
#### Scenario: Invalid graph
- **WHEN** a graph loads undefined color contents or contains cyclic dependencies
- **THEN** compilation fails before GPU submission.

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
The existing single-color graph SHALL validate depth and stencil initialization separately for the configured D32 or D32S8 target. Stencil operations SHALL require a stencil-capable attachment, with initialized contents before load. This change SHALL NOT imply support for arbitrary offscreen resources, multiple color targets or resolve operations.

#### Scenario: Stencil unavailable
- **WHEN** a pass requires stencil while the configured depth target has no stencil component
- **THEN** validation fails before recording with the target incompatibility

#### Scenario: Undefined stencil load
- **WHEN** the first stencil-using pass attempts to load uncleared stencil contents
- **THEN** graph compilation fails; an explicit first clear followed by later loads is accepted
