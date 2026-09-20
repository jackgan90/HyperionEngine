## Why

Editor viewport debugging currently lacks the RenderDoc capture action available in Scene Viewer. Users need a persistent editor preference and a direct capture-and-open action without duplicating frame capture logic.

## What Changes

- Add Edit > Editor preference with a locally persisted RenderDoc capture setting, default off.
- Show a viewport capture icon only while the preference is enabled; report restart or unavailable states explicitly.
- Select the optional RenderDoc service at startup before graphics initialization, preserving explicit plugin disablement.
- Share full-frame capture, cancellation and exact-result replay logic with Viewer.

## Capabilities

### New Capabilities
- `editor-preferences`: Persistent editor settings and preference dialog, initially RenderDoc capture.
- `editor-frame-capture`: Optional viewport capture-and-open action using the common capture service.

### Modified Capabilities

None. Existing Viewer capture behavior is preserved.

## Impact

Editor plugin composition, GUI icon wrapper, Runtime/Capture reusable frame scope, Viewer frame capture caller, build dependencies, tests and editor documentation. No application-host expansion or runtime plugin loading; no Git commit.
