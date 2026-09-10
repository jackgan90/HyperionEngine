# camera-motion-preparation Specification

## Purpose

Keep static local rendering preparation reusable across camera motion while updating current view-dependent data, preserving bounded ownership and recording reproducible performance evidence.

## Requirements

### Requirement: Shared dependency updates reuse validated local preparation

The Renderer SHALL retain validated local material preparation separately from view/shared updates for static published sources. Compatible shared dependency contracts SHALL be evaluated once per view preparation, while unsupported dependencies retain the general evaluation path.

#### Scenario: Camera changes without local source changes
- **WHEN** a camera moves while local geometry, material, instance fields and resource publications remain unchanged
- **THEN** compatible items receive the current shared values without repeating complete local material evaluation for each item

#### Scenario: A shared contract stops being valid
- **WHEN** required input availability, provider dependencies, resource values, overrides or mixed per-item dependencies invalidate a retained contract
- **THEN** ordinary validation/evaluation determines current values and failures instead of reusing an obsolete result

### Requirement: Stable preparation proofs cross pipeline stages safely

The Renderer SHALL reuse batch membership and draw admission only when a validated proof covers ordered sources, local values, rendering state, resource publication and strategy compatibility. Actual shared GPU bindings and per-source receipts SHALL still reflect the current frame and view.

#### Scenario: Only shared numeric view data changes
- **WHEN** source membership and local instance data remain stable and the built-in strategy's compatibility proof remains valid
- **THEN** the Renderer reuses local batch/draw preparation and binds the current shared constants

#### Scenario: Visibility or a custom strategy changes grouping
- **WHEN** ordered membership changes or a custom strategy requires renewed evaluation
- **THEN** the current sources are regrouped through the applicable strategy and every source is submitted or explicitly failed exactly once

#### Scenario: Unchanged local items survive regrouping
- **WHEN** a static view's membership changes while an individual item's validated local inputs remain unchanged
- **THEN** its bounded local input/candidate/record proof may be reused without suppressing current shared-value compatibility or strategy decisions

### Requirement: Retained visibility preparation remains bounded and immutable

Temporarily culled static preparation SHALL have a finite retention bound, invalidate on relevant scene/resource publication, and preserve independently retained snapshots. Retirement SHALL continue to respect native GPU completion.

#### Scenario: Items leave and reenter a camera view
- **WHEN** a static item becomes visible again within the retained working set
- **THEN** its valid local preparation can be reused while visibility and current shared values are reevaluated

#### Scenario: Sources are removed while an old graph exists
- **WHEN** a source is removed or replaced and an earlier graph retains its frame
- **THEN** new frames exclude the obsolete source, caches retire it within their stated bounds, and the old graph retains its original values until its existing owners finish

#### Scenario: A direct collection caller supplies an obsolete or foreign snapshot
- **WHEN** the public collection API receives a previous snapshot from another scene or an earlier scene/resource publication
- **THEN** it excludes that snapshot's retained preparation from the new result and leaves the independently held previous snapshot intact

#### Scenario: A numeric default provider reads a scope with unrelated resources
- **WHEN** a returned numeric value remains live after the original scope is replaced or its cache entry is removed
- **THEN** that value remains immutable and valid without retaining unrelated textures or buffers from the original scope

### Requirement: Performance claims retain workload and validation evidence

The change SHALL record frozen-baseline comparisons for static, small camera motion and changing visibility in Debug and Release, including frame-time distributions, preparation phases and source/draw coverage. Profiling overhead SHALL be separated from normal timing; Debug validation, shadow quality and update frequency SHALL remain enabled.

#### Scenario: A performance result is delivered
- **WHEN** the optimized implementation is compared with the frozen baseline
- **THEN** the report includes raw command/binary identity evidence, equivalent ready scene coverage and images, zero unexpected validation errors, and any remaining missed target or regression
