## Why

Buffer usage membership is encoded independently by numeric masks in RHI consumers, RenderGraph and D3D12. Extending the enum can leave graph declarations, material buffer allocation and native validation inconsistent; M02 of the maintainability plan calls for one RHI-owned foundation while preserving each layer's existing acceptance rules.

## What Changes

- Define enum-derived known, shader-read and shader-write usage sets and constexpr typed composition/queries in RHI, preserving every enum value and public `uint32_t` usage field.
- Express RenderGraph's supported shader-only subset and D3D12's native constraints through the shared foundation without merging their validators or expanding support.
- Share Renderer material-buffer usage derivation between graph imports and native allocation.
- Add independent acceptance-matrix, unknown-bit, graph-state and native buffer regressions, retaining write-only Graph `ShaderRead` compatibility and existing ownership/retirement behavior.
- Document the layered contract and its compatibility limits. This adds no user-facing operation or new buffer capability.

## Capabilities

### New Capabilities

- `buffer-usage-contract`: RHI-owned usage membership and typed queries, layered graph/backend validation, and consistent material-source usage derivation.

### Modified Capabilities

None. Existing graphics, compute and graph behavior remains compatible; the new capability makes its usage foundation and layer differences explicit.

## Impact

- RHI public buffer usage declarations; Renderer graph buffers, compute/graphics declarations and material GPU source allocation; D3D12 buffer creation and resource access validation.
- Focused C++ tests in existing RHI/Renderer targets and the module documentation.
- No asset/configuration/schema migration, plugin dependency changes, shader mapping changes, GPU wire layout changes, new backend, cache policy change or new transport operation.
