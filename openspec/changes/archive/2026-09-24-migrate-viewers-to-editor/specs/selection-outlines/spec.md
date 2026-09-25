## MODIFIED Requirements

### Requirement: Editor activation and comparison
Editor SHALL activate the reusable Renderer feature for existing Outliner and viewport selection and expose an immediate Union / PerObject viewport option. Selection and mode changes MUST NOT dirty the document. Independent default pipelines SHALL not activate outlines automatically. A multi-target comparison exercise SHALL demonstrate both modes without requiring multi-selection UI.

#### Scenario: Switching modes
- **WHEN** the user changes the viewport outline mode
- **THEN** the next frame uses that mode without restarting plugins, changing selection, or creating an undo transaction

#### Scenario: Multiple targets without multi-selection UI
- **WHEN** the comparison exercise supplies multiple selected scene objects
- **THEN** both policies run through the production feature and generate distinguishable comparison images
