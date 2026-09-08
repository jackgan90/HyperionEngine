## ADDED Requirements

### Requirement: No-op material publication avoids state copies
Validated writes that do not change effective authored state SHALL retain the existing immutable snapshot and revision without first copying the full parameter snapshot. Invalid writes SHALL still fail validation before publication. Engine session inputs SHALL likewise avoid invalidating unchanged Global and Scene data.

#### Scenario: Repeated equal write
- **WHEN** a caller repeatedly writes the same already stored typed value
- **THEN** the snapshot identity and revision remain unchanged and existing frame readers and caches retain their data

#### Scenario: Rejected equal-looking input
- **WHEN** a write uses a stale handle or incompatible type even if some numeric bytes resemble current data
- **THEN** validation rejects the write and the prior snapshot remains unchanged
