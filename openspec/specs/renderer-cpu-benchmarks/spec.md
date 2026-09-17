# renderer-cpu-benchmarks Specification

## Purpose
Define reproducible layered CPU submission workloads, strict coverage validation, comparable latency evidence and reusable operation diagnostics.
## Requirements
### Requirement: Reproducible layered CPU submission evidence
The repository SHALL provide repeatable full-scene/material and prepared-packet native recording workloads with explicit warmup, sample count, build identity and settings. Full-engine scans SHALL cover 0, 1, 100, 300, 600 and 1200 ordinary draws and camera/CSM variants. CPU recording SHALL be measured independently of Present waits. Reports SHALL distinguish inclusive parallel work from critical-path frame time.

#### Scenario: Comparing a CPU optimization
- **WHEN** baseline and candidate are measured
- **THEN** repeated runs use identical rendering/validation settings, preserve raw samples, report mean/P95/P99 and record actual item/draw counts and validation status

#### Scenario: Invalid workload
- **WHEN** scene readiness, nonempty requested geometry, expected draw count or CPU/GPU submission coverage is missing
- **THEN** the benchmark rejects the run instead of reporting it as a valid speedup

### Requirement: Stable CPU operation diagnostics
Renderer and native recording SHALL expose reusable phase diagnostics through the existing profiling system and observable cache/recording operations. Profiling-disabled paths SHALL preserve existing compile-time/runtime gating without temporary ad-hoc timers in production hot loops.

#### Scenario: Explaining camera-motion cost
- **WHEN** a profiling capture compares static and moving views
- **THEN** material refresh, batching, scene preparation and native recording work can be attributed separately from presentation and scheduling waits

### Requirement: Component editor iteration comparison
The iteration SHALL deliver a reproducible baseline/candidate performance report using Debug and Release, static and deterministic moving camera cases, and Editor UI/scene/frame measurements. Runs SHALL retain raw evidence, build/content identities, settings, warmup/sample counts, mean/P95/P99, actual workloads and validation status. Load/save and memory effects SHALL be reported with their measurement scope. Unsupported baseline functionality SHALL be labeled rather than assigned invented performance.

#### Scenario: Compare final Sponza
- **WHEN** baseline Sponza and the final component-based whole-model candidate are measured
- **THEN** rendering settings and camera workloads match, ready/nonempty coverage is verified, results include regressions, and object count/UI overhead is explained
