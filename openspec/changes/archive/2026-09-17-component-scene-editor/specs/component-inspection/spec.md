## ADDED Requirements

### Requirement: Reflected component properties
Inspector SHALL derive ordinary property widgets from metadata declared with the component's record description. Visibility, editing and persistence SHALL be independent, nested supported values SHALL be inspectable, and edits SHALL submit validated transactions instead of mutating live fields.

#### Scenario: Extend a component
- **WHEN** a supported field is added to a component reflection declaration with inspection metadata
- **THEN** the generic panel displays it without an additional per-component UI implementation

### Requirement: Reflected display layouts
Record reflection SHALL support an optional CPU-only projection into a separately reflected editing layout. Components SHALL define this mapping beside their data contract without component-specific Gui code. Draft validation, source validation and transactional commit SHALL remain enforced.

#### Scenario: Project one stored field into several editor fields
- **WHEN** Transform is inspected
- **THEN** its local affine matrix is displayed as Position, Rotation and Scale with X/Y/Z inputs, with rotation in degrees around the corresponding axes and a documented X-then-Y-then-Z composition order

#### Scenario: Preserve authored affine data
- **WHEN** an existing reflected, sheared or singular affine transform is inspected, reverted or edited
- **THEN** merely inspecting or reverting preserves exact stored values, translation-only changes preserve the linear matrix, and rotation/scale edits retain the undisplayed shear data

### Requirement: Immutable rendering diagnostics
Inspector SHALL execute on Main and read rendering state only from immutable copied diagnostics with source identity and applied revision. Render-owned mutable objects and GPU pointers SHALL NOT cross the inspection boundary.

#### Scenario: Delayed publication
- **WHEN** an authored edit is newer than the latest rendering diagnostic
- **THEN** Inspector distinguishes the requested and applied revisions and does not overwrite authored data

#### Scenario: Scene replacement
- **WHEN** a diagnostic arrives after its scene/component has been replaced
- **THEN** it is discarded or explicitly identified as stale
