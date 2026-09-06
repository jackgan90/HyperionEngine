## ADDED Requirements
### Requirement: Owner-thread window lifecycle
Window creation, event polling, resize and destruction SHALL be owned by Main and SHALL release platform resources on exit.
#### Scenario: Bounded application run
- **WHEN** the viewer runs with a finite frame count
- **THEN** it creates a window, pumps events and exits successfully after the requested frames
### Requirement: Engine-owned input
The platform wrapper SHALL expose normalized mouse, keyboard, text, focus and quit events without SDL types.
#### Scenario: Event translation
- **WHEN** a supported platform event is received
- **THEN** consumers receive only engine-defined event data
### Requirement: Window state changes
The application SHALL handle resize, minimize, restore and close without losing its event loop or execution-domain lifecycle.
#### Scenario: Lifecycle exercise
- **WHEN** the automated window exercise changes size and minimizes/restores the window
- **THEN** the application remains responsive and completes its shutdown
