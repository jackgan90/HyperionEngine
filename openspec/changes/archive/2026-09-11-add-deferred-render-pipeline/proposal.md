## Why

The Viewer currently shades directly into an RGBA8 backbuffer, with model-local tonemapping and no multiple-render-target support. A traditional deferred renderer needs explicit linear HDR color, configurable GBuffer storage, and shared Forward/Deferred material and lighting code so pipeline selection does not change the lighting model.

## What Changes

- Add a default Deferred pipeline: CSM, opaque/masked BasePass, raster fullscreen LightingPass, forward compatibility/transparent draws, shared tonemapping and display overlays.
- Keep an explicitly selectable Forward pipeline using the same HDR lighting and output transform.
- Add engine-owned color formats, sampled color targets and MRT contracts through Renderer, RenderGraph, RHI and D3D12, including target-aware cache identity and lifetime validation.
- Add validated configurable GBuffer layouts, world-position reconstruction and preservation of shadow receiver inputs.
- Organize shaders into reusable material initialization, GBuffer encoding, lighting and output stages selected by compile-time macros.
- Generalize the existing oversized fullscreen triangle for lighting, tonemapping and depth preview.
- Preserve alpha masking, transparency, double-sided materials, Unlit, instance batching and independent CSM visibility. Defer multi-shading-model stencil dispatch while retaining a clear extension boundary.
- Add correctness, lifecycle and image equivalence validation and reproducible CPU/GPU performance reports.
- **BREAKING**: scene rendering now uses a shared HDR-to-display transform; transparent composition occurs before tonemapping. Color attachment contracts gain explicit formats and multiple slots.

## Capabilities

### New Capabilities
- `deferred-render-pipeline`: Configurable raster deferred rendering, compatibility routing, default selection and measured validation.
- `linear-hdr-output`: Linear scene shading, shared tonemapping and explicit sRGB output and UI contracts.
- `fullscreen-render-passes`: Reusable renderer-owned fullscreen triangle passes with graph dependencies and cached resources.

### Modified Capabilities
- `forward-render-pipeline`: Forward remains selectable and shares linear HDR shading/output with Deferred.
- `render-graph-plugins`: Graph supports multiple explicit color targets and sampled color-resource dependencies.
- `rhi-backend-abstraction`: Backend-independent formats and MRT capability/validation extend color target contracts.

## Impact

Runtime Renderer, RHI, Materials resource descriptions, D3D12, shared shaders, Viewer settings/metrics, DebugUI output, tests and performance tools. Scene remains independent from RHI/Renderer. No new third-party dependency, compute lighting, mobile single-pass deferred, MSAA or multi-model stencil dispatch is introduced.
