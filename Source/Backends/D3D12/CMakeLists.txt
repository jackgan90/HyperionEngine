add_library(hyperion_rhi_d3d12 STATIC Private/D3D12RHIBackend.cpp Private/D3D12RHIDevice.cpp Private/D3D12RHISwapchain.cpp Private/D3D12DeviceState.cpp Private/D3D12Uploads.cpp)
hyp_module(hyperion_rhi_d3d12)
target_link_libraries(hyperion_rhi_d3d12 PUBLIC hyperion_rhi PRIVATE hyperion_core hyp_d3d12ma d3d12 dxgi dxguid)
