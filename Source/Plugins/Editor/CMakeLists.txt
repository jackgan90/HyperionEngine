add_library(hyperion_editor_plugin STATIC
  Private/EditorAcceptance.cpp
  Private/EditorApplication.cpp
  Private/EditorBenchmark.cpp
  Private/EditorDocument.cpp
  Private/EditorDeletion.cpp
  Private/EditorDeletionAcceptance.cpp
  Private/EditorDocumentAcceptance.cpp
  Private/EditorFrame.cpp
  Private/EditorGizmo.cpp
  Private/EditorGizmoTransaction.cpp
  Private/EditorGizmoAcceptance.cpp
  Private/EditorMultiSelectionAcceptance.cpp
  Private/EditorMultiTransformAcceptance.cpp
  Private/EditorHistory.cpp
  Private/EditorSelection.cpp
  Private/EditorBatchHistory.cpp
  Private/EditorHost.cpp
  Private/EditorInspector.cpp
  Private/EditorSelectionInspector.cpp
  Private/EditorOptions.cpp
  Private/EditorOutlineAcceptance.cpp
  Private/EditorPanels.cpp
  Private/EditorPreferences.cpp
  Private/EditorCapture.cpp
  Private/EditorCaptureAcceptance.cpp
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
target_compile_definitions(hyperion_editor_plugin PRIVATE HYP_ENABLE_RENDERDOC=$<BOOL:${HYP_ENABLE_RENDERDOC}>)
if(HYP_ENABLE_RENDERDOC)
  target_link_libraries(hyperion_editor_plugin PRIVATE hyperion_renderdoc hyperion_capture)
endif()
if(BUILD_TESTING)
  add_executable(editor_state_tests Private/EditorStateTests.cpp)
  hyp_executable(editor_state_tests)
  target_link_libraries(editor_state_tests PRIVATE hyperion_scene)
  set_target_properties(editor_state_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_state COMMAND editor_state_tests)
  add_executable(editor_preferences_tests Private/EditorPreferencesTests.cpp Private/EditorPreferences.cpp)
  hyp_executable(editor_preferences_tests)
  target_link_libraries(editor_preferences_tests PRIVATE hyperion_io)
  set_target_properties(editor_preferences_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_preferences COMMAND editor_preferences_tests)
  set_tests_properties(editor_preferences PROPERTIES WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
endif()
