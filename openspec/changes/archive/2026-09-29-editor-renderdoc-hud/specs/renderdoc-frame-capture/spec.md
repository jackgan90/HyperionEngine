## ADDED Requirements

### Requirement: Independent overlay visibility control
The capture wrapper SHALL support startup and runtime process-wide overlay visibility, with vendor calls confined to its private adapter. Changing visibility SHALL preserve other overlay mask bits and SHALL NOT change capture/replay state or keyboard bindings.

#### Scenario: Hide and restore
- **WHEN** overlay visibility changes through the wrapper
- **THEN** only the overall enable bit changes and capture remains usable

#### Scenario: Provider not ready
- **WHEN** visibility is queried or set before initialization, after shutdown or after initialization failure
- **THEN** query reports unavailable and mutation does not dereference an absent API or report success
