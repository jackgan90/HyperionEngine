## ADDED Requirements

### Requirement: Integrated Editor interaction surfaces
The Edit menu SHALL omit Scene settings, Duplicate primary object and Delete primary, keep children. Existing object-oriented scene role controls and Details hierarchy editing SHALL remain available. Shared duplication and keep-children deletion transactions and Automation operations SHALL remain available; their GUI entry points SHALL be deferred until an Outliner or viewport interaction is designed.

#### Scenario: Open the Edit menu
- **WHEN** a user opens Edit with a loaded scene and selected object
- **THEN** the three removed entries are absent and existing Undo/Redo remain available

#### Scenario: Use the retained editing capabilities
- **WHEN** a user edits through existing scene role or hierarchy controls, or an agent duplicates or removes the primary object while retaining children
- **THEN** the operation continues through the existing shared scene document validation and history
