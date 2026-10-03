## ADDED Requirements

### Requirement: Shared canonical scene component edit policy

Scene component editing SHALL own immutable-field rules using canonical reflected member identities. GUI single and selection candidates and automation complete-value submissions SHALL validate those rules before document mutation. GUI contextual read-only presentation SHALL consume the same policy; inspector metadata SHALL NOT authorize field mutation. Existing operation IDs, schemas, errors, resource constraints and transaction/history semantics SHALL remain compatible.

#### Scenario: Immutable source field
- **WHEN** a GUI-equivalent candidate or automation submission changes an immutable model binding or model-source field
- **THEN** shared validation rejects it with no scene revision, dirty state or history change even if presentation metadata is absent or writable

#### Scenario: Valid single and selection edits
- **WHEN** valid component candidates preserve immutable source fields and change editable values
- **THEN** both entry paths preserve unedited per-object values and use the existing atomic transaction and continuous interaction history behavior

#### Scenario: Stable discovery and presentation
- **WHEN** existing scene component operations and types are described and the corresponding inspectors are shown
- **THEN** operation/schema contracts remain unchanged and contextual read-only hints reflect the domain policy
