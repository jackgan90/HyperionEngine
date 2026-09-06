# render-graph-plugins Specification

## Purpose
TBD - created by archiving change add-render-graph-and-triangle-plugin. Update Purpose after archive.
## Requirements
### Requirement: Validated color graph
The engine SHALL validate pass identity, dependencies, color-content initialization and command-context capacity before recording GPU work.
#### Scenario: Invalid graph
- **WHEN** a graph loads undefined color contents or contains cyclic dependencies
- **THEN** compilation fails before GPU submission.

### Requirement: Threaded graph execution
The engine SHALL prepare graph passes on Render, record disjoint contexts on indexed RHI threads and submit them in graph order.
#### Scenario: Triangle frame
- **WHEN** a clear pass and triangle pass are compiled
- **THEN** their command lists produce a triangle over the configured background with valid presentation transitions.

### Requirement: Configurable rendering plugins
The engine SHALL activate rendering plugins using configuration IDs and use engine wrappers for their math, shaders and graphics work.
#### Scenario: Plugin disabled
- **WHEN** no rendering plugins are requested
- **THEN** the Viewer presents only the configured clear color.

### Requirement: Failed graph frame cleanup
Graph execution SHALL join all admitted recording tasks and cancel its active frame before propagating a dispatch, recording or end-frame error.

#### Scenario: Retry after a recording error
- **WHEN** an invalid draw causes graph recording to fail and a valid graph is then executed
- **THEN** the valid graph renders without an already-active-frame error

#### Scenario: Peer recorder still runs
- **WHEN** one recording fails while another recording is pending
- **THEN** cancellation occurs only after the pending recorder finishes
