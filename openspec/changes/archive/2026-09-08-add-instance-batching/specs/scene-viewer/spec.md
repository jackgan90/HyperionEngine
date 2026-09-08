## ADDED Requirements

### Requirement: Instance batching controls and diagnostics
Scene Viewer SHALL render eligible repeated models through the generic instance batch system by default and expose runtime and command-line controls for count-one comparison. Diagnostics SHALL distinguish source visible items, actual draws, instanced draws, submitted instances, fallback reasons, chunk reuse/rebuilds, uploads and planning/preparation timings.

#### Scenario: Toggle batching
- **WHEN** batching is toggled for an unchanged scene and view
- **THEN** rendered content and visible-item coverage remain unchanged while actual draw counts reflect the selected mode

### Requirement: Reproducible batching validation and performance
Delivery SHALL include deterministic CPU/GPU regression and measured repeated A/B performance on the existing scene. Tests SHALL cover visibility changes, differing numeric values, incompatible resources/state, chunk limits, group failures and old-frame lifetime. Measurements SHALL record build/device/configuration, warmup, frame mean/P95 and distinguish CPU/GPU scopes from total frame time.

#### Scenario: Default repeated scene
- **WHEN** the shipped Showcase scene is rendered after loading with batching enabled and disabled
- **THEN** equivalent images are validated and eligible draws reduce through instancing rather than fewer visible items

#### Scenario: Static and moving measurements
- **WHEN** repeated warmed static and moving-camera A/B runs are collected serially
- **THEN** results report draw reduction, cache/upload behavior and performance including regressions and measurement limitations
