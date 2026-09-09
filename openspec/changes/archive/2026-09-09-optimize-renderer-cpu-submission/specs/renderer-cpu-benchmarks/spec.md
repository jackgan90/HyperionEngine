## ADDED Requirements

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
