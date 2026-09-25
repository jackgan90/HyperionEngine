## Context

The user confirmed shadow controls are authored light properties, not merely relocated global widgets. Existing render settings and benchmark tools contain global shadow defaults; scene publications already copy directional light values. Model Details readback is a runtime-only UI section.

## Goals / Non-Goals

Goals: separated settings/HUD/visualization surfaces, true directional light shadow authoring, shared GUI/Automation authority, unchanged legacy scenes on open, current mode and profiling coverage.

Non-goals: additional Unreal visualizers, point/spot shadow rendering, rewriting unrelated controls, editing HyperionAssets, Git commits.

## Decisions

1. CPU shadow value types and validation live in Scene. The renderer's cascaded settings extend CPU values with only the transient preview viewport. DirectionalLight receives an optional reflected shadow-settings group; absent legacy data inherits existing session defaults. Enabling the override authors values, and generic component transactions provide live editing, multi-selection, undo/redo and native persistence. Point/spot schemas remain unsupported rather than accepting ineffective values.
2. Renderer resolves authored values from immutable main-light publication each frame, retaining session defaults separately so switching to an unconfigured light does not leak another light's settings. Current operation IDs remain valid for session defaults; documentation explicitly states component precedence. Missing optional contact rendering remains a controlled capability limitation.
3. Remove both old diagnostics surfaces. Render settings uses explicit, labeled configuration controls; Exposure and named Lit/GBuffer modes use shared viewport/render services. The Viewport toolbar wraps controls when narrow. Compact layouts keep Exposure, Visualizer and HUD controls on the toolbar; camera selection remains available in Viewport options. HUD text uses a bounded compact font when its region is narrow.
4. New nullable viewport option fields expose HUD visibility, profiling category selection and visualization through existing typed Automation operations. Transient view state does not dirty documents. Runtime Gui provides clipped, noninteractive text overlays using the viewport window draw list, with independent left/right regions and bounded line display.
5. Profiling categories separate overview, tasks, GPU passes, device, views and lighting. Controls for Tracy collection remain available in the profiling popup; toggling HUD visibility does not start/stop collection. Snapshot only when needed and keep transport free of feature branches.

## Risks / Trade-offs

- New scene fields -> versioned default migration and read/write/history tests; no automatic file rewrite.
- Settings precedence -> test authored override, missing main light and switch back to legacy defaults.
- Crowded toolbar/HUD -> scale-aware wrapping and clipped text, input remains with the viewport; verify narrow and enlarged UI.
- Exposure range error -> verify both literal and float-roundtripped lower bounds through record drafts, keep validation atomic, and keep settings errors local to the settings window.
- Prior work is uncommitted -> preserve it and record this follow-up separately.
