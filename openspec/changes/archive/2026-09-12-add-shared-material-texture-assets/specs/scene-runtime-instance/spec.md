## MODIFIED Requirements

### Requirement: Persistent scene snapshots
The scene entity SHALL snapshot current asset references, stable unique instance IDs, lossless transforms, visibility, supported simple material overrides, asset-backed whole-model/per-section material selections and typed local overrides. The caller SHALL supply current camera state. Runtime handles, prepared data and mutable material pointers SHALL not be serialized; unrepresentable selections or source-less attachments SHALL fail explicitly.

#### Scenario: Save edited scene
- **WHEN** instances are duplicated, moved, hidden or removed and a scene snapshot is saved then reloaded
- **THEN** the current persistent state is restored with valid shared references and independent runtime handles

#### Scenario: Save material selections
- **WHEN** one instance selects an independent material asset and modifies typed local values before Save As
- **THEN** reload resolves rebased material/texture references, preserves the selected values and leaves other instances unchanged
