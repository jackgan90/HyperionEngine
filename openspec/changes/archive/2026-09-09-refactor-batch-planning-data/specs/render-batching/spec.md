## ADDED Requirements

### Requirement: Reusable prepared planning inputs
The built-in planner SHALL reuse bounded immutable program/pass instance contracts and validated stable item instance inputs across visibility changes. View plans and compact chunk lookup SHALL consume these preparations without repeatedly reconstructing reflection and full effective instance snapshots for unchanged items. Complete source, owner, state, resource and value equality SHALL determine reuse; hashes or anonymous ordinals SHALL NOT prove identity. Shared overlays affecting instance parameters SHALL invalidate changed instance inputs. Custom strategies SHALL retain conservative dependency validation.

#### Scenario: Visibility changes around unchanged items
- **WHEN** visible membership changes while surviving stable items retain their effective state and instance values
- **THEN** surviving prepared instance inputs and compiled contracts are reused and affected compact chunks preserve current ordered membership

#### Scenario: Changed instance overlay or resources
- **WHEN** a shared overlay changes an instance field or an item changes resources, generation, section or state
- **THEN** the affected preparation is revalidated and stale instance payload or group compatibility is not reused

#### Scenario: Bounded preparation lifetime
- **WHEN** sources disappear, cache pressure occurs or a plan is invalidated
- **THEN** retained preparation history remains bounded and native source leases retire without invalidating in-flight frames

#### Scenario: Payload pressure with stable planning inputs
- **WHEN** stable item proofs fit the item-count limit but payload caching is small or disabled, including ordinary-only fallback plans
- **THEN** byte-cache eviction does not invalidate non-owning planning proofs, numeric trees are not retained by those proofs, and actual cached payload remains within the byte limit

#### Scenario: Failed preparation cache insertion
- **WHEN** preparation map insertion fails after its LRU node was created
- **THEN** rollback preserves the map/LRU correspondence and subsequent eviction cannot dereference a missing entry

### Requirement: Evidence for planner optimization
Planner optimizations SHALL be verified using the real planner on unchanged and changing visibility/order/value workloads, with identical workload and configuration between baseline and candidate. Reported rendering comparisons SHALL verify readiness, source coverage and validation errors. Measurements SHALL include Debug and Release and identify regressions and limits of the evidence.

#### Scenario: Comparing an internal planner refactor
- **WHEN** a candidate is evaluated against frozen baseline binaries
- **THEN** results include controlled planner timings and full rendering measurements with equivalent source work and preserved correctness checks
