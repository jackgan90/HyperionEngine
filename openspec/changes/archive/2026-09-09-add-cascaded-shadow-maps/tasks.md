## 1. Offscreen graphics foundation

- [x] 1.1 Add sampled depth texture sources, RHI creation/attachment contracts and comparison sampler state.
- [x] 1.2 Implement D3D12 depth SRV/DSV creation, depth-only recording, validation and fenced retention.
- [x] 1.3 Extend graph compilation with explicit depth read/write dependencies, transitions and empty-pass initialization.
- [x] 1.4 Verify depth-only rendering and comparison sampling with CPU/RHI/GPU regression tests.

## 2. Pipeline and shared scene views

- [x] 2.1 Add explicit camera projection data and one spatial maintenance boundary per view family.
- [x] 2.2 Separate per-view targets, pass selection and visibility/batch statistics while preserving ordinary BuildViews clients.
- [x] 2.3 Introduce ForwardRenderPipeline with shadow, forward and extension-stage orchestration; route Viewer through it.

## 3. Cascaded shadows and model permutations

- [x] 3.1 Implement stable splits, finite light basis, caster-aware bounds, texel snapping and independent shadow-view queries.
- [x] 3.2 Add opaque/masked ShadowDepth permutations in Model.hlsl with shared transform/alpha functions and instance support.
- [x] 3.3 Bind rendered depth sources and shared shadow parameters to forward materials with neutral disabled defaults.
- [x] 3.4 Implement bounded bias/normal offset/receiver-plane filtering, PCF, cascade overlap blending and distance fade.

## 4. Viewer and validation

- [x] 4.1 Add SceneViewer shadow controls, cascade/depth visualization and shadow-focused fixture content.
- [x] 4.2 Add named fence-safe GPU timings and CPU/caster/draw/resource diagnostics.
- [x] 4.3 Validate contact, sloped/masked/thin/mirrored/offscreen casters, empty cascades, extreme light and camera motion.
- [x] 4.4 Run repeated warmed Release off/on performance and sustained-motion/resource stability checks; document evidence.
- [x] 4.5 Complete Debug/Release regression, style/naming/boundary/OpenSpec validation; document results and leave changes uncommitted.
