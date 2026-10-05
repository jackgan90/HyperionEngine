add_library(hyperion_render_controls STATIC
  Private/FrameTiming.cpp
  Private/ProfilingHud.cpp
  Private/RenderBatchStats.cpp
  Private/RenderCaptureControl.cpp
  Private/RenderDiagnostics.cpp
  Private/RenderOutput.cpp
  Private/RenderSettings.cpp
  Private/RenderStatistics.cpp
  Private/SceneDiagnostics.cpp
  Private/SceneLightControls.cpp
  Private/SceneViewport.cpp
  Private/ShadowControls.cpp
  Private/ViewportChoices.cpp)
hyp_module(hyperion_render_controls)
target_link_libraries(hyperion_render_controls PUBLIC hyperion_scene hyperion_scene_editing hyperion_math hyperion_reflection hyperion_raster_options)
