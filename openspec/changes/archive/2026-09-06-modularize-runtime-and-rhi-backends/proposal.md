## Why

The current RHI hides native types but implements its concrete device and all resource payloads directly in one D3D12 translation unit. Flat source directories also obscure the difference between foundation services, future scene/animation systems, rendering orchestration and native backends.

## What Changes

- **BREAKING**: Introduce abstract RHI device, swapchain and backend contracts, an explicit backend registry/factory, capability queries, and backend-owned resource payloads with device ownership validation.
- Separate headless D3D12 device/resource creation from window presentation and remove hard-coded DXIL selection from rendering plugins.
- **BREAKING**: Organize owned code into cohesive modules under Source/Runtime, Source/Backends, Source/Plugins, Source/Applications and Source/Tests, with module-local Public/Private directories and CMake ownership.
- Define the future dependency placement of Scene, Animation and core utilities without implementing those systems yet.
- Preserve existing rendering and threading behavior, update Visual Studio browsing and validation tools, and add backend-contract tests.

## Capabilities

### New Capabilities

- `rhi-backend-abstraction`: Backend factories, device/swapchain interfaces, capability and resource-ownership contracts.
- `runtime-module-organization`: Cohesive source modules, dependency direction and extension rules for scene/animation systems.

### Modified Capabilities

None. Existing DX12 acceptance requirements remain in force.

## Impact

Public include paths, runtime CMake targets and dependencies, D3D12 implementation, Viewer setup/configuration, rendering plugins, tests, style/boundary tooling and current developer documentation. No Vulkan/Metal implementation or dependency upgrades are included.
