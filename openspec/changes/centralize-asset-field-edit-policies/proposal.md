## Why

Asset editing currently repeats field-name branches across domain code, GUI and automation, while callers choose whether an edit invalidates previews. This already makes a model-node rename conservatively invalidate previews through automation but not through the GUI. M11B builds on M11A's canonical member identity so one AssetEditing policy owns validation, preparation and before/after effects.

## What Changes

- Associate AssetEditing field policies with the reflected member identity established by M11A, preserving reflection's domain independence.
- Centralize generic-field versus dedicated-operation access, normalization, reference requirements and change effects; remove caller-supplied preview flags and duplicated field-name dispatch in adapters.
- Derive metadata-only versus preview-relevant changes from old/new values, including mixed model-node and primitive fields, and retain effects in history.
- Preserve operation IDs/schemas, persisted asset formats, reference-loading and generation checks, transaction/history/save behavior, and dedicated texture/primitive protections.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `native-asset-editors`: Canonical field policies and domain-owned effects shared by GUI, standalone and attached editing.

## Impact

Depends on `bind-asset-properties-to-record-members` (M11A), which must be accepted first. Runtime AssetEditing, Editor/Automation property adapters and relevant equivalence tests/documentation are affected. No transport branches, public operation/schema additions, persistence migration, renderer dependency in Reflection or new plugin lifecycle are introduced.
