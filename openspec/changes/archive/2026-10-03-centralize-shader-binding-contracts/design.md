## Context

At baseline 0088237, material and compute bindings carry numeric stage masks. RHI visibility repeats their encoding, and D3D12PipelineValidation contains resource translation and reflection/layout coverage rules with no native dependencies. Renderer separately translates the same resource kinds. Existing ShaderRegisterMapping remains authoritative for register encoding and is unchanged.

## Goals / Non-Goals

Goals: give stage sets a typed owner, share common binding facts and validation, and preserve accepted/rejected inputs and rendering output.

Non-goals: new stages/backends/resources, performance changes, draw cache changes, other audit items, archive or commit.

## Decisions

1. Shaders owns EShaderStage and EShaderStageMask in a focused public header, with explicit stage conversion, graphics-set validation and overlap queries. Keep values Vertex=1, Pixel=2, Graphics=3, Compute=4. RHI visibility remains a separate contract with explicit conversion; consumers must not rely on enum ordinals. A typedef or scattered named integers would retain the implicit protocol.
2. RHI owns reflected resource translation and shader/layout validation in a focused implementation. Both Renderer layout construction and D3D12 pipeline admission use it. Preserve texture dimension/scalar rejection, sampler comparison, constant size, structured stride, register-space coverage and instance array checks. Native descriptor/root-signature/device limits remain in D3D12. Do not replace these distinct layers with one permissive validator.
3. Replace per-stage positional counters with records naming their stage; preserve graphics-only material merge and separate compute layouts. No serialization or shader cache version changes are needed because these are CPU binding descriptions, not stored shader artifacts.
4. Add backend-free table and negative tests for shared contracts, retain material shader compilation and native device/compute regressions. Tests use independently stated expected kinds/masks and invalid layouts rather than reproducing implementation loops.

## Risks / Trade-offs

- Earlier resource rejection in Renderer may alter error timing: retain supported inputs and existing native diagnostics; record any deliberate diagnostic consolidation.
- Moving validation may accidentally omit native entry calls: retain the existing call sites and test both native pipelines and public validation.
- MaterialPreparation.cpp already exceeds 500 lines: its touched stage assignment is a local migration; record deferred builder/layout decomposition rather than expanding this change.

## Migration Plan

Freeze baseline tests, add typed/shared contracts and migrate their consumers, run Debug/Release and style/boundary checks, then inspect the exact diff. Keep the change active after implementation. Reverting the scoped source changes restores the previous internal representation without data migration.

## Open Questions

None.
