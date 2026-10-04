# reserved-asset-entry-contracts Specification

## Purpose
Define Assets-owned reserved entry and publication staging names while preserving consumer-specific filtering policies.
## Requirements
### Requirement: Reserved asset entries have one owner
Assets SHALL own reserved entry names and the publication staging prefix. Discovery, browsing, migration and publication SHALL consume that definition while retaining caller-specific case and legacy exclusions.

#### Scenario: Case and legacy filtering
- **WHEN** exact-case discovery/migration and case-folding Editor browsing encounter reserved names, mixed-case names or migration-only internal/catalog entries
- **THEN** each produces its previous visible/discovered set and publication creates names recognized by the shared contract

#### Scenario: Independent asset use
- **WHEN** Assets is used without Content or Editor
- **THEN** reserved entry validation remains available without a reverse module dependency
