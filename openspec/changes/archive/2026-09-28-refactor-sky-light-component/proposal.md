## Why

Sky lighting is currently an Editor feature ("Use Default Sky" toolbar action) rather than an ordinary placeable scene object, which does not match how users expect sky lights to work in common engines. The environment component also exposes its sky reference as an optional field, so Details shows an "Override sky asset" checkbox that cannot be cleared, and it shows properties that have no effect in the current mode.

## What Changes

- Present the environment light component as **Sky Light**, placeable from Place Object (Basic and Lights categories) with a generated marker icon, defaulting to SkyAsset mode with the Engine Cloudy sky.
- **BREAKING** Remove the Editor "Use Default Sky" toolbar button and the `scene.sky.use_default` automation operation.
- **BREAKING** Change the environment record to v3: the sky reference is always present (defaulting to Engine Cloudy), yaw is authored in degrees, and a SkyAsset-only linear `Tint` multiplies sky radiance (diffuse SH, specular IBL and background). v1/v2 records migrate.
- Show only the properties effective in the selected mode. ConstantColor shows Color and Intensity. SkyAsset shows Sky asset, Tint, Intensity, Yaw and Show background.
- Add generic reflection metadata for conditional property visibility, and a generic typed asset-reference picker in reflected inspection. The picker supports a combo of indexed assets and Content Browser drops.
- Keep a single active sky light. A newly created or newly added sky light becomes active only when none is active. Details reports inactive sky lights as not effective and offers an undoable "Set as active sky light" action.
- Tint changes update lighting parameters without reloading sky resources.

## Capabilities

### New Capabilities
None.

### Modified Capabilities
- `engine-default-sky`: remove the shared default-sky Editor/automation action; placement replaces it.
- `scene-lights`: automatic activation of the first environment light; always-present sky reference, Tint and degree yaw in the persistent environment source.
- `skybox-skylighting`: Tint applies consistently to background, diffuse SH and specular IBL, without reload.
- `object-placement`: add the SkyLight entry and marker.
- `component-inspection`: conditional property visibility and typed asset-reference selection.

## Impact

- Runtime modules: Reflection, Scene, SceneEditing, Gui and Renderer (environment parameters and sky pass).
- Shaders: `Common/Sky.hlsl` and `Lighting/EnvironmentLighting.hlsli`.
- Editor and Automation plugins, the AssetTool placement icon build, and Engine editor icon content.
- Scene, sky rendering, placement, automation and Editor acceptance tests.
- Documentation: Editor, automation capabilities, content file system and sky lighting.
- No external dependency upgrades or application lifecycle changes. The change is not archived or committed.
