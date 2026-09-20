## MODIFIED Requirements

### Requirement: Selected object PRS overlay
Editor SHALL show one active PRS overlay for the primary selected object in the editor view. X/Y/Z handles SHALL use red/green/blue, with a highlighted hovered or active handle, and SHALL be clipped to the viewport image. The selection and highlighting capabilities SHALL supply the shared ordered selection.

#### Scenario: Select and change mode
- **WHEN** an object is selected in Outliner and the user chooses Position, Rotation or Scale
- **THEN** the matching arrow, ring or scale-handle overlay appears at its world origin

#### Scenario: Preview an authored camera
- **WHEN** the viewport is showing an authored scene-camera preview
- **THEN** transform manipulation and its overlay are inactive

## ADDED Requirements

### Requirement: Primary-centered group transform
Multi-selection gizmos SHALL apply a common snapshot-relative world transformation around the primary origin. The primary's final world matrix SHALL match its single-selection result, including affine effects of nonuniform parents. Other objects SHALL change positions and linear transforms together. Selected descendants SHALL inherit the operation once. Undefined group scale ratios and required singular parent mappings SHALL reject the affected operation without partial changes; single-object zero-scale recovery and Details absolute assignment SHALL remain available.

#### Scenario: Translate a hierarchy once
- **WHEN** a parent and child are selected and translated by a world displacement
- **THEN** both world positions change by that displacement exactly once

#### Scenario: Rotate and scale around primary
- **WHEN** two independent objects are rotated or scaled using the primary gizmo
- **THEN** the primary origin stays fixed and the other object's offset and orientation or shape follow the same affine operation

#### Scenario: Initially zero group scale
- **WHEN** a group scale operation would require an undefined ratio from an initially zero primary scale
- **THEN** that operation is unavailable with an explanation, while a gesture starting from valid scale can cross zero into negative scale
