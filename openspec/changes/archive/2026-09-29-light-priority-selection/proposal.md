## Why

Only explicitly selected directional lights currently illuminate the scene, so enabled additional lights silently do nothing. Sky and directional components need one understandable Priority control instead of activation buttons and persistent readiness text.

## What Changes

- Add reflected signed integer Priority to sky and directional components. Resolve the highest eligible priority with stable persistent-ID tie breaking.
- Render all enabled directional lights; allocate one CSM/contact shadow source by priority among enabled, nonzero, shadow-casting lights.
- Select one enabled sky by priority independently of asset readiness, intensity and background visibility.
- Replace activation/status blocks with component properties and contextual diagnostics on Priority and Sky asset fields.
- **BREAKING** Remove authored scene light selections and obsolete main-light automation controls; expose component editing and resolved lighting diagnostics equally to GUI and automation.
- Preserve current single-light assets with default priority zero; do not implement legacy selection/appearance compatibility. Do not archive or commit this change.

## Capabilities

### New Capabilities
- `light-priority-selection`: Shared deterministic light resolution, component priority UI, asset diagnostics and automation parity.

### Modified Capabilities
- `scene-lights`: Replace explicit lighting selection with priority resolution and additive directional lighting.
- `scene-light-shadow-properties`: Resolve shadow authoring from the priority-selected shadow source.

## Impact

Scene, SceneEditing, Renderer, Gui, Editor and Automation; builtin lighting shaders; scene persistence and fixtures; editor/render/automation tests and current documentation. No new external dependency or plugin lifecycle.
