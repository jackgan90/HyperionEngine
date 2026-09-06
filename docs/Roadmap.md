# Windows rendering foundation

Status: all nine changes implemented and verified on 2026-09-06. See [acceptance evidence](Verification.md) and the archived changes under `openspec/changes/archive`.

The user authorized continuous implementation, validation, spec synchronization and archival of these changes in order. Each change must have proposal, design, delta specifications, checked implementation tasks and concrete verification evidence before archival.

1. bootstrap-build-and-core-services — build, dependency isolation, logging, memory, profiling
2. add-task-system-and-execution-domains — oneTBB workers and dedicated Main/Render/RHI executors
3. add-reflection-config-and-plugin-runtime — reflection, persisted settings and static plugins
4. add-windows-application-platform — SDL3 window and application lifecycle
5. add-asset-and-math-foundation — GLM, glTF, PNG and EXR through engine types
6. add-shader-compilation-pipeline — DXC, SPIR-V reflection and MSL conversion
7. add-d3d12-rhi-and-presentation — DX12 device, resources, presentation and GPU lifetime
8. add-render-graph-and-triangle-plugin — validated graph and triangle through RHI
9. add-debug-ui-and-end-to-end-validation — ImGui/ImPlot, screenshots and full validation

The initial renderer uses Windows x64 and D3D12. Plugins are statically linked logical units selected at startup. General CPU jobs use oneTBB. Main, Render and RHI execution domains are owned by Hyperion. Native graphics calls and third-party types remain inside private adapters. Shader sources use HLSL, with DXC producing DXIL/SPIR-V and SPIRV-Cross producing MSL; Metal runtime verification belongs to a future platform change.

Deferred: dynamic plugin reload, AST reflection, full asset cooking, Vulkan/Metal backends (including VMA), multi-GPU-queue graph scheduling, resource aliasing and advanced rendering algorithms.
