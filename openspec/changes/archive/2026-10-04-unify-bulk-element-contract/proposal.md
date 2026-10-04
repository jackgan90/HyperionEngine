## Why

Bulk element identities are independently enumerated by archive decoding, GUI inspection, asset JSON export and reflected wire conversion. A shared Reflection contract will prevent their supported types, wire names and widths from drifting while preserving existing persisted data.

## What Changes

- Define the ten supported bulk element types once in Reflection and derive typed identity, wire name, byte width and typed dispatch from that declaration.
- **BREAKING**: replace the C++ `FBulkData::Element` string and `BulkElement<T>()` string result with `EBulkElement`; migrate all repository consumers. Serialized names and bytes remain unchanged.
- Preserve archive v1/v2 compatibility, shared storage views, numeric validation and existing GUI/wire/JSON behavior for supported elements.
- Add independent compatibility fixtures and rejection coverage for the common definition and its consumers.

## Capabilities

### New Capabilities
- `bulk-element-contract`: Reflection-owned bulk element identities and authoritative mappings used by archive, wire, GUI and source JSON consumers.

### Modified Capabilities

None. Existing archive and automation behavior is preserved.

## Impact

Reflection public archive/value headers and wire conversion, Serialization reader/writer, Gui inspection, AssetImport source JSON export, Scene clipboard accounting, focused tests and native asset documentation. No module dependency, plugin composition, reflection registration, automation operation/schema, archive version or asset hash migration is introduced. Record envelope accessors are a separate stage.
