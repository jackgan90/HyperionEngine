## MODIFIED Requirements

### Requirement: Frame-correlated diagnostics and capture
Asynchronous Runtime consumers SHALL consume completed results on Main, expose CPU stage progress, and correlate benchmark/capture outputs to originating frame IDs. Explicit RenderDoc requests SHALL target their intended frame despite prior queued work. Existing synchronous execution entry points SHALL remain available; Editor SHALL continue using synchronous submission while preserving frame-correlated benchmark and capture results.

#### Scenario: Async screenshot and benchmark tail
- **WHEN** the final tick requests a screenshot during an asynchronous benchmark
- **THEN** the output is saved after its completion and all benchmark rows use their own frame's draw and device statistics
