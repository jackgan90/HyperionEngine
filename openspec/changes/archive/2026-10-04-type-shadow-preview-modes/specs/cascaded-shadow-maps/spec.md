## ADDED Requirements

### Requirement: Explicit directional preview interpretation
Rendering SHALL determine cascade coloring, normal-lighting participation and optional cascade-depth selection through the shared typed preview contract. Both forward rendering paths SHALL preserve the existing selection and lighting behavior without interpreting display indices or subtracting numeric mode offsets.

#### Scenario: Cascade depth modes
- **WHEN** a cascade depth preview is selected
- **THEN** the matching existing cascade texture is displayed in both pipeline paths

#### Scenario: Normal and color modes
- **WHEN** lit or cascade-color mode is selected
- **THEN** the previous lighting participation and shader color flag are preserved
