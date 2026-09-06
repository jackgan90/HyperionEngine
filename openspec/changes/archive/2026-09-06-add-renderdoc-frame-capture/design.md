## Context

Both experiments use one Viewer and one diagnostics panel. The current PNG readback is separate from a GPU capture. Plugins start after device creation, while RenderDoc must intercept graphics initialization. Frame preparation runs on Render, graphics mutations/submission on RHI 0, and command recording on multiple RHI threads; the application waits for the whole frame.

## Goals / Non-Goals

**Goals:** Optional, engine-owned RDC capture; shared interactive controls; verified capture paths; optional replay UI opening; reproducible Windows x64/D3D12 validation.

**Non-Goals:** Dynamic plugin unloading, multi-device targeting, Vulkan/Metal implementation, general replay API integration, or changing PNG capture behavior. GPU markers are a small diagnostic enhancement, not a new profiling subsystem.

## Decisions

1. **Isolate the dependency in Runtime/Capture.** FFrameCapture exposes only engine types and implements the narrow capture service through Private/Adapters. Plugins/RenderDoc contains a lifecycle plugin that owns the service. Viewer wires UI actions and frame boundaries; Runtime and DebugUI do not depend on the concrete plugin. This follows the mandatory runtime wrapper rule more strictly than putting the vendor header inside a plugin.
2. **Use a compile option and startup configuration.** HYP_ENABLE_RENDERDOC is OFF by default. Explicit developer build switches enable the optional targets and bootstrap a hash-pinned official API header. No import library or installed DLL is required to compile. The existing plugins array selects renderdoc at runtime. Existing configurations continue to work. A requested plugin missing from the build is a clear configuration error; a compiled plugin with a missing/incompatible runtime reports unavailable without stopping rendering.
3. **Activate a small separate startup plugin set.** Filter renderdoc from the rendering plugin requests and activate it before the first graphics call. Start only needs settings; capture later receives the existing FNativeSurface. No general phase/dependency framework or public native device escape hatch is needed for one startup plugin. Keep the loaded DLL resident until process exit; stop disables the service and discards only a capture it owns.
4. **Resolve the existing injected DLL first, otherwise use an explicit absolute DLL path or the standard installed RenderDoc directory.** Request API 1.6.0, which the locally installed v1.37 supports. Keep Windows, RenderDoc symbols and UTF-8 conversions in the private adapter. Header version is not the RenderDoc product version. Do not install or update system software.
5. **Capture explicit complete frames.** Queue one request; begin on RHI 0 before GUI/model GPU preparation; finish on RHI 0 after all recorders, submission and Present. Calls are serialized by existing task waits. Detect external captures and refuse overlap; only discard owned captures on failures. A missing target is a failure, not a successful no-op. Minimized frames leave a request pending until rendering resumes or shutdown cancels it. Capture completion can stall a frame because RenderDoc writes synchronously.
6. **Use exact capture records.** Set an absolute output template with a session-specific prefix. Snapshot capture count before starting, read newly appended records after success, and verify the matching nonempty RDC exists. Keep previous successful paths distinct from current request failures; never auto-open an old file when a new request fails. Preserve UTF-8/Unicode and quote paths containing spaces.
7. **Separate capture and open outcomes.** LaunchReplayUI opens the successfully captured path, optionally after each capture, default off. A launch failure preserves capture success and exposes its own message. Open-last validates that the file still exists. ShowReplayUI alone is insufficient to open a new file. New UI instances are acceptable initially.
8. **Shared controls and deterministic acceptance.** Add an RDC section, capture/open buttons, an auto-open checkbox and status. Disabled controls are genuinely disabled in the Gui wrapper. Add CLI controls for opt-in runtime, path, output and a bounded capture frame; preserve --capture as PNG. Exercise actual widget press/release through normalized GUI events, capture both experiments with the installed runtime, and validate replay using the installed RenderDoc tool.

## Risks / Trade-offs

- Early DLL loading or driver incompatibility -> real capture/replay tests on this machine, plus unavailable diagnostics.
- External/hotkey capture races -> check capture state and avoid changing global hotkeys or terminating external captures.
- Missing DLL, unwritable output, stale/deleted file, failed UI launch -> explicit independent errors, recoverable service, no false success.
- Large model capture overhead -> one outstanding request and visible status; no claim of nonblocking capture.
- Optional dependency bootstrap -> lock just the official header and license, ensure disabled builds never fetch or require it.
- Single-device targeting -> use wildcard device and the existing native surface; document this scope before future multi-device support.

## Migration Plan

Existing presets/configs remain usable. Enable the build option with documented helper switches, enable the runtime plugin through config or CLI, and leave auto-open off unless selected. Roll back by disabling renderdoc in the plugin list and restarting, or rebuilding with the CMake option off. Complete Debug/Release tests and publish local usage/evidence documentation before syncing specs and archiving.

## Open Questions

No product decisions block implementation. Real capture/replay compatibility and replay launch will be resolved during validation.
