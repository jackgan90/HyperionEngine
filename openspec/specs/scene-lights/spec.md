# scene-lights Specification

## Purpose
Define Scene-owned directional and environment light state, additive directional lighting, priority-resolved sky and shadow sources, persistent defaults and consistent immutable publication across rendering paths.
## Requirements
### Requirement: Scene-owned directional and environment lights
Scene SHALL own directional-light and environment-light nodes with stable identity, hierarchy, enabled state, linear RGB color and intensity. Directional lights SHALL also own a cast-shadows flag. Colors, intensity and their radiance product SHALL be finite and nonnegative. Directional emission forward SHALL derive from the node world minus-Z pose; the existing surface-to-light semantic SHALL receive its opposite. Environment radiance SHALL be independent of node position and orientation.

#### Scenario: Parent rotation and translation
- **WHEN** a directional light parent rotates and then translates without further rotation
- **THEN** the rotation changes world light direction, the translation does not change it, and both edits persist with the scene

#### Scenario: Invalid light edit
- **WHEN** a light edit supplies a negative, nonfinite or overflowing radiance value
- **THEN** the edit is rejected without changing the existing light or publication revisions

### Requirement: One published lighting source for all builtin paths
Forward materials, deferred lighting, sky background and shadows SHALL consume lighting derived from the same immutable scene publication. Application, CLI, GUI and automation controls SHALL edit scene components. Scene-owned lighting SHALL NOT be overwritten by session defaults or external providers for protected builtin semantics. Selected shadow inputs and the full directional list SHALL describe distinct roles.

#### Scenario: Same-frame edits
- **WHEN** priority, transforms and enablement change before frame admission
- **THEN** all builtin rendering uses that publication and retained frames remain unchanged

### Requirement: Empty and unshadowed light behavior
Absent, disabled or zero-radiance directional lights SHALL NOT compete for shadows. Shadow candidates SHALL require cast-shadows; the highest-priority candidate SHALL supply the one CSM/contact source. Other enabled lights SHALL retain direct lighting. No candidate SHALL disable shadow sampling with finite neutral inputs. Pipeline quality remains separate from authored light properties.

#### Scenario: Higher priority without shadows
- **WHEN** the highest-priority directional light disables cast-shadows and another light remains eligible
- **THEN** both illuminate and the eligible light supplies shadows

#### Scenario: Empty scene
- **WHEN** a scene has no enabled sky or directional lights
- **THEN** it contributes no corresponding light and no automatic nodes are created

### Requirement: Default lighting is explicit scene content
Legacy scene migration SHALL create actual default light nodes where historically required. An empty FScene SHALL remain empty. CLI overrides SHALL edit or explicitly create scene components once, and SHALL NOT continuously inject session parameters or authored main-light selections.

#### Scenario: CLI override followed by save
- **WHEN** a light-direction override is applied and saved
- **THEN** the edited component direction persists on reload

### Requirement: Persistent environment source selection
Environment lights SHALL select either ConstantColor or SkyAsset, with an always-present native sky reference, a linear RGB tint, finite yaw in degrees, shared radiance intensity and independently controlled background visibility. New environment values SHALL default to SkyAsset with the Engine default sky. Sky orientation SHALL use explicit yaw, independent of node transform. SkyAsset mode SHALL replace constant ambient, and the constant color SHALL affect only ConstantColor mode. Existing environment records SHALL migrate while retaining their source, yaw orientation and requested sky. A record without a sky reference SHALL receive the Engine default sky. Snapshot and Save As SHALL preserve and rebase requested references, including pending or failed choices.

#### Scenario: Save a sky selection
- **WHEN** a sky reference, tint, yaw, intensity and visibility are edited and saved to another directory
- **THEN** loading the saved scene restores the same requested environment and correctly resolves its pinned dependencies

#### Scenario: Migrate an earlier environment record
- **WHEN** a version 2 environment record with yaw in radians and no sky reference is loaded
- **THEN** it keeps ConstantColor, its yaw is expressed in equivalent degrees, and its sky reference is the Engine default sky

#### Scenario: Reject an invalid sky reference
- **WHEN** an edit supplies an empty reference, a non-sky reference or a negative tint
- **THEN** the edit is rejected without changing the existing light

### Requirement: Generation-safe environment publication
Environment loading SHALL be asynchronous and selection-generation-safe. Complete sky resources SHALL publish together; initial readiness SHALL include environment readiness. Failed or stale loads SHALL NOT overwrite newer choices, mix visual/lighting generations or implicitly enable fixed ambient. Retained complete skies MAY remain visible during replacement while requested state and failures remain observable.

#### Scenario: Rapid selection and failure
- **WHEN** sky A is replaced by B and then a missing asset before B completes
- **THEN** stale completions do not become authoritative, the failure is reported, and saving never substitutes the previously rendered asset for the requested one

### Requirement: Additive directional lights and priority environment
All effectively enabled directional lights SHALL contribute their individual direct radiance in builtin Forward, Deferred and transparent paths. One effectively enabled sky SHALL be resolved by Priority and supply background and environment lighting consistently. Sky intensity, background visibility and resource readiness SHALL NOT affect selection. Directional lights SHALL use a global list rather than duplicate entries in every local-light cluster.

#### Scenario: Multiple directions without shadows
- **WHEN** two enabled nonzero directional lights have different directions/colors and neither casts shadows
- **THEN** both illuminate the scene, with indirect and emissive terms evaluated once

#### Scenario: Zero or invisible sky
- **WHEN** the winning sky has zero intensity or disables background visibility
- **THEN** it remains selected and lower-priority environments do not contribute
