# editor-preferences Specification

## Purpose
Provide locally persisted editor preferences, beginning with explicit startup selection of optional RenderDoc capture, while preserving safe defaults and controlled failure behavior.

## Requirements
### Requirement: Persistent editor preference dialog
The editor SHALL offer Edit > Editor preference, opening a dialog whose first setting enables RenderDoc capture. The setting SHALL default to disabled, save locally on change, and restore on the next launch independently of scene and layout data.

#### Scenario: First launch and persistence
- **WHEN** no preference file exists and the user enables capture in Editor preference
- **THEN** the initial value is disabled, the changed value is saved, and a subsequent launch restores enabled

#### Scenario: Disable and restart
- **WHEN** the user disables capture and restarts
- **THEN** capture remains disabled and the viewport has no capture button

#### Scenario: Invalid or unwritable preferences
- **WHEN** reading or saving preferences fails
- **THEN** the editor reports the failure without crashing or claiming a successful save, uses safe defaults on load failure, and preserves the prior saved file on replacement failure

### Requirement: Startup-only optional capture selection
The editor SHALL select the optional capture plugin before graphics creation when the saved preference enables it. Explicit plugin disablement SHALL win. Changing preferences SHALL NOT dynamically load or unload plugins.

#### Scenario: First enable while running
- **WHEN** capture is enabled in a process started without the service
- **THEN** the editor saves the preference and explains that restarting is required before capture is available

#### Scenario: Unavailable provider
- **WHEN** capture is enabled but support is not compiled, the plugin is explicitly disabled, or the runtime is unavailable
- **THEN** the editor remains usable and displays the relevant unavailability reason
