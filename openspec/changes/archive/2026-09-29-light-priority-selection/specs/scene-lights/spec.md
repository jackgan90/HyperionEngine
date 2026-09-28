## REMOVED Requirements

### Requirement: Explicit main lighting selection
**Reason**: Component priorities replace authored active/main references.
**Migration**: Existing lights default to priority zero; legacy selection behavior is not preserved.

## ADDED Requirements

### Requirement: Additive directional lights and priority environment
All effectively enabled directional lights SHALL contribute their individual direct radiance in builtin Forward, Deferred and transparent paths. One effectively enabled sky SHALL be resolved by Priority and supply background and environment lighting consistently. Sky intensity, background visibility and resource readiness SHALL NOT affect selection. Directional lights SHALL use a global list rather than duplicate entries in every local-light cluster.

#### Scenario: Multiple directions without shadows
- **WHEN** two enabled nonzero directional lights have different directions/colors and neither casts shadows
- **THEN** both illuminate the scene, with indirect and emissive terms evaluated once

#### Scenario: Zero or invisible sky
- **WHEN** the winning sky has zero intensity or disables background visibility
- **THEN** it remains selected and lower-priority environments do not contribute

## MODIFIED Requirements

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
