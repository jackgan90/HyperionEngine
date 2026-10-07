## ADDED Requirements

### Requirement: Explicit new and close scene commands
Editor SHALL provide File > New Scene and Close Scene through a UI-independent host service. New SHALL replace the active document with an initially clean, untitled zero-node scene using default scene settings and an independent browsing camera. Close SHALL leave no active scene while preserving the application, asset documents and content root.

#### Scenario: Create without Game content
- **WHEN** New Scene is invoked with no dirty scene and no selected Game root
- **THEN** an empty editable scene is created without implicit camera/light nodes or disk writes

#### Scenario: Close and resume authoring
- **WHEN** a clean scene is closed
- **THEN** the viewport offers New/Open, scene mutation and saving are unavailable, and a subsequent New/Open restores authoring

#### Scenario: Closed-scene placement presentation
- **WHEN** a scene containing placed objects is closed
- **THEN** Place Object retains disabled object entries without per-object preparation prompts or feedback from the retired scene, and New/Open restores normal placement presentation

### Requirement: Unsaved scene transition decisions
New and Close SHALL prompt for Save, Discard or Cancel when the current scene is dirty, using the existing native window-group modal coordination. Save SHALL save the original scene and execute the requested action only after successful persistence and a current clean-state check. Discard SHALL affect only the scene. Cancel and dialog dismissal SHALL retain the current scene and withdraw the continuation.

#### Scenario: Save an untitled scene before replacing it
- **WHEN** Save is chosen for a dirty untitled scene before New or Close
- **THEN** Save As requests a path and successful persistence is followed by the original action

#### Scenario: Save fails or path selection is cancelled
- **WHEN** persistence fails or Save As is cancelled
- **THEN** the current scene and unsaved changes remain and New/Close does not execute

#### Scenario: Cancel after save admission
- **WHEN** a pending transition is cancelled after its save was admitted
- **THEN** the save may complete but its late completion does not execute New/Close

#### Scenario: Asset window is open
- **WHEN** a scene transition prompts while an auxiliary asset window is open
- **THEN** the decision stays above and blocks auxiliary input, and retained asset documents remain unchanged after the scene decision

### Requirement: Empty scene persistence and document invalidation
Untitled Save and Ctrl+S SHALL route to Save As. An empty scene SHALL save/reload as a native scene without implicit objects. Successful scene replacement or close SHALL invalidate prior scene documents and node handles and clear history, selection, drafts and transient viewport/placement state. Invalid, stale or busy requests SHALL have no scene-changing effects.

#### Scenario: Save and reload zero objects
- **WHEN** a fresh New scene is saved and reopened
- **THEN** it remains a zero-object scene with the saved settings and a clean save point

#### Scenario: Old handle after close or new
- **WHEN** an operation uses a previous document or handle following a successful New/Close
- **THEN** it is rejected rather than editing the new or closed document

### Requirement: Discoverable scene lifecycle parity
Automation SHALL expose reflected scene.new and scene.close operations calling the same host service as GUI commands. Requests SHALL require current document/revision and default to rejecting dirty replacement, with explicit Save/Discard decisions. Untitled Save SHALL require scenePath. Completion SHALL report a ready new scene or a closed scene, without waiting for closed-scene readiness. Existing scene.open behavior SHALL remain compatible.

#### Scenario: Discover and invoke lifecycle
- **WHEN** an agent searches, describes and invokes scene.new or scene.close
- **THEN** schemas, enum meanings, examples, effects and actual completion are available through unchanged catalog/session/transports

#### Scenario: Save and close asynchronously
- **WHEN** an agent requests Save before Close at the current document/revision
- **THEN** success occurs only after persistence and retirement, with loaded=false and ready=false while Editor continues running

#### Scenario: Provider unavailable
- **WHEN** the scene provider is absent or disabled
- **THEN** lifecycle operations report controlled unavailability and unrelated operations remain usable
