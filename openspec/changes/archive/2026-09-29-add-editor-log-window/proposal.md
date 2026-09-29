## Why

Editor diagnostics currently live in a separate console that opens on startup. Users need a dockable in-editor log that includes startup output and remains useful after hiding the console.

## What Changes

- Add a Window > Log toggle and a draggable Log panel initially tabbed with Content Browser at the bottom.
- Retain the current process's complete log history independently of panel visibility, with disk-backed text and paged reads.
- Support Debug, Info, Warning and Error, displaying errors red, warnings yellow and information/debug white.
- Capture process stdout/stderr alongside structured engine logs without losing redirected command-line output.
- Build only Editor as a Windows GUI executable, preserving command-line arguments, exit status and failure diagnostics.
- Expose reflected, paginated log reads to attached automation through the same history consumed by the GUI.

## Capabilities

### New Capabilities
- `editor-log`: Process history, dockable log presentation, automation reads and console-free Editor startup.

### Modified Capabilities
None. Existing editor layout, logging and automation contracts remain compatible; the new capability adds the specified behavior.

## Impact

Runtime/Core logging, Runtime/Platform standard-stream and startup diagnostics adapters, Runtime/Gui docking/text adapters, Editor plugin and application entrypoint, Automation plugin, documentation and targeted regression tests. No external dependency is added. Application host lifecycle remains unchanged. No archive or Git commit is part of this delivery.
