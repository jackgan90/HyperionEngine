## Why

Editor acceptance observations convert existing typed widget identities into hand-written string paths shared across 18 files. Reparent acceptance also encodes fixture roles, selection policies and expected outcomes in numeric case branches, making equivalent maintenance difficult to review.

## What Changes

- Replace `InspectionBounds` string paths with shared test-owned widget keys and separately identified reflected-property keys across all observation writers and consumers.
- Preserve distinct required, optional/polling and presence queries, including current surface invalidation behavior.
- Describe the existing 14 Reparent cases with named identities, fixture roles, targets, interruptions and expected outcomes; keep integer indices only for traversing the definition table.
- Replace Reparent parallel handle/ID/world arrays with named fixture records, preserving current fixture IDs, input order, timing and assertions.
- Validate affected existing acceptance scenarios and perform independent quality-audit with confirmed repairs and re-review.
- Leave this change active and all changes uncommitted; no archival or push.

## Capabilities

### New Capabilities

- `editor-acceptance-semantic-targets`: typed acceptance observation identities, explicit Reparent fixture/case definitions and behavior-preserving migration contracts.

### Modified Capabilities

None. Production GUI, scene editing and automation behavior remain unchanged.

## Impact

Editor-owned acceptance headers, observation collection and all `InspectionBounds` consumers, Reparent acceptance fixtures/cases, build registration for test-owned helpers if necessary, and acceptance maintenance documentation. No Runtime API, serialization, automation schema, CLI flag or report contract changes are required.
