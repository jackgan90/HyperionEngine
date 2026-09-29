## ADDED Requirements

### Requirement: Independent RenderDoc HUD preference
Editor SHALL provide Show RenderDoc HUD, default false, persisted independently of capture selection. Old preference files without this field SHALL load with HUD hidden. GUI and typed automation SHALL share one domain operation that persists before publishing a changed preference and applies it to an available provider without restart or scene-history changes.

#### Scenario: Live toggle and restore
- **WHEN** the user enables or disables HUD with RenderDoc loaded
- **THEN** subsequent frames reflect the setting without restart and the next launch restores it before rendering

#### Scenario: Provider unavailable
- **WHEN** HUD preference changes while RenderDoc is uncompiled, disabled or failed to initialize
- **THEN** the preference is saved without loading a plugin and effective HUD state is reported unavailable

#### Scenario: Persistence failure
- **WHEN** saving a HUD change fails
- **THEN** the error is visible and prior preference, effective HUD state and saved file remain unchanged

#### Scenario: Automation equivalence
- **WHEN** a client discovers and invokes renderdoc.hud.get or renderdoc.hud.set
- **THEN** reflected contracts expose saved preference and effective state and mutation uses the GUI domain operation
