add_library(hyperion_rhi_d3d12 STATIC Private/D3D12RHIBackend.cpp Private/D3D12RHIDevice.cpp Private/D3D12RHISwapchain.cpp Private/D3D12DeviceState.cpp Private/D3D12Uploads.cpp)
hyp_module(hyperion_rhi_d3d12)
target_link_libraries(hyperion_rhi_d3d12 PUBLIC hyperion_rhi PRIVATE hyperion_core hyp_d3d12ma d3d12 dxgi dxguid)

if(BUILD_TESTING)
  add_executable(d3d12_frame_failure_tests Private/D3D12FrameFailureTests.cpp Private/D3D12RHISwapchain.cpp)
  hyp_executable(d3d12_frame_failure_tests)
  target_compile_definitions(d3d12_frame_failure_tests PRIVATE HYP_TEST_D3D12_PRESENT=1)
  target_link_libraries(d3d12_frame_failure_tests PRIVATE hyperion_rhi_d3d12 hyperion_render hyperion_core hyperion_tasks hyperion_platform hyp_d3d12ma)
  set_target_properties(d3d12_frame_failure_tests PROPERTIES FOLDER "Hyperion/Tests"
    VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
endif()
