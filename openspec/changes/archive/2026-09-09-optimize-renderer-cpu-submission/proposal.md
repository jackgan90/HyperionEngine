## Why

The measured 600 ordinary-draw workload takes 26.123 ms in Debug and 3.734 ms in Release; camera movement increases it to 49.307/7.705 ms despite negligible GPU work. The default four-cascade scene repeats material refresh and batch planning across five views, explaining most of its camera-motion CPU regression. Existing incremental caches still perform substantial per-item allocation, copying and comparison work.

## What Changes

- Establish reproducible CPU submission benchmarks and preserved baseline binaries, separating full scene/material work, native recording, and presentation waits, with stable profiling scopes and operation counts.
- Share immutable parameter updates at their true dependency boundary, using prepared parameter indices and conservative handling of mixed scopes, overrides and custom providers.
- Separate batch membership compatibility from changing shared constants and unchanged instance payloads; preserve conservative invalidation for arbitrary strategies.
- Reuse fenced D3D12 recording objects, suppress redundant native state setters and remove avoidable validation scratch allocation while retaining validation and resource ownership checks.
- Reduce repeated family cache retirement and command-packet copies; consolidate redundant RHI scheduling where dependencies permit.
- Validate correctness, output, resource lifetime and Debug/Release performance after each stage, publishing reproducible measurements and remaining costs.

## Capabilities

### New Capabilities
- `renderer-cpu-benchmarks`: Reproducible workload, coverage, build configuration and CPU phase evidence for renderer submission changes.

### Modified Capabilities
- `material-parameter-binding`: Shared dependency update plans and bounded immutable parameter refresh reuse across compatible items.
- `render-batching`: Reuse grouping and instance payload independently from compatible shared parameter refreshes.
- `rhi-presentation`: Fence-safe native recording reuse and immutable command ownership without changing cancellation or failed-Present recovery.

## Impact

Renderer material evaluation, batch caches, render graph and session scheduling; D3D12 command recording and binding validation; benchmark/test tools and documentation. Public additions remain engine-owned and preserve current API behavior, native validation, shadow quality, source-item coverage and in-flight immutability. No dependency replacement, reduced Debug checks, shadow update skipping, or application-specific material shortcuts are involved.
