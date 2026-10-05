## ADDED Requirements

### Requirement: Every-frame rolling interval diagnostics
Editor SHALL collect each measured update-start frame interval independently of HUD visibility and optional profiling providers, excluding the initial synthetic interval. The shared render diagnostics result SHALL retain the existing raw `frameIntervalMilliseconds` field and add reflected rolling statistics with target duration, actual covered duration, sample count, mean milliseconds, throughput FPS, last interval, nearest-rank P95, maximum interval, long-frame threshold/count and capacity status. Storage SHALL be bounded. Whole intervals SHALL be retained as the shortest recent suffix covering at least one second, or all available samples during warmup, subject to capacity. Nonpositive and nonfinite intervals SHALL NOT enter the window. Intervals longer than the target duration SHALL remain observable.

#### Scenario: A long frame between HUD refreshes
- **WHEN** mostly 4 ms frames include a 20 ms frame between two HUD refreshes
- **THEN** the next shared snapshot includes that frame in its mean, P95, maximum and count of frames above 16.67 ms
- **AND** FPS equals 1000 multiplied by sample count divided by total covered milliseconds

#### Scenario: Shared overview and automation data
- **WHEN** the overview HUD refreshes or an agent invokes `render.statistics`
- **THEN** both consume the same typed rolling-statistics source and expose its actual time coverage
- **AND** operation IDs, versions, revisions and the original raw frame-interval meaning remain unchanged

#### Scenario: Warmup and bounded capacity
- **WHEN** less than one second of valid measured intervals is available or the fixed sample capacity is reached
- **THEN** diagnostics report the actual retained coverage and sample count
- **AND** insufficient coverage caused by full capacity is explicitly identified

#### Scenario: Statistics survive visibility changes
- **WHEN** profiling HUD visibility or profiling collection is disabled and later enabled
- **THEN** the overview represents continuously collected recent intervals rather than only frames observed while visible
- **AND** scene history and render scheduling are unchanged

### Requirement: Explicit average and spike presentation
The profiling overview SHALL label average milliseconds/FPS and the actual rolling window, and SHALL display last, P95, maximum and a count with its explicit 60 FPS budget threshold. It SHALL continue using the existing throttled HUD refresh, viewport clipping and input-isolation behavior.

#### Scenario: Inspect average performance and spikes
- **WHEN** the user opens the profiling overview after warmup
- **THEN** the primary ms/FPS line represents the recent-window average and throughput
- **AND** a separate line retains individual-frame and tail-latency information without changing profiling-provider state
