## ADDED Requirements

### Requirement: Explicit Editor interaction policies
Editor SHALL centrally capture named current interaction facts and expose separate admission policies for camera navigation, picking, placement, gizmos, hierarchy gestures, shortcuts, scene-document busy state and auxiliary-window blocking. Policies SHALL preserve existing entry-specific differences, input cancellation, focus recovery and shared document validation. Decisions SHALL observe state changes made earlier in the same frame.

#### Scenario: Modal or gesture changes within a frame
- **WHEN** a modal, transition or gesture begins or ends before another input entry is evaluated in the same frame
- **THEN** that entry uses current facts and performs its existing suspend, cancel, finish or reset behavior without stale admission

#### Scenario: Distinct consumers observe the same facts
- **WHEN** an ordinary popup, preference dialog, pending reparent gesture, inspector edit or close-save state is active
- **THEN** each consumer follows its independently specified existing policy rather than a universal busy flag
- **AND** ordinary popup blocks shortcuts without itself making the scene document busy, while close-save blocks auxiliary windows

#### Scenario: Shared editing remains authoritative
- **WHEN** GUI or an attached agent requests a scene mutation or document transition
- **THEN** shared document identity, revision, idle, history and persistence checks remain in force with unchanged operation schemas and errors
