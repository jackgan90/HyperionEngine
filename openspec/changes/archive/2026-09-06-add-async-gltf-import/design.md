## Context

The current synchronous cgltf wrapper neither resolves a full scene nor participates in an asset lifecycle. The engine needs a typed asynchronous entry point whose codecs reuse engine IO.

## Goals / Non-Goals

Goals: Add typed asset requests, codec registration, duplicate request sharing, cancellation and CPU-ready immutable results. Add private cgltf scene adapter using memory parsing and engine-controlled external dependency reads. Decode GLB, data URIs, PNG/JPEG, sparse/interleaved attributes, materials and node instances; validate unsupported required features. Expose native reflected model save/load through the same asset service, with explicit codec read/write capabilities.

Non-goals: no implementation of future FBX/OBJ/COLLADA codecs, animation, compressed glTF extensions, full asset cooking, ECS or additional graphics backends in this series.

## Decisions

### 1. Decision

FAssetService registers readers by type ID and extension; each selected parser validates its format contents. The built-in .hasset codec supports read/write, and other writes report unsupported capability errors. LoadAsync<T> returns immutable CPU data; the cache key includes an absolute lexically normalized path and requested record type (not symlink/case canonicalization). Per-consumer cancellation must not cancel a shared load needed by another consumer. Successful native saves invalidate their destination cache entry.

### 2. Decision

AssetImport privately hosts cgltf. Read the root through IO, parse from memory on Worker, resolve buffers/images through IO and resume Worker, decode images from memory. No cgltf/stb file loader is used by this path. Preserve GLB source bytes until dependent parsing finishes.

### 3. Decision

Support glTF 2.0 static triangle lists/strips/fans, indexed/unindexed data, normalized/interleaved/sparse accessors, normals/tangents/colors, UV0/UV1, external and data URI buffers/images, embedded GLB images, PNG/JPEG, default scene and repeated nodes. Generate missing normals and tangents when needed.

### 4. Decision

Import metallic-roughness materials and KHR_materials_unlit. Unsupported required extensions, skins, morph targets and animations fail explicitly in this static importer rather than silently misrender. Optional unsupported extensions produce diagnostics. No Draco/meshopt/BasisU decoding this iteration.

### 5. Decision

Native .hasset uses the reflected record codec and same IO service for save/load. glTF export returns an unsupported capability error until separately implemented. Registry is generic so Assimp IOSystem/IOStream can be added later without renderer changes.

## Risks / Trade-offs

Third-party synchronous callbacks are not asynchronous IO. Prefer resumable worker reads outside locks; never suspend while holding OS-thread-affine locks or Tracy scopes. Bound decoded dimensions and bytes; keep per-request temporary memory alive and ensure all requests drain before shutdown.

## Migration Plan

Implement after `add-static-model-data`. Keep existing target names, serialized keys, triangle and configuration tests working. Each change is additive until its replacement paths are verified; retain explicit compatibility wrappers where required. Validate focused tests before continuing and run full Debug/Release verification at the end.

## Open Questions

No blocking product decisions. Implementation refinements must be reflected here and validated before task completion.
