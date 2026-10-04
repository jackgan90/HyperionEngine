## ADDED Requirements

### Requirement: Named batch cache identity dimensions
Batch item and instance record cache keys SHALL expose named identity dimensions. Equality, hash mixing, ordering and per-view range retirement SHALL preserve existing behavior, including source generation, local item identity and depth convention where applicable. Key presentation SHALL NOT change payload ownership or cache algorithms.

#### Scenario: Change an identity dimension
- **WHEN** one legacy key dimension changes
- **THEN** equality and ordering distinguish the same identities as the previous tuple model

#### Scenario: Retire one view range
- **WHEN** cached chunks for a view and usage are retired
- **THEN** the ordered lookup includes exactly that prefix and leaves other views and usages unaffected
