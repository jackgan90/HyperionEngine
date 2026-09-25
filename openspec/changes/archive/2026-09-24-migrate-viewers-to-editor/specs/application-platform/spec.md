## MODIFIED Requirements

### Requirement: Owner-thread window lifecycle
Window creation, event polling, resize and destruction SHALL be owned by Main and SHALL release platform resources on exit.
#### Scenario: Bounded application run
- **WHEN** the application runs with a finite frame count
- **THEN** it creates a window, pumps events and exits successfully after the requested frames
