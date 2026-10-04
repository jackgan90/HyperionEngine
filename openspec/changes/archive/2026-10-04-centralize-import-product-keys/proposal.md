## Why

AssetImport constructs shared image identities in model conversion and interprets their string layout during publication. Publication also repeats its persisted root marker and texture-content setting. Keeping these contracts in one private owner makes their meaning explicit without changing existing asset identities.

## What Changes

- Define named inputs for source, named-product and shared-product publication keys, and one shared-image format/parser with typed encoding.
- Centralize the persisted root marker, builtin white key, image recipe and texture-content setting in AssetImport.
- Preserve the exact legacy key bytes, path-portability behavior, opaque custom shared keys, OutputIds and reimport identity reuse.
- Add fixed legacy samples and seeded historical OutputIds tests, including custom importer and cross-root sharing behavior.

## Capabilities

### New Capabilities

- `import-product-key-contracts`: Single-owner import identity formatting and compatible publication reuse.

### Modified Capabilities

None. Existing native publication and model conversion behavior remains unchanged.

## Impact

AssetImport-private model conversion and publication, native publication tests, and NativeAssets documentation. Public importer records, reflection/schema, automation operations, plugin composition, dependency boundaries and persistence versions remain unchanged.
