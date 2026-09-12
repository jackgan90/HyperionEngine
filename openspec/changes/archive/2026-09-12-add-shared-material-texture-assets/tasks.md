## 1. Reflected asset contracts

- [x] 1.1 Add the CPU Textures module with validated texture records and offline color-aware mip generation.
- [x] 1.2 Add general material records for pass/shader/state/schema and typed persistent values with texture references.
- [x] 1.3 Separate embedded source model data from current native model material slots and define explicit legacy upgrade handling.
- [x] 1.4 Add CPU round-trip and invalid-data tests and verify module boundaries.

## 2. Offline conversion and publication

- [x] 2.1 Extend importer conversion with stable named typed subasset products and generic dependency rewriting.
- [x] 2.2 Implement shared library identities, revision publication and exclusive publication ordering.
- [x] 2.3 Convert glTF materials and external/embedded image variants into independent assets using the general material preset.
- [x] 2.4 Implement legacy embedded model split upgrade with preserved root identity and rollback.
- [x] 2.5 Update AssetTool, content recipes and incremental/failure tests for shared assets.

## 3. Runtime loading and resource reuse

- [x] 3.1 Resolve native model/material/texture graphs asynchronously with pinned reference validation and structured errors.
- [x] 3.2 Reuse immutable texture sources and material definitions across model assets through bounded shared preparation.
- [x] 3.3 Feed authored shaders and typed material values through existing compilation/reflection and GPU caches.
- [x] 3.4 Separate material-only edits from geometry/texture preparation and expose meaningful reuse diagnostics.
- [x] 3.5 Verify missing dependencies, one-worker lifecycle, instance isolation and fenced old-version lifetime.

## 4. Scene and viewer persistence

- [x] 4.1 Add reflected asset-backed whole-model and section selections with typed local overrides.
- [x] 4.2 Resolve scene material selections and preserve reference association in live scene state.
- [x] 4.3 Snapshot/save/reload selections and overrides with Save As path rebasing and explicit unsupported-resource errors.
- [x] 4.4 Update Viewer and samples to exercise independently shared material/texture assets.

## 5. Validation and delivery

- [x] 5.1 Add source-denied GPU tests for cross-model sharing, role/sampler variants and custom shader assets.
- [x] 5.2 Add GPU evidence for numeric-only edits and saved scene material reload.
- [x] 5.3 Run appropriate Debug and Release builds/tests, style/naming/boundary checks and strict OpenSpec validation.
- [x] 5.4 Document usage, migration, measured reuse and validation evidence; leave all changes uncommitted.
