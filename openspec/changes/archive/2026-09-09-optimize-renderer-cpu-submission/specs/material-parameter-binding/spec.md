## ADDED Requirements

### Requirement: Shared immutable parameter refresh work
Compatible items SHALL reuse prepared parameter dependency metadata and shared-scope refresh work under bounded ownership. Sharing SHALL validate effective source, scope and immutable input identities, preserve mixed dependencies and override/default/required semantics, and SHALL NOT mutate earlier resolved results. Unchanged resources SHALL retain resource identity.

#### Scenario: One changed view across many objects
- **WHEN** compatible objects retain object/material inputs while their shared view values change
- **THEN** common parameter updates reuse immutable refresh data while each item retains correct object/draw data and earlier frames retain their old view values

#### Scenario: Mixed provider and override transitions
- **WHEN** a custom mixed View/Object provider or permitted override changes or becomes unavailable
- **THEN** affected items refresh independently with current precedence and failure behavior rather than reusing another item's result

#### Scenario: Continuous motion
- **WHEN** a live scope changes for many frames
- **THEN** refresh history remains bounded and eviction preserves externally retained immutable data
