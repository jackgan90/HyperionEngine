## Why

Asset preview mesh and channel behavior currently depends on GUI ordering, duplicated numeric ranges and parallel arrays. Sky product references, cached textures and captions also rely on matching positions, making presentation changes require coordinated edits across preparation, validation and display.

## What Changes

- Define typed preview shape and channel identities with authoritative mappings to the existing wire integers, captions, geometry paths and channel components.
- Keep reflected preview requests/results and operation IDs unchanged; decode and validate numeric options at the shared preview service boundary.
- Replace positional sky product storage and mapping with named members and one descriptor per reference/product/caption association.
- Resolve preview texture format captions by explicit format identity instead of enum ordinal.
- Preserve existing ordering, defaults, pixel conversion, preview invalidation, asset history and async/GPU lifetimes; verify GUI and automation behavior.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `native-asset-editors`: Require typed preview option identities and explicit product/display mappings while preserving existing preview behavior.
- `automation-capability-parity`: Require stable preview wire values/schema and shared validation when internal preview options become typed.

## Impact

Runtime/AssetEditing owns the CPU preview option contract. Editor consumes it for GUI, preview preparation and texture display; sky texture storage remains private to Editor. Existing asset.preview.get/set discovery and request/result schemas, native assets and renderer algorithms remain unchanged. No new plugin, transport branch or module dependency on Renderer/RHI is introduced. Tests and documentation cover compatibility; this change stops after implementation and validation, without archive or Git commit.
