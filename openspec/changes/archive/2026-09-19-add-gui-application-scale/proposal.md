## Why

The editor's fixed 15-pixel font and fixed widget dimensions are too small on large displays. Users need one persistent application scale that enlarges text and controls together without changing scene rendering or losing their dock layout.

## What Changes

- Add an engine-owned GUI application scale, applied at frame boundaries from unscaled style metrics.
- Rebuild fonts at the selected size and transport immutable atlas snapshots with GUI draw data for safe RHI updates.
- Scale explicit widget dimensions and provide an Editor appearance menu with presets, custom scale and reset.
- Persist the preference separately from scene documents and dock layouts; support a startup override.
- Validate scaling, input, font replacement, persistence and existing GUI/editor behavior.

## Capabilities

### New Capabilities
- `gui-application-scale`: Shared GUI scaling with crisp fonts, safe resource lifetime and persistent application preferences.

### Modified Capabilities

None.

## Impact

Runtime/Gui, Runtime/GuiRenderer, ApplicationServices GUI plugin, Editor configuration/menu, Viewer panel sizing, GUI tests and editor documentation. No dependency upgrade or native backend-specific API is required. No Git commit is part of this change.
