## ADDED Requirements

### Requirement: Editable document history and save points
Editor SHALL provide component property and object structural editing, undo/redo, native scene save/save-as and a document dirty indicator. Save SHALL capture immutable authored state, and asynchronous completion SHALL not mark subsequent edits as saved. Asset replacement SHALL use explicit reference resolution and reject stale results.

#### Scenario: Edit undo save reload
- **WHEN** an instance property is edited, undone, redone, saved and reopened
- **THEN** its final authored value and independent identity survive without changing other instances

#### Scenario: Edit while saving
- **WHEN** another edit is committed after a save captures its snapshot
- **THEN** successful save completion leaves the document dirty

### Requirement: Independent editor camera
Editor viewport navigation SHALL retain the established fly input and speed behavior while using an independent pose/lens override. Ordinary navigation SHALL not mutate authored scene cameras or document history.

#### Scenario: Browse and save
- **WHEN** the user navigates the editor viewport and saves without editing scene content
- **THEN** scene camera transforms and settings remain unchanged
