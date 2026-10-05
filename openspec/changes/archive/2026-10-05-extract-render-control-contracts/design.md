## Context

Automation currently includes seven Renderer control headers. Settings mix authored values with pipeline conversion and persistence; diagnostics mix reflected counters with a full render view and device/task sampling. Other contracts are mostly CPU values but acquire those dependencies through shared headers.

## Goals / Non-Goals

Goals are a single authority for existing control identities and reflected fields, a real CPU consumer target, and explicit Renderer production adapters. Provider ownership, operation IDs, rendering algorithms, serialized keys and behavior remain stable. A CPU-only application executable and broad Renderer subsystem restructuring are outside this change.

## Decisions

### Own CPU contracts in RenderControls

Add `hyperion_render_controls` under Runtime with explicit Scene, SceneEditing, Math, Reflection and RasterOptions dependencies. Move interface/DTO definitions, reflection, validation and small value operations. Existing Renderer headers re-export the authoritative definitions and declare their implementation helpers where needed. This avoids duplicate lightweight models and preserves existing renderer include entry points.

| Candidate | Planned disposition | Renderer production ownership |
| --- | --- | --- |
| IRenderSettings | Migrate interface, authored values and reflection | Pipeline conversion and configuration file read/write |
| ISceneViewport | Migrate interface, request/state/options and validation | View preparation, camera/render bridge and host execution |
| IShadowControls | Migrate interface and settings/reflection | Cascade resources and applying published scene metadata |
| ISceneLightControls | Migrate interface, light/sky state and reflection | GPU environment preparation and light production |
| IRenderOutput | Migrate request/result/pending state, window tokens and reflection | File validation, readback completion, PNG writing |
| IRenderDiagnostics | Migrate reflected counters, health and component diagnostics | Device/task sampling and complete render-frame snapshot |
| IRenderCaptureControl | Migrate interface, capture request/status and reflection | Backend capture session and host execution |

All seven candidates are included. Newly separated support values are viewport choices, profiling categories, batch/visibility/light/HZB/view counters, timing values and publication identity. Their render algorithms and resource ownership remain in Renderer.

### Preserve render-only snapshots separately

`FForwardPipelineStatistics` keeps its CPU counters, scene token, camera status and `MainView()` aggregation. A Renderer-owned frame statistics extension retains `MainCameraView`, which has never been reflected. Frame producers and Editor camera consumers use the complete extension; the control diagnostics result receives its CPU base explicitly. No reflected field disappears or changes identity.

The existing `FViewport` numeric value moves to RasterOptions and is re-exported by RHI; control shadow settings therefore retain the exact type, fields and defaults without linking RHI. Cascade implementation stays with Renderer.

### Migrate real consumers and guard boundaries

Automation includes the new contract headers and replaces its direct Renderer dependency with RenderControls. Renderer compatibility headers continue to serve rendering callers. Guard RenderControls' complete production closure against Renderer/RHI/backend/plugin dependencies and add a compile/link consumer covering all seven interfaces and reflection. Inspect the configured graph and actual compile/link inputs. ApplicationServices remains a rendering aggregate and is documented as such.

## Risks / Trade-offs

- Transitive include assumptions may surface during migration → build complete Debug/Release and add explicit includes at real consumers.
- Reflection or defaults could drift during extraction → preserve declarations/descriptors verbatim, compare serialized control schemas and round trips, run existing automation and GUI regressions.
- A sliced render snapshot could lose camera state for Editor → update only complete-frame consumers and exercise camera/status diagnostics.
- Compatibility headers temporarily expose broader renderer APIs → direct control consumers use the new public entry points and the independent consumer verifies the intended boundary.

## Migration Plan

Create CPU support values, extract the contracts and production adapters, migrate Automation, then validate targets, schemas, provider absence and rendering consumers. Rollback restores previous authoritative owners and direct links together. No data migration is needed.
