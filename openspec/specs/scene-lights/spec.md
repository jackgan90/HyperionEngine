# scene-lights Specification

## Purpose
Define Scene-owned directional and environment light state, explicit main-light selection, persistent defaults and one published lighting source for forward, deferred and shadow rendering.
## Requirements
### Requirement: Scene-owned directional and environment lights
Scene SHALL own directional-light and environment-light nodes with stable identity, hierarchy, enabled state, linear RGB color and intensity. Directional lights SHALL also own a cast-shadows flag. Colors, intensity and their radiance product SHALL be finite and nonnegative. Directional emission forward SHALL derive from the node world minus-Z pose; the existing surface-to-light semantic SHALL receive its opposite. Environment radiance SHALL be independent of node position and orientation.

#### Scenario: Parent rotation and translation
- **WHEN** a directional light parent rotates and then translates without further rotation
- **THEN** the rotation changes world light direction, the translation does not change it, and both edits persist with the scene

#### Scenario: Invalid light edit
- **WHEN** a light edit supplies a negative, nonfinite or overflowing radiance value
- **THEN** the edit is rejected without changing the existing light or publication revisions

### Requirement: Explicit main lighting selection
The scene SHALL persist explicit selections for one main directional light and one environment light. Multiple candidate nodes SHALL remain editable and persistable, but the builtin pipeline SHALL consume only the selected node of each kind. Empty selections and disabled selected nodes SHALL contribute zero corresponding radiance. Removing a selected node SHALL clear the selection without silently selecting a different light.

#### Scenario: Multiple candidate lights
- **WHEN** a scene contains two enabled directional-light candidates and selects only the second
- **THEN** builtin direct lighting and CSM use the second, and editing the first does not change the rendered lighting

#### Scenario: Remove the selected light
- **WHEN** the selected directional light is removed
- **THEN** subsequent frames have zero directional radiance and no directional shadows until the scene explicitly selects or creates a light

#### Scenario: Disable a light ancestor
- **WHEN** the selected light's ancestor is disabled and subsequently reenabled
- **THEN** that light contribution disappears and returns through normal scene publication without session parameter edits

### Requirement: One published lighting source for all builtin paths
Forward materials, deferred lighting and CSM SHALL consume lighting derived from the same resolved scene publication. Application, CLI, GUI and benchmark light controls SHALL edit scene nodes. Scene-owned lighting SHALL NOT be overwritten by session defaults or external providers for the protected builtin semantics. Existing builtin light semantic names and uniform block layouts SHALL remain stable.

#### Scenario: Same-frame camera and light edit
- **WHEN** camera, model parent and main-light state change before a frame is admitted
- **THEN** all builtin rendering paths consume the same publication's effective state and the old retained frame remains unchanged

#### Scenario: Competing input source
- **WHEN** a scene-bound caller attempts to override protected light inputs through SetSceneParameters or a custom builtin-light provider
- **THEN** the conflicting operation or scene-frame admission fails explicitly instead of rendering with a second authoritative light state

### Requirement: Empty and unshadowed light behavior
Absent, disabled or zero-radiance directional lights SHALL disable their CSM contribution and publish zero direct radiance with a finite neutral direction. A selected light with cast-shadows disabled SHALL retain direct lighting while disabling CSM. Pipeline shadow quality and preview settings SHALL remain separate from persistent light properties. Ambient, emissive and unlit terms SHALL preserve their independent behavior.

#### Scenario: Toggle cast shadows
- **WHEN** the selected directional light changes cast-shadows from true to false
- **THEN** direct lighting remains while shadow views and stale shadow sampling are disabled for the new frame

#### Scenario: No lights in a native scene
- **WHEN** a v4 scene explicitly contains no selected lights
- **THEN** it stays unlit by directional and environment sources across load, render, save and reload without automatic default-light resurrection

### Requirement: Default lighting is explicit scene content
Legacy scene migration and ModelViewer scene initialization SHALL create actual default light nodes matching established radiance and direction. An empty FScene SHALL remain empty. An explicit CLI light override SHALL mutate or explicitly create and select a scene light once after scene initialization, rather than continuously injecting session parameters.

#### Scenario: Load a legacy scene
- **WHEN** a legacy scene without stored lights is migrated
- **THEN** the migrated scene contains deterministic default light nodes, preserves the prior appearance, and subsequent edits to those nodes can be saved

#### Scenario: CLI override followed by save
- **WHEN** a user applies the existing light-direction CLI override and saves the scene
- **THEN** the saved scene contains the overridden scene-node direction and reload does not restore the prior session default
