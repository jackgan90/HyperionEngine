## ADDED Requirements

### Requirement: Component editor iteration comparison
The iteration SHALL deliver a reproducible baseline/candidate performance report using Debug and Release, static and deterministic moving camera cases, and Editor UI/scene/frame measurements. Runs SHALL retain raw evidence, build/content identities, settings, warmup/sample counts, mean/P95/P99, actual workloads and validation status. Load/save and memory effects SHALL be reported with their measurement scope. Unsupported baseline functionality SHALL be labeled rather than assigned invented performance.

#### Scenario: Compare final Sponza
- **WHEN** baseline Sponza and the final component-based whole-model candidate are measured
- **THEN** rendering settings and camera workloads match, ready/nonempty coverage is verified, results include regressions, and object count/UI overhead is explained
