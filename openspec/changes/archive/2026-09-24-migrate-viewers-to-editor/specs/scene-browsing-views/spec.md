## MODIFIED Requirements

### Requirement: Navigation is temporary
Editor SHALL navigate independent viewport values initialized from the preset or deterministic framing. Ordinary navigation and scene saving SHALL NOT modify the preset, authored camera transforms, or scene revision.

#### Scenario: Camera-free browsing
- **WHEN** a scene without camera objects is loaded and the user navigates
- **THEN** Editor renders normally without adding objects and the authored snapshot remains unchanged

#### Scenario: Explicit initial-view update
- **WHEN** the user sets the editor view as the initial view and saves
- **THEN** the change is undoable, marks the document dirty, and controls the next opening view

### Requirement: Sponza migration preserves content
Shipped Sponza SHALL migrate its verified browsing-only default camera into InitialView through an explicit guarded publication. Other objects, shared dependency files, model references, and appearance SHALL remain unchanged.

#### Scenario: Unsafe migration input
- **WHEN** the candidate camera has additional components, descendants, or conflicting references/state
- **THEN** migration refuses to discard that state

#### Scenario: Published camera-free scene
- **WHEN** the migrated Sponza is opened in Editor
- **THEN** it starts at the previous saved camera framing, renders its model/lights, and contains no placeholder camera object
