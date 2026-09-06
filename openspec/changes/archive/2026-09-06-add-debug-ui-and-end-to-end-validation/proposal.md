## Why
The working triangle framework needs interactive controls and measurable end-to-end evidence to support rapid rendering experiments.
## What Changes
- Integrate Dear ImGui and ImPlot behind a generic engine GUI wrapper and copied draw data.
- Add a debug UI plugin with reflected controls, memory/thread metrics, frame history, experiment saving and screenshots.
- Complete clean Debug/Release builds, GPU lifecycle and configuration validation, documentation and a visible local launch.
## Capabilities
### New Capabilities
- `debug-ui-validation`: GUI abstraction, debug controls, frame metrics and reproducible Windows acceptance checks.
### Modified Capabilities
None.
## Impact
Private ImGui/ImPlot adapters, GUI shaders/plugin, Viewer controls, acceptance tests, build/documentation polish.
