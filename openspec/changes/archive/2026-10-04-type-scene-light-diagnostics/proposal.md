## Why

Renderer writes light diagnostic kinds as strings, and Editor interprets both those strings and reflected field names as semantic identities. Centralizing typed identity removes this spelling coupling while preserving the existing diagnostic protocol.

## What Changes

- Represent known diagnostic kinds with an enum and retain opaque unknown wire tokens for lossless compatibility.
- Generate and consume diagnostic kinds through the Renderer-owned type.
- Identify light Inspector properties through reflected C++ member associations.
- Preserve wire/archive strings, schemas, defaults, selection, status messages and Inspector presentation.

## Capabilities

### New Capabilities
- `typed-scene-light-diagnostics`: Typed diagnostic kinds and member-based light Inspector routing with unchanged external contracts.

### Modified Capabilities
None.

## Impact

Renderer diagnostic values and reflection, Editor light property presentation, and focused native/protocol regressions. No new automation operations, plugin dependencies, persistence migration or lighting algorithm changes.
