## Why

The migrated diagnostics window mixes editable renderer configuration, view visualization, light authoring and live profiling. A model readback section in Details also resembles an authored component despite being runtime-only.

## What Changes

- Remove model rendering readback from Details and retire Window > Render diagnostics.
- Add an Edit > Render settings window for pipeline configuration and renderer switches, preserving live/restart behavior.
- Add viewport controls for a left status HUD, Exposure, named Lit/GBuffer visualization modes and a categorized right profiling HUD.
- Store directional/contact shadow settings on directional light components with history, native persistence, backward-compatible loading and Automation access. Point/spot shadow authoring remains unsupported with an explicit capability boundary.
- Preserve diagnostic queries, profiler controls and existing render operation IDs. Legacy render shadow values remain defaults for assets without authored shadow settings; component values take precedence.

## Capabilities

### New Capabilities

- `scene-light-shadow-properties`: CPU-owned optional light shadow properties with persistence, validation, history and renderer consumption.

### Modified Capabilities

- `editor-render-diagnostics`: Separate settings, visualization, status and profiling surfaces while retaining shared services.

## Impact

Scene light values/reflection, Renderer settings/publication resolution, Gui overlay primitives, Editor controls and Automation viewport/component coverage, tests and documentation. No Git commit or external asset-repository edits.
