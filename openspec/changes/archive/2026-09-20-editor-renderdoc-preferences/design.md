## Context

Viewer already uses FFrameCapture through the optional renderdoc plugin, and a private RAII scope brackets ExecuteGraphOnRhi. Editor builds its viewport and GUI graph on Render and synchronously executes it on RHI. RenderDoc hooks must exist before device creation; plugins are selected only at startup.

## Goals / Non-Goals

**Goals:** Persistent extensible editor preferences, a viewport capture icon, exact latest capture replay, common capture orchestration, controlled optional-service failure.

**Non-Goals:** Runtime plugin loading, automatic editor restart, changing Viewer CLI semantics, storing preferences in scene assets, committing or archiving without a separate request.

## Decisions

- Store editor-owned preferences separately from scene data and GUI layout at out/editor/Preferences.ini, with an overridable path for isolated acceptance. Default RenderDoc to false; validate input, report malformed/read/write failures and retain prior saved data on a failed replacement. A small versioned key/value format avoids introducing another serialization dependency for one boolean and can grow with additional settings.
- Read preferences before host startup and explicitly request renderdoc when enabled. The checkbox is explicit plugin intent, unlike scene data. DisabledPlugins retains final precedence. Missing compilation/runtime support leaves editing functional and exposes a reason in the dialog and button tooltip.
- Checkbox changes save immediately. Disabling hides the icon immediately; first enabling in a process without the service shows a disabled icon and a restart explanation. No DLL is loaded after device creation. A service already started can be used again after toggling off/on.
- Move the RAII scope to Runtime/Capture and share request, begin, end, cancellation and optional replay. Viewer preserves scheduled-capture failure behavior; Editor displays capture errors without terminating editing. Calls bracket GPU preparation through Present on RHI 0. Replay occurs immediately after successful EndFrame, before another frame can replace LastCapture.
- Use an engine-owned camera icon, the existing modal wrapper and typed optional service. Editor retains synchronous frame execution; copy capture action and native surface into RHI work. Provider lifetime remains managed by plugin ordering.

## Risks / Trade-offs

- Restart required on initial enable → explanatory UI and documentation; no unsafe late hooks.
- Missing DLL/build support or explicit disable → disabled action with a specific reason and normal editing remains available.
- Preference write failure → preserve saved file, show error, allow retry.
- Capture/replay failure → preserve capture service ownership rules and report status without opening stale results.

## Migration Plan

No existing files require migration. Absence of the preference file uses default off. Build with RenderDoc support to make capture available. Rollback removes the new UI; the local preference file is harmless.

## Open Questions

None.
