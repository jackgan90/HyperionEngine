# retained-render-frames Specification

## Purpose
Define retained scene and view preparation, immutable packet reuse, complete invalidation and ownership rules, and measured frame coordination costs.

## Requirements
### Requirement: Stable view preparation is retained
Renderer SHALL retain stable built-in scene/view preparation and immutable packets across warmed frames when all relevant scene, resource, visibility, ordering, material and batch inputs are unchanged. Reuse SHALL be observable separately from rebuilding and SHALL NOT change submitted draw coverage.

#### Scenario: Unchanged ordinary draw scene
- **WHEN** a resource-ready static scene and camera repeat with no changing provider dependencies
- **THEN** collection, parameter preparation, batch planning and packet construction are reused while the same ordered draws are recorded each frame

#### Scenario: Camera and scope invalidation
- **WHEN** camera, viewport, culling mode, material inputs, targets, or any declared dynamic scope changes
- **THEN** every dependent result is refreshed before submission and independent stable data remains eligible for reuse

#### Scenario: Shared camera input ownership
- **WHEN** many items refresh the same engine scopes in one view
- **THEN** compatible evaluations share an immutable engine-input block while local scopes and old-frame values remain independent

#### Scenario: Stable shadow setup
- **WHEN** the camera, light, shadow settings and static scene/resource revisions are unchanged
- **THEN** shadow projection and caster setup reuse their previous result; dynamic query clients without a stable revision continue to execute

#### Scenario: Changed object or resource publication
- **WHEN** an object is added, removed, updated, or its resource readiness/version changes
- **THEN** retained work cannot submit the previous affected state and unrelated valid groups remain renderable

### Requirement: Extension and ownership correctness survives reuse
Retained preparation SHALL preserve conservative custom collection and strategy behavior, stable transparent ordering, complete material dependencies, group failure recovery, current draw diagnostics and frozen frame ownership. Retained histories SHALL be bounded and releasable.

#### Scenario: Custom collection and strategy
- **WHEN** an extension has no stable collection contract or reads arbitrary per-item inputs
- **THEN** it receives the established collection/evaluation opportunities and cannot reuse an invalid plan

#### Scenario: Old frame survives later changes
- **WHEN** a new view/object/material version replaces retained data while older packets remain recorded or submitted
- **THEN** older packets and constants retain their original values until all CPU/GPU users finish

#### Scenario: Native immutable stream reuse
- **WHEN** an owned immutable draw stream repeats with the same target contract
- **THEN** validated native resource addresses and required state operations may be reused while every draw is recorded again, and borrowed or changed packets remain independently validated

#### Scenario: Failed frame and removal
- **WHEN** preparation fails or the scene is removed after reuse
- **THEN** diagnostics identify the current frame/revision, later valid preparation recovers, and resources retire without requiring a new rendered frame

### Requirement: Measured normal-path cost and synchronization
Performance evidence SHALL compare preserved binaries with equal workloads/settings, report warmup/readiness/coverage, and distinguish actual CPU work, overlapping task waits, queue delays and GPU/present waits. Counters SHALL identify retained versus rebuilt work and native state/draw operations.

#### Scenario: Static and moving benchmark
- **WHEN** 0/1/100/300/600/1200 ordinary draws and forward/CSM scenes are measured
- **THEN** reports include static/moving Debug/Release latency distributions, matching validation and coverage, and a source-evidenced wait graph without summing overlapping waits as overhead

#### Scenario: Late GPU completion across capture boundaries
- **WHEN** an older submission completes after a new GPU timing capture begins
- **THEN** its timing remains associated with its original submission capture and does not consume the new capture's sample capacity
