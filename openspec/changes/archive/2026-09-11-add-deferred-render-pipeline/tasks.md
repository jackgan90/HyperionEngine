## 1. Foundation and baseline

- [x] 1.1 Validate proposal/specs and preserve available baseline binary/build identity for comparison.
- [x] 1.2 Add vendor-free color formats, render-color texture descriptions, capabilities and target signatures.
- [x] 1.3 Implement D3D12 sampled color allocation, MRT views/PSO/recording, validation and resource retention.
- [x] 1.4 Extend RenderGraph and Renderer target declarations, hazards, content validation and cache identities to MRT.
- [x] 1.5 Add and run targeted format/MRT graph and native negative/lifecycle tests.

## 2. Shared shading and output

- [x] 2.1 Generalize the existing fullscreen triangle and migrate depth preview to shared pass construction.
- [x] 2.2 Split model material initialization, alpha coverage and lighting into shared shader includes with pass macros and instance variants.
- [x] 2.3 Implement shared HDR SceneColor, Reinhard tonemapping, sRGB output, clear-color and GUI contracts; add HDR Forward selection.

## 3. Deferred rendering

- [x] 3.1 Implement validated GBuffer layout configuration, resource generations, encode/decode and pixel position reconstruction.
- [x] 3.2 Implement raster BasePass/LightingPass with CSM receiver continuity and shared lighting.
- [x] 3.3 Integrate exclusive opaque/masked, Unlit compatibility and globally sorted transparent routing with retained scene preparation.
- [x] 3.4 Make Deferred default in Viewer and expose pipeline/layout selection, pass statistics and GBuffer diagnostics.

## 4. Acceptance and delivery

- [x] 4.1 Validate shader variants and Forward/Deferred color, material, shadow, instance and image equivalence fixtures.
- [x] 4.2 Validate resize, retained/in-flight target replacement, cancellation, failed-frame recovery and default Viewer workflows.
- [x] 4.3 Extend reproducible performance capture and measure static/moving SceneViewer and ModelViewer with matching Forward/Deferred workloads, CSM and layout/resolution variants.
- [x] 4.4 Complete appropriate Debug/Release builds/tests, formatting/naming/boundary checks and strict OpenSpec validation.
- [x] 4.5 Document implemented API/output contracts, measured performance and limitations, and record final implementation evidence.

## Implementation evidence

- API/color/GBuffer/fullscreen/lifetime and compatibility contracts: [DeferredRendering.md](../../../../docs/DeferredRendering.md).
- 96 matched Release runs, 48,000 sampled frames; 12 historical-condition runs; pooled CPU median/P95, per-pass GPU, memory, binding and upload results: [DeferredPerformance.md](../../../../docs/DeferredPerformance.md), [data](../../../../docs/DeferredPerformanceData.json).
- Debug and Release full CTest: 54/54 each; final ModelViewer camera metadata change: affected Viewer/RenderDoc tests 4/4 each; final native image fixture passes with maximum error approximately one 8-bit code.
- Shader contracts cover DXIL/SPIR-V/MSL; native MRT, HDR, material routes, instance variants, CSM continuity, subviewport, queued generation, resize and failure recovery checks passed.
- Formatting, 219 translation-unit semantic naming plus the final two-file recheck, dependency boundaries, strict OpenSpec validation and whitespace checks passed.
- No GPU speedup is claimed for the measured single-directional-light scenes; exact regressions and historical comparison limitations are documented.

## Audit and archive evidence

- Four confirmed P2 findings were repaired and independently re-reviewed; Debug and Release CTest each passed 54/54 after repair. See [DeferredAudit.md](../../../../docs/DeferredAudit.md).
- Post-repair Scene 1080p measurements passed 24 runs / 12,000 sampled frames with zero validation errors; pre-audit measurements remain labeled separately.
- Archived on 2026-09-11 with nine added and two modified requirements synchronized across six main specifications.
