## Why

The current synchronous cgltf wrapper neither resolves a full scene nor participates in an asset lifecycle. The engine needs a typed asynchronous entry point whose codecs reuse engine IO.

## What Changes

- Add typed asset requests, codec registration, duplicate request sharing, cancellation and CPU-ready immutable results.
- Add private cgltf scene adapter using memory parsing and engine-controlled external dependency reads.
- Decode GLB, data URIs, PNG/JPEG, sparse/interleaved attributes, materials and node instances; validate unsupported required features.
- Expose native reflected model save/load through the same asset service, with explicit codec read/write capabilities.

## Capabilities

### New Capabilities
- `async-gltf-import`: The current synchronous cgltf wrapper neither resolves a full scene nor participates in an asset lifecycle. The engine needs a typed asynchronous entry point whose codecs reuse engine IO.

### Modified Capabilities
None. Existing configuration, triangle and task behavior is preserved.

## Impact

Assets, new AssetImport module, Scene, IO, Serialization, image adapters and fixtures. Reuse pinned cgltf and stb. Depends on add-static-model-data.
