add_executable(hyperion_viewer
  Private/Main.cpp Private/Viewer.manifest
  Private/ViewerApplication.cpp Private/ViewerApplication.h
  Private/ViewerOptions.cpp Private/ViewerOptions.h
  Private/ViewerFrame.cpp Private/ViewerOutput.cpp Private/ViewerBenchmark.cpp
  Private/ViewerCapture.cpp Private/ViewerCaptureScope.h)
hyp_executable(hyperion_viewer)
target_link_libraries(hyperion_viewer PRIVATE hyperion_render hyperion_debug_ui hyperion_triangle hyperion_model_viewer hyperion_scene_viewer hyperion_asset_import hyperion_io hyperion_assets hyperion_rhi_d3d12 hyperion_core hyperion_tasks hyperion_config)
target_compile_definitions(hyperion_viewer PRIVATE HYP_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_compile_definitions(hyperion_viewer PRIVATE HYP_ENABLE_RENDERDOC=$<BOOL:${HYP_ENABLE_RENDERDOC}>)
if(HYP_ENABLE_RENDERDOC)
  target_link_libraries(hyperion_viewer PRIVATE hyperion_renderdoc hyperion_capture)
endif()
set_target_properties(hyperion_viewer PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
if(CMAKE_GENERATOR MATCHES "^Visual Studio")
  include("${PROJECT_SOURCE_DIR}/cmake/ViewerContent.cmake")
endif()
