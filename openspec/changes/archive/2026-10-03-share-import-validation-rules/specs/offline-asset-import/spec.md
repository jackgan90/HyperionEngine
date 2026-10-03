## ADDED Requirements

### Requirement: Independent publication with shared input rules
The publication service SHALL use the same foundational output, default-library and source-identity rules as workspace preflight while remaining independently callable without a workspace. Refactoring SHALL preserve existing accepted inputs, exception types, diagnostic text, check order, synchronous versus asynchronous rejection and transactional publication behavior. Read-only preparation SHALL reuse library normalization without gaining additional publication admission requirements.

#### Scenario: Output cannot replace the source
- **WHEN** the normalized output equals the normalized source or its case-insensitive extension is not .hasset
- **THEN** ImportAsync rejects synchronously with the existing separate-file diagnostic before publication writes

#### Scenario: Publication source identity rejects in worker
- **WHEN** an admitted import has an incomplete sourceRoot/sourceId pair or a non-portable logical sourceId
- **THEN** its asynchronous result reports the existing specific invalid_argument diagnostic and publishes no files

#### Scenario: Independent grouped library policy
- **WHEN** a grouped service request specifies a library equal to the normalized output parent
- **THEN** it remains accepted, while a separate library is rejected synchronously

#### Scenario: Preparation stays read-only
- **WHEN** PrepareAsync uses an omitted or explicitly equivalent dependency library
- **THEN** the prepared content remains equivalent, uses the normalized library and writes no publication files
