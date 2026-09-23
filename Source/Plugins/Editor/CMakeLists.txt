add_library(hyperion_editor_plugin STATIC
  Private/EditorCloseService.cpp
  Private/EditorViewportService.cpp
  Private/EditorCaptureService.cpp
  Private/AssetPreviewService.cpp
  Private/EditorSceneDocument.cpp
  Private/EditorAcceptance.cpp
  Private/EditorAssetAcceptance.cpp
  Private/EditorAssetPropertyAcceptance.cpp
  Private/EditorAssetWorkspaceAcceptance.cpp
  Private/EditorApplication.cpp
  Private/AssetEditorWindow.cpp
  Private/AssetEditorWindowGui.cpp
  Private/EditorAssetWindow.cpp
  Private/EditorAssetWindowAcceptance.cpp
  Private/EditorAssetAuditAcceptance.cpp
  Private/EditorAssetPreviewAcceptance.cpp
  Private/AssetWorkspace.cpp
  Private/AssetWorkspaceService.cpp
  Private/AssetPreview.cpp
  Private/AssetProperties.cpp
  Private/AssetReferenceEditor.cpp
  Private/AssetTextureEditor.cpp
  Private/AssetMaterialEditor.cpp
  Private/ContentBrowser.cpp
  Private/EditorContent.cpp
  Private/EditorContentTransition.cpp
  Private/EditorContentAcceptance.cpp
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
target_link_libraries(hyperion_editor_plugin PRIVATE hyperion_content)
target_link_libraries(hyperion_editor_plugin PUBLIC hyperion_application_services
  PRIVATE hyperion_application hyperion_plugins hyperion_gui hyperion_gui_renderer hyperion_render hyperion_assets hyperion_asset_editing
  hyperion_asset_types hyperion_scene hyperion_materials hyperion_platform hyperion_io hyperion_core hyperion_tasks
  hyperion_shaders hyperion_rhi hyperion_math hyperion_config hyperion_textures hyperion_environment)
target_compile_definitions(hyperion_editor_plugin PRIVATE HYP_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_compile_definitions(hyperion_editor_plugin PRIVATE HYP_ENABLE_RENDERDOC=$<BOOL:${HYP_ENABLE_RENDERDOC}>)
if(HYP_ENABLE_RENDERDOC)
  target_link_libraries(hyperion_editor_plugin PRIVATE hyperion_renderdoc hyperion_capture)
endif()
if(BUILD_TESTING)
  add_executable(editor_asset_workspace_tests Private/AssetWorkspaceTests.cpp)
  hyp_executable(editor_asset_workspace_tests)
  target_sources(editor_asset_workspace_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/Source/Tests/Renderer/AssetWorkspaceTests.cpp"
    "${PROJECT_SOURCE_DIR}/Source/Tests/Assets/EditorAssetFixture.cpp")
  target_link_libraries(editor_asset_workspace_tests PRIVATE hyperion_editor_plugin hyperion_render hyperion_asset_editing
    hyperion_gui hyperion_gui_renderer hyperion_assets hyperion_scene hyperion_environment hyperion_materials hyperion_rhi_d3d12)
  target_include_directories(editor_asset_workspace_tests PRIVATE "${PROJECT_SOURCE_DIR}/Source/Tests")
  target_compile_definitions(editor_asset_workspace_tests PRIVATE HYP_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  set_target_properties(editor_asset_workspace_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_asset_workspace COMMAND editor_asset_workspace_tests)
  set_tests_properties(editor_asset_workspace PROPERTIES WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
    TIMEOUT 120 RUN_SERIAL TRUE LABELS "gpu")
  add_executable(editor_asset_document_tests Private/AssetDocumentTests.cpp)
  hyp_executable(editor_asset_document_tests)
  target_sources(editor_asset_document_tests PRIVATE "${PROJECT_SOURCE_DIR}/Source/Tests/Assets/EditorAssetFixture.cpp")
  target_link_libraries(editor_asset_document_tests PRIVATE hyperion_asset_editing hyperion_assets hyperion_textures hyperion_scene hyperion_environment hyperion_materials)
  target_include_directories(editor_asset_document_tests PRIVATE "${PROJECT_SOURCE_DIR}/Source/Tests")
  target_compile_definitions(editor_asset_document_tests PRIVATE HYP_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  set_target_properties(editor_asset_document_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_asset_documents COMMAND editor_asset_document_tests)
  set_tests_properties(editor_asset_documents PROPERTIES WORKING_DIRECTORY "${PROJECT_BINARY_DIR}" FIXTURES_SETUP editor_asset_fixture)
  add_test(NAME editor_asset_editors COMMAND hyperion_editor
    --exercise-assets "${PROJECT_BINARY_DIR}/editor-asset-tests"
    --editor-preferences "${PROJECT_BINARY_DIR}/editor-asset-tests/EditorPreferences.ini"
    --layout "${PROJECT_BINARY_DIR}/editor-asset-tests/Layout.ini" --hidden)
  set_tests_properties(editor_asset_editors PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    FIXTURES_REQUIRED editor_asset_fixture TIMEOUT 150 RUN_SERIAL TRUE LABELS "gpu;desktop")
  add_executable(editor_content_tests Private/ContentBrowserTests.cpp Private/ContentBrowser.cpp Private/EditorPreferences.cpp)
  hyp_executable(editor_content_tests)
  target_compile_definitions(editor_content_tests PRIVATE HYP_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  target_link_libraries(editor_content_tests PRIVATE hyperion_application_services hyperion_assets hyperion_io hyperion_scene hyperion_textures hyperion_tasks)
  set_target_properties(editor_content_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_content COMMAND editor_content_tests)
  set_tests_properties(editor_content PROPERTIES WORKING_DIRECTORY "${PROJECT_BINARY_DIR}" FIXTURES_SETUP editor_content_fixture)
  add_test(NAME editor_content_transition COMMAND hyperion_editor
    --exercise-content "${PROJECT_BINARY_DIR}/editor-content-tests"
    --editor-preferences "${PROJECT_BINARY_DIR}/editor-content-tests/EditorPreferences.ini"
    --layout "${PROJECT_BINARY_DIR}/editor-content-tests/Layout.ini"
    --capture "${PROJECT_BINARY_DIR}/editor-content-tests/Browser.png" --hidden)
  set_tests_properties(editor_content_transition PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    FIXTURES_REQUIRED editor_content_fixture TIMEOUT 120 RUN_SERIAL TRUE LABELS "gpu;desktop")
  add_test(NAME editor_content_startup COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/Source/Tests/Integration/ContentRootAcceptance.py" $<TARGET_FILE:hyperion_editor>
    "${PROJECT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}/editor-content-tests")
  set_tests_properties(editor_content_startup PROPERTIES FIXTURES_REQUIRED editor_content_fixture TIMEOUT 150 RUN_SERIAL TRUE LABELS "gpu;desktop")
  add_executable(editor_state_tests Private/EditorStateTests.cpp)
  hyp_executable(editor_state_tests)
  target_link_libraries(editor_state_tests PRIVATE hyperion_scene_editing)
  set_target_properties(editor_state_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_state COMMAND editor_state_tests)
  add_executable(editor_preferences_tests Private/EditorPreferencesTests.cpp Private/EditorPreferences.cpp)
  hyp_executable(editor_preferences_tests)
  target_link_libraries(editor_preferences_tests PRIVATE hyperion_io)
  set_target_properties(editor_preferences_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_preferences COMMAND editor_preferences_tests)
  set_tests_properties(editor_preferences PROPERTIES WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
endif()

target_link_libraries(hyperion_editor_plugin PRIVATE hyperion_automation_plugin hyperion_scene_editing)
