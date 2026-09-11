## 1. Reflection and schema evolution

- [x] 1.1 Extend record metadata with required/persistent fields, aliases, checked type identity, recursive visitation and optional/map/full unsigned support.
- [x] 1.2 Add type registry, explicit migration chains, enum validation and transactional contextual read diagnostics.
- [x] 1.3 Add reflection regression tests for new type registration, defaults, mismatches, migration, nested diagnostics and unchanged destination on failure.

## 2. Native serialization and asset data

- [x] 2.1 Implement fixed little-endian archive v2 with indexed bulk blocks, allocation limits, ownership-safe decoding and legacy v1 reading.
- [x] 2.2 Add lightweight AssetTypes references/metadata and native asset envelope integrity, identity/revision and reflected dependency verification.
- [x] 2.3 Test wire golden values, bulk data, corrupt metadata/payload, limits and legacy upgrade.

## 3. Native asset management

- [x] 3.1 Replace runtime source codecs with registered native type loading, path-only discovery and checked typed load/save.
- [x] 3.2 Implement portable catalog/reference resolution and generic dependency graph loading with shared work, cycle and per-dependency diagnostics.
- [x] 3.3 Bound completed cache ownership, prune finished tasks and implement explicit invalidation plus ordered save/load revision behavior.
- [x] 3.4 Validate cancellation/drain, retry, one-Worker dependency cycles, relocation, cache eviction and concurrent same-path save/load.

## 4. Offline importing and tool

- [x] 4.1 Introduce source import service/context and move glTF plus scene JSON/legacy primitive conversion behind AssetImport.
- [x] 4.2 Implement tracked fingerprints, settings/importer versions, stable identities, incremental checks and complete immutable dependency publication before the root.
- [x] 4.3 Add AssetTool import/model-to-scene, inspect/validate, catalog and legacy-upgrade commands.
- [x] 4.4 Test preserved glTF semantics, deterministic/unchanged imports, changed external sources, failed batch reimport and conflicting publication.

## 5. Scene, Viewer and content migration

- [x] 5.1 Evolve reflected scene references/transforms/overrides with v1 migration and implement lossless current scene snapshots.
- [x] 5.2 Route SceneInstance through native Assets references and add asynchronous Viewer scene saving with camera and visible status.
- [x] 5.3 Add CMake sample/fixture native content generation and migrate experiment asset paths, runtime source dependencies and integration tests.
- [x] 5.4 Verify 79-instance Showcase and 10-instance Shadows, model/scene source-denied rendering and scene edit/save/reload.

## 6. Delivery validation and documentation

- [x] 6.1 Measure source conversion/native loading time, reads/bytes and peak memory with readiness and build context.
- [x] 6.2 Run relevant CPU/GPU tests and complete Debug/Release suites; validate style, semantic naming, module boundaries and OpenSpec strict rules.
- [x] 6.3 Update asset/source-layout/Viewer usage documentation and record actual verification evidence and limitations.
- [x] 6.4 Verify final task completion and diff consistency, leaving all changes uncommitted.
