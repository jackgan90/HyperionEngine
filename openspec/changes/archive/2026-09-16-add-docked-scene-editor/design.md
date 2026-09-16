## Context

Gui already copies ImGui CPU geometry into engine-owned snapshots; DebugUI owns its RHI rendering but accepts only the font atlas. Scene rendering currently presents directly to the backbuffer. SceneInstance and SceneCameraController already provide reusable asynchronous loading and navigation.

## Goals / Non-Goals

Goals: a real docked scene editor with menu-based native scene selection, a sampled scene viewport, Sponza support and existing camera gestures. Preserve Viewer and module boundaries. Validate actual rendered output and lifecycle.

Non-goals: detached operating-system windows, scene authoring/saving, gizmos, undo/redo, importing external formats, game playback, and a new general retained widget framework.

## Decisions

- Add Applications/Editor with its own executable and Main-owned document, panel and input state. Load mounted scene catalogs and enumerate native scene entries; opening routes to SceneInstance.Load. Loading failures remain visible and another scene can be opened.
- Pin the official v1.92.9b-docking commit. Keep direct ImGui calls under Gui/Private/Adapters. Add small engine-owned dock, menu, tree, table, image and layout APIs. Apply a flat charcoal theme with blue accents and compact spacing in the editor only.
- Add Runtime/GuiRenderer; DebugUI delegates its existing rendering methods to this service. GUI commands carry opaque numeric texture IDs; per-frame engine texture bindings retain their sources and lifetimes. No native descriptors enter Gui APIs.
- Extend scene pipeline presentation with an explicit optional offscreen color target. Tone mapping writes sRGB-encoded RGBA8 through an sRGB RTV; its ordinary UNORM SRV is sampled by the existing GUI sRGB conversion shader, then displayed through an sRGB backbuffer view. Declare the same source identity in RenderGraph so ordering and transitions are tracked.
- Allow sRGB RenderGraph color views for RGBA8 offscreen textures. D3D12 allocates these resources as typeless, exposes linear and sRGB RTVs and retains the ordinary UNORM SRV. Other color formats remain linear-only. Validate the conversion with actual GPU readback.
- Preserve each legacy Forward material's linear/sRGB output contract by selecting the offscreen RGBA8 view per draw batch. Tonemap and GUI retain explicit sRGB views; mixed legacy batches must match the existing backbuffer output.
- Keep GUI operations on Main, graph construction on Render and resources/submission on RHI. The editor initially joins each submitted frame before consuming statistics; no mutable UI state is captured by rendering tasks. Resizing replaces a retained source generation. The shared Viewer pipeline keeps its existing asynchronous behavior.
- Route camera input using viewport content bounds and focus, preserving capture through right-button drag. Reset on focus loss, minimization, hidden viewport and scene replacement. Reuse SceneCameraController and SceneNavigation, including Home framing.
- Configure the editor's shared controller for fly navigation: translation requires a held viewport RMB, mouse deltas rotate at the current world eye position, and mouse release stops translation immediately. Keep the controller's default orbit mode compatible with SceneViewer. Preserve held key state across an ordinary RMB release so pressing RMB again works with a physically held key; clear held state on UI capture or lifecycle interruption. Forward focus events even when viewport navigation is blocked.
- Give each fly controller its own translation speed, initialized from the first available camera's existing focus-based speed. RMB wheel scales it by 1.2 per notch within 0.01–100000 scene units/second and never dollies. Bare wheel preserves the existing dolly operation without changing this speed. Process button and wheel events in order, preserve speed through input resets, and display the same speed used by Advance in the viewport toolbar. The default orbit controller keeps its focus-based translation and wheel behavior. Speed is session-local, not persisted to disk.
- Persist layout in local generated state with reset support. Do not overwrite a user's saved layout every frame. Use a scene browser modal for mounted native scenes rather than adding a platform-specific file-picker dependency.

## Risks / Trade-offs

- Source and GPU resource lifetimes across resize/load -> retain sources in frame values, join before replacement, drain work on teardown.
- Offscreen color mismatch -> explicit sRGB output view and capture validation.
- Input leaking to viewport behind modal/search -> independent viewport focus and active-widget gating, targeted input tests.
- DockBuilder is internal -> isolate it to the GUI adapter and pin the upstream commit.
- First editor frame loop joins each render frame -> simpler initial ownership, with existing FramePipeline available when overlap is justified.
- Existing static font-atlas path remains -> document the first-version font scope; dynamic atlas/IME expansion is separate work.

## Migration Plan

Additive editor target; existing Viewer commands remain valid. Keep the old dependency archive in generated cache while replacing the checked-out dependency through a verified directory move. No asset repository writes or git commit.

## Open Questions

None blocking the requested viewing workflow.
