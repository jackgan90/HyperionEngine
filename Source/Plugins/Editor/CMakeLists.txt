add_library(hyperion_editor_plugin STATIC
  Private/EditorAcceptance.cpp
  Private/EditorApplication.cpp
  Private/EditorBenchmark.cpp
  Private/EditorDocument.cpp
  Private/EditorDocumentAcceptance.cpp
  Private/EditorFrame.cpp
  Private/EditorGizmo.cpp
  Private/EditorGizmoAcceptance.cpp
  Private/EditorHistory.cpp
  Private/EditorHost.cpp
  Private/EditorInspector.cpp
  Private/EditorOptions.cpp
  Private/EditorPanels.cpp
  Private/EditorPlacement.cpp
  Private/EditorPlacementAcceptance.cpp
  Private/EditorPlacementResources.cpp
  Private/EditorLightMarkers.cpp
  Private/EditorLightMarkerAcceptance.cpp
  Private/EditorTransformAcceptance.cpp
  Private/EditorViewAcceptance.cpp
  Private/EditorViews.cpp
  Private/EditorPicking.cpp
  Private/EditorPickingAcceptance.cpp
)
hyp_module(hyperion_editor_plugin)
target_link_libraries(hyperion_editor_plugin PUBLIC hyperion_application_services
  PRIVATE hyperion_application hyperion_plugins hyperion_gui hyperion_gui_renderer hyperion_render hyperion_assets
  hyperion_asset_types hyperion_scene hyperion_materials hyperion_platform hyperion_io hyperion_core hyperion_tasks
  hyperion_shaders hyperion_rhi hyperion_math hyperion_config)
target_compile_definitions(hyperion_editor_plugin PRIVATE HYP_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
