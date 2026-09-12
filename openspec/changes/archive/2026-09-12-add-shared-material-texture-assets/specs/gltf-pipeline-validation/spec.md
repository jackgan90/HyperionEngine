## ADDED Requirements

### Requirement: Shared asset pipeline evidence
Validation SHALL cover reflected material/texture round trips, corrupt references, shared library reimport, legacy split upgrade, custom shader execution, isolated edits and saved scene reload. GPU tests SHALL consume native-only content and report relevant resource identities/counters plus D3D12 validation status in Debug and Release.

#### Scenario: End-to-end independent assets
- **WHEN** two distinct models sharing assets are rendered, edited, saved and reloaded with source asset IO denied
- **THEN** expected pixels, sharing counts, scene state and zero D3D12 validation errors are verified

#### Scenario: Publication and lifetime failure cases
- **WHEN** shared reimport fails, a pinned revision mismatches or old GPU work remains in flight during an edit
- **THEN** prior roots remain valid, mismatches fail explicitly and resource retirement remains fence-safe
