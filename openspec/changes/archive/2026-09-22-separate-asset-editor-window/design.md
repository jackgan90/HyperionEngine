## Context

The existing asset workspace owns detached drafts, previews, history and save completion. UI selection currently suppresses the main scene viewport and replaces scene Details. Platform already routes native events per window, while the Editor currently submits one GUI context and one swapchain.

## Goals / Non-Goals

**Goals:** One native asset window with multiple tabs, simultaneous scene rendering, independent input/history, safe close/save/cancel, shared asset refresh, asset-only and owner-group minimization, independent resizing, and repeatable GPU/input acceptance.

**Non-Goals:** Arbitrary tab tear-off, cross-window docking, separate processes, new asset formats, dependency modifications or automatic commits.

## Decisions

- Keep secondary-window orchestration in the Editor plugin, using a focused private asset-window owner. It borrows existing typed asset/task/device/compiler/resource services and owns a native window, GUI context, GUI renderer and swapchain. Do not add application-host lifecycle branches or runtime plugin switching. Separate GUI contexts avoid requiring a new platform-viewport backend for this bounded scope.
- Reuse FAssetWorkspace without the Scene tab. The main Details panel always inspects scene selection. The asset window has a preview/tab panel and its own properties panel with an independent persisted docking layout.
- Associate the asset top-level window with the main native window through an engine-owned Platform owner relationship. It remains non-modal and resizable, above its owner when the main window receives focus, but is not desktop-always-on-top. Explicit asset activation raises its window only on an open/reveal request. Do not repeatedly steal focus from scene controls. Native ownership groups owner minimization/restoration; a minimized asset window remains independently recoverable. This corrects the initial unowned-window implementation, whose hidden-window tests missed desktop stacking.
- Poll and route each native window's event stream separately. Main-window commands always target the scene; asset-window commands target its selected asset. Asset close modals and main document/root/exit modals gate edits in both windows where needed. Switching OS focus releases camera input; window-local coordinates never enter the other context.
- Preserve native activation clicks in Platform's input stream. SDL's default focus-click suppression drops the first press/release when returning from the asset window, reducing a Content Browser double-click to a single selection. Configure the owned adapter to deliver activation clicks; keep existing GUI double-click detection and the asset-open action unchanged. Cover real mouse activation because synthetic engine events bypass this platform behavior.
- Render visible windows separately on the existing Render/RHI domains, sharing the device and resource service. Only visible asset previews draw. Main-window minimization suspends both presentations using native owner-group behavior, while asset-window polling, asynchronous saves and close handling continue. Minimizing only the asset window does not suspend the main viewport. DPI conversion uses each window's own framebuffer scale.
- Create the asset window lazily, restore/raise it when opening another asset, and preserve identity-based tab deduplication. Closing the asset window saves/discards/cancels all asset documents without affecting scene edits or application exit. Queue accepted closure at the next Main frame boundary. Complete saves and publish refresh before destruction. Destroy GUI bindings, previews and swapchain safely before releasing the native window; partial initialization uses the same cleanup order.
- Keep global application exit and content-root transition protection covering both document sets. Close the secondary host before resetting mounted resources. Persist layout separately from the main workspace and propagate the existing application scale.

## Risks / Trade-offs

- Two presented windows cost more than one → skip minimized windows and hidden asset tabs; keep explicit per-window render state.
- Input/history can leak across contexts → remove selection-based main-command routing and test both event streams with edits present.
- Close during submitted rendering or save can invalidate resources → retain existing deferred tab close and handle host destruction between completed frames after publication.
- Existing acceptance assumes one coordinate space → route synthetic events to the intended native host, retain existing asset/property tests and add two-window lifecycle/isolation checks.

## Migration Plan

Keep the existing main layout readable; use a separate asset layout file. No content migration. Implement wrappers/host, integrate document and input routing, then validate Debug/Release builds, GPU/input regressions, plugin absence/shutdown and style/OpenSpec. Leave both active changes uncommitted.

## Open Questions

None blocking. The user selected the independent native asset-window option.
