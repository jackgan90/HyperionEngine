## Context
HLSL is the source language and Windows/DX12 is the first runtime. DXC and SPIRV-Cross are pinned.
## Goals / Non-Goals
Goals: actual compilation and reflection behind owned types; deterministic cache identity. Non-goals: Metal library compilation/runtime verification, shader hot reload, exhaustive binding models.
## Decisions
- DXC emits DXIL or Vulkan 1.1 SPIR-V; SPIRV-Cross reflects uniform buffers, sampled textures and samplers and converts SPIR-V to MSL source.
- Compiler instances serialize calls with a mutex; callers can dispatch work through TaskSystem. Separate instances allow parallel compilation.
- SHA-256 cache identity includes compiler/toolchain pin, target, entry, stage and the complete sorted source-root file contents. This conservatively invalidates more artifacts but handles transitive includes correctly.
- Include loading is restricted to the configured source root; disk cache stores the artifact bytes and verifies an accompanying SHA-256 digest before use.
- The first shader profiles use shader model 6.0. Engine types represent bindings independently of compiler reflection types.
## Risks / Trade-offs
- Conservative cache invalidation → acceptable for a small research shader tree; replace with dependency manifests later.
- MSL generation is not Metal execution → report it as source validation only.
- Per-instance serialized compilation → create independent compiler instances for future bulk cooking.
