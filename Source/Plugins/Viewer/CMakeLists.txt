add_library(hyperion_viewer_plugin STATIC
  Private/ViewerApplication.cpp
  Private/ViewerBenchmark.cpp
  Private/ViewerCapture.cpp
  Private/ViewerContactAcceptance.cpp
  Private/ViewerFrame.cpp
  Private/ViewerFramePipeline.cpp
  Private/ViewerHost.cpp
  Private/ViewerOptions.cpp
  Private/ViewerOutput.cpp
  Private/ViewerProfiling.cpp
  Private/ViewerShadows.cpp
)
hyp_module(hyperion_viewer_plugin)
target_link_libraries(hyperion_viewer_plugin PUBLIC hyperion_application_services
  PRIVATE hyperion_application hyperion_plugins hyperion_gui hyperion_gui_renderer hyperion_render hyperion_assets
  hyperion_asset_types hyperion_scene hyperion_materials hyperion_platform hyperion_io hyperion_core hyperion_tasks
  hyperion_shaders hyperion_rhi hyperion_math hyperion_config)
target_compile_definitions(hyperion_viewer_plugin PRIVATE HYP_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_compile_definitions(hyperion_viewer_plugin PRIVATE HYP_ENABLE_TRIANGLE=$<BOOL:${HYP_ENABLE_TRIANGLE}>)
if(HYP_ENABLE_TRIANGLE)
  target_link_libraries(hyperion_viewer_plugin PRIVATE hyperion_triangle)
endif()
target_compile_definitions(hyperion_viewer_plugin PRIVATE HYP_ENABLE_MODEL_VIEWER=$<BOOL:${HYP_ENABLE_MODEL_VIEWER}>)
if(HYP_ENABLE_MODEL_VIEWER)
  target_link_libraries(hyperion_viewer_plugin PRIVATE hyperion_model_viewer)
endif()
target_compile_definitions(hyperion_viewer_plugin PRIVATE HYP_ENABLE_SCENE_VIEWER=$<BOOL:${HYP_ENABLE_SCENE_VIEWER}>)
if(HYP_ENABLE_SCENE_VIEWER)
  target_link_libraries(hyperion_viewer_plugin PRIVATE hyperion_scene_viewer)
endif()
target_compile_definitions(hyperion_viewer_plugin PRIVATE HYP_ENABLE_DEBUG_UI=$<BOOL:${HYP_ENABLE_DEBUG_UI}>)
if(HYP_ENABLE_DEBUG_UI)
  target_link_libraries(hyperion_viewer_plugin PRIVATE hyperion_debug_ui)
endif()
target_compile_definitions(hyperion_viewer_plugin PRIVATE HYP_ENABLE_RENDERDOC=$<BOOL:${HYP_ENABLE_RENDERDOC}>)
if(HYP_ENABLE_RENDERDOC)
  target_link_libraries(hyperion_viewer_plugin PRIVATE hyperion_renderdoc)
endif()
if(HYP_ENABLE_RENDERDOC)
  target_link_libraries(hyperion_viewer_plugin PRIVATE hyperion_capture)
endif()
