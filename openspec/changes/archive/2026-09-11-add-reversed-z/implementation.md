# Implementation and validation

## Delivered behavior

- Viewer captures `reversed_z` once at startup; the default and all four example configs are true. The reflected setting remains editable/saveable for the next launch, while diagnostics distinguish active and configured values.
- Math owns the depth convention and finite perspective/clip-space transforms. Low-level omitted arguments retain Standard for existing custom callers; application-created views explicitly carry the active convention.
- Scene and legacy targets, swapchain optimized clears, ordinary/instanced/fullscreen material preparation, and relevant view/batch caches use the selected convention. Built-in material states adapt comparison and bias; custom raw states and stencil retain their meaning.
- CSM adapts projection, depth textures and clears, comparison samplers, neutral textures, caster bias and receiver bias together. Transparent sorting and inverse-projection reconstruction preserve their existing geometry semantics.
- Triangle now tests and writes depth and adapts its canonical clip-space depth. Raw depth previews intentionally show reversed far/clear values as black.

## Verification

Environment: Windows, repository Ninja/MSVC Debug and Release presets, native D3D12. GPU suites run sequentially. Debug retains D3D12 validation; profiling remains default-off.

- Final Debug build succeeded; full CTest **56/56 passed**, 191.80 seconds (`out/ReversedZBuildDebugFinal.log`, `out/ReversedZFullDebugFinal.log`).
- Release build succeeded; full CTest **56/56 passed**, 153.70 seconds (`out/ReversedZBuildRelease.log`, `out/ReversedZFullRelease.log`).
- CPU checks cover projection endpoints/monotonicity over multiple near/far ranges, inverse reconstruction, physical frustum equivalence, invalid inputs, clear values, raw/relative compares, bias signs and unchanged stencil.
- GPU tests exercise both conventions through Forward/Deferred, overlapping opaque and transparent geometry, ordinary/instanced paths, same-session cache isolation, fullscreen depth (immediate/deferred preparation), CSM opaque/masked/offscreen casters, motion and resource lifecycle.
- `depth_viewer_acceptance` checks Triangle/Model/Scene x Standard/Reversed x Forward/Deferred with two-frame queue leads. It asserts ready scene content and zero validation errors. Both pending-edit directions preserve the current image exactly, save the desired value and activate it only in a fresh process.
- Visually inspected the captured reversed Deferred Scene image; model surfaces, ground and shadows are present. Captures/logs are generated under each build directory's `depth-acceptance/`.
- Semantic C++ naming, local declarations and boolean-prefix checks passed for **221 translation units** (`out/ReversedZNaming.log`).
- Filename/include/format checks passed for 358 source files; module-boundary checks passed for 340 source files across 25 modules. OpenSpec strict validation and `git diff --check` passed.

## Scope and compatibility

This change supports startup selection and finite far planes. It does not introduce runtime switching or infinite projections. Callers supplying custom view matrices, clip-space geometry, `SV_Depth` or raw material comparisons must explicitly obey their view's convention; see [DepthConventions.md](../../../../docs/DepthConventions.md).

Implementation and independent audit are complete. The user subsequently requested OpenSpec archive/spec synchronization followed by a Git commit.

## Subsequent quality audit

The independent review confirmed and the implementation fixed RZ-01: clip depth remapping could be mistaken for a geometric mirror. Renderer now adapts canonical clip-space WVP, culling and sorting while preserving World, and material view identities track the convention. The original reviewer closed the issue after re-review. See [audit.md](audit.md) for reproduction, fixed-version identity, added GPU regressions and post-fix validation results.
