## 1. Portable shader foundation

- [x] 1.1 Add Compute stage, thread-group metadata and DXIL/SPIR-V/MSL writable-resource reflection with cache versioning.
- [x] 1.2 Add portable compute compilation, reflection, defines/cache and invalid-input tests.

## 2. RHI compute and storage

- [x] 2.1 Add compute pipeline/dispatch descriptors, capabilities, texture mip views and storage buffer/view contracts.
- [x] 2.2 Implement D3D12 storage allocation, SRV/UAV descriptors, compute pipeline validation and bindings.
- [x] 2.3 Implement compute command recording, transitions/UAV ordering and precise diagnostic readback with retained ownership.
- [x] 2.4 Validate independent shader/parameter/resource replacement and negative compute RHI contracts on GPU.

## 3. Graph execution and Renderer compute

- [x] 3.1 Add graph compute/buffer/mip accesses, per-subresource hazards, explicit write coverage and deferred dispatch preparation.
- [x] 3.2 Record ordered logical passes in bounded native batches and preserve per-pass timings, state isolation and failure cleanup.
- [x] 3.3 Add Renderer compute programs, reflected named parameter publication and shared constant/resource helpers with bounded caches.
- [x] 3.4 Validate mixed compute/graphics dependencies, more than 16 logical passes, frozen inputs and lifetime/failure cases.

## 4. Requested hierarchical depth

- [x] 4.1 Implement per-frame/view HZB request sharing and metadata without a contact-settings dependency.
- [x] 4.2 Implement R32F mip-zero initialization and conservative NPOT nearest/farthest compute reduction for both depth conventions.
- [x] 4.3 Compare every GPU mip with full-precision CPU references and test consumer activation, resize and stale-generation rejection.

## 5. Contact visibility and pipeline integration

- [x] 5.1 Implement deterministic hierarchical contact tracing with geometric bias, thickness, clipping, full-resolution confirmation and fades.
- [x] 5.2 Insert requested HZB and fullscreen mask after BasePass; combine only directional direct visibility in ordinary/clustered Deferred lighting.
- [x] 5.3 Add frozen runtime settings, DebugUI toggles/parameters and mask/mip/activation diagnostics, preserving Forward and later surfaces.
- [x] 5.4 Validate analytic contact fixtures, dual depth conventions, CSM/contact combinations and disabled baseline behavior.

## 6. Delivery validation and documentation

- [x] 6.1 Run ready current Sponza with GUI toggles, static/moving cameras, resize and resource-lifetime checks in Debug/Release and both depth conventions.
- [x] 6.2 Capture actual GPU HZB/mask costs and stable allocation/descriptor evidence, including disabled-work assertions.
- [x] 6.3 Update current architecture/usage documentation and implementation evidence; run style/naming/boundaries, relevant/full build tests and strict OpenSpec validation.
- [x] 6.4 Verify final diff and unfinished-work status, leaving all changes uncommitted.
