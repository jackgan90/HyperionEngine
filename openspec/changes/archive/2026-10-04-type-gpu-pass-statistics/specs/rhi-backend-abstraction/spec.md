## ADDED Requirements

### Requirement: Opaque pass timing metadata
RHI commands and completed GPU pass timing results SHALL carry caller-owned opaque value metadata without depending on Renderer categories, parsing display labels or borrowing caller memory. Native backends SHALL preserve the metadata separately for every logical pass, including batched native recording and delayed fence completion. Default metadata SHALL remain valid. Timing enablement, cancellation, capture epoch isolation, capacity handling and exactly-once collection SHALL retain their existing behavior.

#### Scenario: Batched tagged commands
- **WHEN** multiple logical commands with distinct tags share native recording and their submission completes
- **THEN** each published pass timing contains the original tag and display name with its own duration

#### Scenario: Delayed completion
- **WHEN** callers release command references after submission and GPU completion is delayed
- **THEN** retained immutable command ownership preserves metadata until timing collection

#### Scenario: Untimed or cancelled work
- **WHEN** timing is disabled or recorded work is cancelled before submission
- **THEN** no timing sample is fabricated and tags do not change rendering or cleanup behavior
