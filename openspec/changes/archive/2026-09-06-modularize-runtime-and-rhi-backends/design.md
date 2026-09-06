## Context

The current D3D12 wrapper combines device setup, resources, command recording and presentation. The engine already has factory-based logical plugins and vendor-free public types, but has no replaceable graphics backend contract. Modules also share global include/src/adapters directories.

## Goals / Non-Goals

**Goals:** Independently extensible backends, headless device creation, explicit capabilities and ownership, cohesive source modules and unchanged Windows triangle/GUI behavior.

**Non-goals:** Implement Vulkan/Metal, scene management, animation, DLL reload, multiple GPU queues, a general resource graph or a universal render-command vocabulary.

## Decisions

1. Use `Source/Runtime/<Module>/{Public/Hyperion/<Module>,Private}` for Core, Tasks, Reflection, Config, Plugins, Platform, Math, Assets, Shaders, RHI, Renderer and Gui. Backend and algorithm modules live under `Source/Backends` and `Source/Plugins`; executable composition roots live under `Source/Applications`; tests are grouped under `Source/Tests`. Each module owns its CMakeLists and exports only its Public include root. Adapter code is module-local `Private/Adapters`; native graphics implementations live in backend Private directories. Existing executable and established target names remain stable, with new targets for newly separated responsibilities.
2. Scene and Animation will be peer runtime modules using Core/Math/Assets/Tasks. They produce scene data consumed by Renderer; they do not issue RHI commands or depend on native backend modules. Generic utilities belong to focused Core subfolders rather than a catch-all Utils module. Document these extension points rather than adding empty implementation stubs.
3. An explicit `FRHIBackendRegistry` owns `IRHIBackend` providers. Applications register compiled providers and request a backend at startup. The common RHI module neither includes nor links D3D12. A test-only provider proves the factory and interfaces without native graphics linkage. Unregistered/unknown backends fail explicitly; no silent fallback or production Vulkan stub.
4. `IRHIDevice` has a virtual destructor, capability/feature queries, resource creation, swapchain creation, idle wait and statistics. Creation is transactional through the backend factory and does not require a window. `IRHISwapchain` owns the existing window-color frame recording/presentation contract. This limited contract can later give way to generic command contexts; it is explicitly not an offscreen render graph yet.
5. Capabilities include backend/shader target, recording/descriptor limits and feature support versus enabled state. Baseline rendering features are enabled; queried advanced hardware features remain disabled until exposed by the RHI. Required unsupported/unimplemented features fail creation. Optional features remain disabled when unavailable. Backend enum selection is persisted as `rhi_backend` with a backward-compatible d3d12 default and optionally overridden by `--backend`.
6. Handles contain typed polymorphic engine payload interfaces. D3D12 payloads retain shared native device state and are dynamically checked for backend type and device ownership before recording. GPU frame fences retain resource handles until completion. Command lists additionally carry swapchain/frame/context identity and reject foreign or stale submission.
7. D3D12 device state owns the queue and synchronization used by uploads and swapchains. Swapchains own their buffers, allocators and frame-retention rings. Explicit idle waits and destruction drain work; resource/shared-state lifetimes remain valid if handles or swapchains outlive the public device wrapper.
8. Plugins query the device's shader target instead of spelling DXIL. Current shader resource layouts and single RGBA8 color target remain limited; expanding Vulkan layouts, formats and queues belongs to the implementing backend change.

## Risks / Trade-offs

- Moving files can mask include-path dependency leaks → export module-local include roots, validate ownership and allowed include dependencies, and regenerate VS filters.
- Polymorphic resources add casts at the backend boundary → validate packet resources before recording; keep native loops concrete and revisit handles only with profiling evidence.
- Shared queue/fence state can race with recording → frame control, uploads, submission and destruction remain serialized on RHI 0; only distinct recording contexts run concurrently.
- Device/swapchain separation can release resources too early → test headless creation, outstanding resource lifetimes, foreign device resources, stale lists, resizing and GPU readback.

## Migration Plan

Move owned modules and repair build/tooling paths, add the common interfaces and factory, extract D3D12 device/state/resources/swapchain, migrate consumers and config, add CPU/GPU contract tests, then verify VS Debug/Release and semantic style checks. Record final results and archive the change.
