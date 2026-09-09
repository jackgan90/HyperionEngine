## ADDED Requirements

### Requirement: Independent grouping and payload refresh
Built-in instance planning SHALL distinguish effective group compatibility, shared numeric parameters and instance payload invalidation. When a shared-scope update preserves the effective compatibility partition and ordering, the planner SHALL reuse that partition and unchanged instance storage. Arbitrary strategies SHALL retain conservative invalidation unless their dependency contract proves reuse safe. Family cache maintenance SHALL avoid a full repeated scan per view.

#### Scenario: Camera update with stable visible members
- **WHEN** only compatible shared view parameters change and emitted members and instance values remain stable
- **THEN** groups and instance data are reused while draws bind the new shared values

#### Scenario: Compatibility split or custom dependency
- **WHEN** a resource/state/shared-value change splits a group or a custom strategy observes changed candidate values
- **THEN** planning re-evaluates the affected compatibility and preserves exact source coverage and fallback
