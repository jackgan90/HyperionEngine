## ADDED Requirements

### Requirement: Uniform GUI application scale
The GUI SHALL expose an engine-owned finite application scale in [1, 2], applying text, style and explicit logical widget sizes together before the next frame. Framebuffer conversion and input coordinates SHALL remain independent.

#### Scenario: Change and restore scale
- **WHEN** a user changes from 1 to 2 and back to 1 repeatedly
- **THEN** controls and text grow together and return to their original dimensions without cumulative drift or incorrect hit testing

#### Scenario: Invalid scale
- **WHEN** a caller supplies NaN, infinity or an out-of-range scale
- **THEN** the GUI rejects it without changing its active scale

#### Scenario: Stationary pointer during live adjustment
- **WHEN** the user stops moving the pointer while still holding the custom scale control
- **THEN** the requested scale, applied control dimensions and font atlas remain stable over consecutive frames, even after the control changes its own size
- **AND** further pointer movement adjusts the value live and releasing it preserves the last value

### Requirement: Clear fonts and retained atlas lifetime
The GUI SHALL rasterize fonts for the effective size and framebuffer density and carry immutable atlas ownership with draw snapshots. RHI preparation SHALL preserve resources referenced by earlier frames during replacement.

#### Scenario: Queued frames across a change
- **WHEN** a new scale is applied while earlier GUI frames remain queued
- **THEN** every frame renders using its corresponding font atlas and valid retained bindings

### Requirement: Persistent user preference
The Editor SHALL default to 1.25 and expose presets 1, 1.25, 1.5, 1.75 and 2, custom adjustment and reset. GUI services SHALL persist the value separately from scene and layout state and support explicit launch overrides.

#### Scenario: Restart and override
- **WHEN** the application restarts after a normal shutdown
- **THEN** it restores the saved scale unless an explicit valid launch override is supplied

#### Scenario: Invalid saved preference
- **WHEN** the preference is missing or malformed
- **THEN** the application remains usable with its default scale

### Requirement: Preserve layout and rendering semantics
Scaling SHALL preserve docking topology, scene data and camera state. Viewports SHALL continue using the actual available area and GUI-disabled application branches SHALL remain usable.

#### Scenario: Scale a docked workspace
- **WHEN** the user changes scale in a docked editor
- **THEN** menus, properties, popups and bars resize while dock splits remain intact and the scene viewport fits its available region
- **AND** the dock workspace uses the new bar dimensions in the same frame, without a one-frame overlap with the toolbar or status bar
