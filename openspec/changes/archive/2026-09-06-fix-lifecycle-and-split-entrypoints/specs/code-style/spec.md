## ADDED Requirements

### Requirement: Focused function length
Repository guidance SHALL state that a single function normally does not exceed 100 lines and longer functions are split at logical boundaries. An exception SHALL be allowed when the logic is so tightly coupled that meaningful decomposition is difficult; the reason SHALL be documented near the function.

#### Scenario: Review an oversized function
- **WHEN** a new or materially changed function exceeds 100 lines including its complete definition
- **THEN** it is split by responsibility or carries a concrete tightly coupled logic justification

#### Scenario: Refactor Viewer and importer
- **WHEN** this bounded refactor is complete
- **THEN** the decomposed Viewer and importer functions satisfy the principle while existing external behavior remains unchanged
