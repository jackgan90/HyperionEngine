## Why

The Viewer can save PNG screenshots but cannot capture replayable GPU commands and resources from its interface. An optional RenderDoc integration will let both Triangle and Model experiments produce RDC captures directly and optionally inspect them immediately.

## What Changes

- Add an engine-owned Runtime/Capture wrapper and an optional RenderDoc lifecycle plugin, activated before graphics device creation.
- Support loading the installed Windows x64 RenderDoc runtime or reusing an injected runtime without a link-time DLL dependency.
- Capture one complete rendered frame, expose its verified file path and status, and handle unavailable runtimes, overlapping requests and failures.
- Add shared RDC capture/open controls to the existing diagnostics panel and an opt-in automatic open setting.
- Preserve existing screenshot CLI and UI behavior and provide deterministic capture acceptance commands.
- Add reproducible dependency setup, documentation, and real D3D12 capture/replay validation.

## Capabilities

### New Capabilities
- `renderdoc-frame-capture`: Optional runtime loading, capture lifecycle, output discovery, shared Viewer controls, and replay UI launching.

### Modified Capabilities

None. Existing plugin identifiers, screenshot semantics, renderer contracts and configuration behavior remain compatible.

## Impact

New Runtime/Capture and Plugins/RenderDoc targets; Viewer startup/frame orchestration; Config settings; Gui/DebugUI controls; optional CMake/bootstrap dependencies; tests and documentation. RenderDoc APIs remain private to a runtime adapter. No native graphics handles are added to public RHI contracts. Initial support is Windows x64/D3D12 with one graphics device and one window.
