## Context

The repository already has recursive FRecordDescriptor reflection, a HYPA v1 binary archive, native load/save branches in FAssetService, reflected FModelAsset/FSceneManifest, and a memory-backed cgltf adapter. Viewer currently registers glTF and scene JSON codecs into Assets; Renderer owns the JSON codec registration and resolves model paths. Existing scene samples contain 79 and 10 instances. These capabilities are extended in place.

## Goals / Non-Goals

**Goals:** make registered C++ data the native persistence contract; provide explicit compatible schema evolution; ship a reproducible source-to-native import tool; load and save both asset types through the same service; preserve asynchronous loading, scene sharing, rendering and shutdown; verify runtime loading without source format access.

**Non-Goals:** compiler-generated C++ reflection, serializing object memory/pointers, migrating configuration/UI reflection, adding FBX/OBJ/animation support now, compression/streaming, automatic hot reload, independent material/texture assets, GPU binaries or mip baking. The existing GPU preparation path remains responsible for backend adaptation and mip generation.

## Decisions

### 1. Extend record reflection

Stable string type/field IDs remain independent of C++ names and offsets. A type registry associates IDs with descriptors and checked C++ type identity, enabling untyped path-only loads and checked typed wrappers. Fields have persistent/required flags and read aliases. Generic nested records, arrays, numeric bulk arrays, optional values and string-keyed maps share recursive code; enum descriptors validate stable numeric values. Unsigned 64-bit scalar encoding supports the full range.

Each record owns its schema version, minimum readable version and explicit adjacent migration steps over temporary field nodes. Older versions require a declared path (a no-op step can declare additive compatibility); future and missing-path versions fail. Defaults come from C++ construction; absent required fields fail after migrations; aliases conflict with a simultaneously present current name. Unknown fields produce contextual diagnostics. Duplicate descriptor IDs/aliases are rejected. Reads construct temporary values and validate before committing or publishing; failures include asset/type/field/index context. Save emits current schema, and loading never rewrites disk.

### 2. Stable archive and native envelope

Serialization owns fixed numeric wire tags, little-endian scalars and an explicit archive version. Its new format separates record metadata from indexed numeric/image bulk blocks, validates all counts/ranges and tracks an allocation budget. Owned-byte decoding can retain bulk views until typed construction to avoid an intermediate bulk copy. Public span decoding remains ownership-safe. Legacy HYPA v1 is read-only compatible and can be upgraded through the tool.

AssetTypes owns lightweight reference/header/catalog value data and reflection, without IO/Tasks/Renderer/RHI. Assets owns an envelope containing stable AssetId, root TypeId/schema, content revision, reflected dependency references, optional import provenance and object archive. The native header and payload have integrity checks. Dependencies are collected by a generic reflection visitor recognizing the asset reference descriptor, never a model/scene switch. The dependency table is cross-checked against reflected content. A minimal portable catalog maps IDs to relative paths and rejects duplicate IDs; references can resolve relative to the containing file and validate ID/type/revision. File paths use explicit containing-directory semantics, not an ambient working directory.

### 3. Request, dependency and cache semantics

Assets accepts native files only. Type registration is independent of external importer registration. Decoded CPU objects are immutable snapshots; a generic dependency operation exposes dependencies and failures separately from object and GPU readiness. Dependency graph traversal checks cycles before recursively waiting, shares native loads and retains error context. Scene can continue showing independently successful models.

Cache entries are bounded by retained byte weight and count; in-flight requests have separate ownership and cannot be evicted into duplicate producers. Completed tasks are pruned. Explicit invalidation/reload and save completion affect future requests while existing consumers retain their snapshots. Saves to the same destination execute in admission order and form a barrier for subsequent loads. Cancellation of one consumer does not cancel shared producers; service destruction cancels and drains its own work. Atomic single-file storage remains on the IO domain.

### 4. Independent import service and tool

AssetImport owns importer registration and FAssetImportContext, including tracked source reads through FIOService. cgltf remains memory-backed and private; existing accessor validation, topology conversion, image decoding and static support limits remain intact. Scene JSON decoding moves from Scene/Renderer to an import adapter. The old synchronous primitive loader also moves into AssetImport. The tool and importer tests link AssetImport; runtime Viewer does not.

FAssetImportService converts sources asynchronously and publishes native assets. Import records include importer/version, settings, source/dependency fingerprints and stable output identities. New output IDs are created once and retained across reimport; content hashes identify revisions, not identities. Repeated unchanged imports verify source fingerprints and output integrity and skip rewriting; changed external buffers/images/settings rebuild. Importing a glTF can produce a model, or a scene referencing the complete model with its original node hierarchy. Importing an existing scene JSON deduplicates model conversions and preserves instance IDs, TRS/visibility and camera.

For multi-file imports, dependencies are written under immutable revision-qualified paths and the root scene is atomically replaced only after all dependencies succeed. Its dependency table is a complete portable lookup for that published graph. Old root readers continue resolving old immutable dependencies. Failed imports leave the previous root usable; unreachable generation cleanup is explicit rather than deleting files used by live readers. A catalog operation supports ID-based lookup and directory relocation. Concurrent writes within a service are serialized; tools refuse conflicting active publication to the same output.

AssetTool provides import, inspect/validate, catalog and legacy upgrade operations. It returns nonzero on invalid sources/outputs. Sample content is built from checked-in sources through the C++ importer as a build dependency into out/content, with a deterministic build recipe; fixtures are imported before runtime tests. No Python implementation of the hasset wire format is introduced.

### 5. Scene persistence and Viewer integration

FSceneManifest remains the persisted scene root (hyperion.scene). Asset entries use native typed references; instance entries gain a lossless matrix representation and persistent simple material overrides. Explicit v1 migrations turn existing path entries and TRS into the current representation. FScene/FSceneHandle, GPU resources, mutable material pointers, prepared caches and change queues are never serialized.

FSceneInstance consumes native scenes using Assets reference resolution and retains generation-safe attachment/polling. It exposes a snapshot of current instance state with stable unique instance IDs, current transforms, visibility and simple overrides. The Viewer supplies its current camera, offers asynchronous save, and reports unsupported generic material selections instead of silently discarding them. Scene graph source paths are rebased/validated when saving elsewhere. Model snapshots use generic SaveAsync.

Existing --model/--scene and model_source/scene_source keys stay stable; their asset values become native paths. Experiment configuration remains JSON. Passing a source format at runtime reports the import command to use. Default model/scene/shadow content, fixtures and acceptance scripts migrate together.

## Risks / Trade-offs

- Large asset allocations -> bulk views, explicit cumulative budgets, byte-weighted cache limits and measured load memory; GPU preparation remains separately measured.
- Source conversion changes appearance -> retain adapter algorithms and compare native round trips and known GPU pixels, including shadows/transparency/UVs.
- Reimport exposes mixed revisions -> immutable dependency generations and last-step atomic root publication; do not claim multi-file atomicity from individual writes.
- Scene snapshot loses state -> persist matrices and simple overrides; reject unrepresentable runtime material selections and source-less model attachments.
- Broad fixture migration -> keep importer tests on source formats, runtime tests on generated native assets, and retain test names and CTest dependency ordering.
- Legacy archive ambiguity -> separate container/record/importer versions, retain a bounded legacy reader and only write current versions.

## Migration Plan

Implement and validate reflection/container contracts first, then native asset management, then import tooling and scene schemas. Wire sample generation and runtime entrypoints once tool output is validated. Migrate tests and docs in the same change. Keep source assets and historical archives unchanged; all generated content lives under out. The change remains uncommitted as requested.

## Open Questions

No user decision blocks implementation. Fine-grained API/file naming and allocation defaults will be selected with tests and documented in the implementation evidence; the authorized scope above remains the acceptance contract.
