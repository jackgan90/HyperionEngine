add_library(hyperion_editor_plugin STATIC
  Private/EditorLog.cpp
  Private/EditorStorage.cpp
  Private/EditorClipboard.cpp
  Private/AssetImportPanel.cpp
  Private/ImportPreviewPanel.cpp Private/ImportPreviewProperties.cpp
  Private/EditorStructure.cpp
  Private/EditorReparentController.cpp
  Private/EditorRenderSettings.cpp
  Private/EditorLightControls.cpp
  Private/EditorRenderSettingsGui.cpp
  Private/EditorProfilingGui.cpp
  Private/EditorHud.cpp
  Private/EditorDebugGeometry.cpp
  Private/EditorCloseService.cpp
  Private/EditorViewportService.cpp
  Private/EditorFraming.cpp
  Private/EditorCaptureService.cpp
  Private/AssetPreviewService.cpp
  Private/AssetTextureDisplay.cpp
  Private/EditorSceneDocument.cpp
  Private/EditorApplication.cpp
  Private/EditorReport.cpp
  Private/EditorAcceptanceReport.cpp
  Private/AssetEditorWindow.cpp
  Private/AssetEditorWindowGui.cpp
  Private/EditorAssetWindow.cpp
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
  Private/EditorBenchmark.cpp
  Private/EditorDocument.cpp
  Private/EditorDeletion.cpp
  Private/EditorFrame.cpp
  Private/EditorViewport.cpp
  Private/EditorDocumentTransition.cpp
  Private/EditorGizmo.cpp
  Private/EditorGizmoTransaction.cpp
  Private/EditorHistory.cpp
  Private/EditorShortcuts.cpp
  Private/EditorInteractionPolicy.cpp
  Private/EditorInteractionPolicyCapture.cpp
  Private/EditorSelection.cpp
  Private/EditorBatchHistory.cpp
  Private/EditorHost.cpp
  Private/EditorInspector.cpp
  Private/EditorSelectionInspector.cpp
  Private/EditorOptions.cpp
  Private/EditorPanels.cpp
  Private/EditorOutliner.cpp
  Private/EditorPreferences.cpp
  Private/EditorCapture.cpp
  Private/EditorPlacement.cpp
  Private/PlacementService.cpp
  Private/EditorPlacementResources.cpp
  Private/EditorLightMarkers.cpp
  Private/LightInspection.cpp
  Private/EditorViews.cpp
  Private/EditorPicking.cpp
)
hyp_module(hyperion_editor_plugin)
target_link_libraries(hyperion_editor_plugin PRIVATE hyperion_raster_options)
target_link_libraries(hyperion_editor_plugin PRIVATE hyperion_asset_import)
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
  add_executable(editor_reparent_controller_tests Tests/Unit/EditorReparentControllerTests.cpp Private/EditorReparentController.cpp)
  hyp_executable(editor_reparent_controller_tests)
  target_include_directories(editor_reparent_controller_tests PRIVATE "${PROJECT_SOURCE_DIR}/Source/Tests")
  target_link_libraries(editor_reparent_controller_tests PRIVATE hyperion_gui hyperion_scene_editing hyperion_io)
  set_target_properties(editor_reparent_controller_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_reparent_controller COMMAND editor_reparent_controller_tests)
  add_executable(editor_capture_options_tests Tests/Unit/CaptureExerciseTests.cpp)
  hyp_executable(editor_capture_options_tests)
  target_link_libraries(editor_capture_options_tests PRIVATE hyperion_editor_plugin hyperion_render hyperion_platform)
  set_target_properties(editor_capture_options_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_capture_options COMMAND editor_capture_options_tests)
  add_executable(editor_interaction_policy_tests Tests/Unit/EditorInteractionPolicyTests.cpp Private/EditorInteractionPolicy.cpp)
  hyp_executable(editor_interaction_policy_tests)
  target_link_libraries(editor_interaction_policy_tests PRIVATE hyperion_platform)
  set_target_properties(editor_interaction_policy_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_interaction_policies COMMAND editor_interaction_policy_tests)
  add_test(NAME editor_acceptance_boundary COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/Source/Tests/Integration/EditorAcceptanceBoundary.py" "${PROJECT_BINARY_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/EditorSources-$<CONFIG>.txt")
  add_executable(editor_document_transition_tests Tests/Unit/EditorDocumentTransitionTests.cpp Private/EditorDocumentTransition.cpp)
  hyp_executable(editor_document_transition_tests)
  target_link_libraries(editor_document_transition_tests PRIVATE hyperion_content)
  set_target_properties(editor_document_transition_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_document_transitions COMMAND editor_document_transition_tests)
  add_executable(editor_placement_preparation_tests Tests/Unit/PlacementPreparationTests.cpp)
  hyp_executable(editor_placement_preparation_tests)
  target_link_libraries(editor_placement_preparation_tests PRIVATE hyperion_editor_plugin hyperion_render hyperion_scene_editing)
  set_target_properties(editor_placement_preparation_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_placement_preparation COMMAND editor_placement_preparation_tests)
  add_executable(editor_light_inspection_tests Tests/Unit/LightInspectionTests.cpp Private/LightInspection.cpp)
  hyp_executable(editor_light_inspection_tests)
  target_link_libraries(editor_light_inspection_tests PRIVATE hyperion_render)
  set_target_properties(editor_light_inspection_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_light_inspection COMMAND editor_light_inspection_tests)
  target_sources(hyperion_editor_plugin PRIVATE
    Tests/Acceptance/EditorAcceptanceDriver.cpp
    Tests/Acceptance/EditorAcceptanceHarness.cpp
    Tests/Acceptance/EditorAcceptanceHooks.cpp
    Tests/Acceptance/EditorAcceptanceObservations.cpp
    Tests/Acceptance/EditorLogAcceptance.cpp
    Tests/Acceptance/EditorClipboardAcceptance.cpp
    Tests/Acceptance/EditorImportAcceptance.cpp
    Tests/Acceptance/EditorReparentAcceptance.cpp
    Tests/Acceptance/EditorRenderControlsAcceptance.cpp
    Tests/Acceptance/EditorRasterOptionsAcceptance.cpp
    Tests/Acceptance/EditorViewportChoicesAcceptance.cpp
    Tests/Acceptance/EditorFramingAcceptance.cpp
    Tests/Acceptance/EditorAcceptance.cpp
    Tests/Acceptance/EditorAssetAcceptance.cpp
    Tests/Acceptance/EditorAssetPropertyAcceptance.cpp
    Tests/Acceptance/EditorAssetWorkspaceAcceptance.cpp
    Tests/Acceptance/EditorAssetWindowAcceptance.cpp
    Tests/Acceptance/EditorAssetAuditAcceptance.cpp
    Tests/Acceptance/EditorAssetPreviewAcceptance.cpp
    Tests/Acceptance/EditorContentAcceptance.cpp
    Tests/Acceptance/EditorDeletionAcceptance.cpp
    Tests/Acceptance/EditorDocumentAcceptance.cpp
    Tests/Acceptance/EditorGizmoAcceptance.cpp
    Tests/Acceptance/EditorMultiSelectionAcceptance.cpp
    Tests/Acceptance/EditorMultiTransformAcceptance.cpp
    Tests/Acceptance/EditorSelectionShortcutsAcceptance.cpp
    Tests/Acceptance/EditorShortcutRoutingAcceptance.cpp
    Tests/Acceptance/EditorOutlineAcceptance.cpp
    Tests/Acceptance/EditorCaptureAcceptance.cpp
    Tests/Acceptance/EditorPlacementAcceptance.cpp
    Tests/Acceptance/EditorModelPlacementAcceptance.cpp
    Tests/Acceptance/EditorLightMarkerAcceptance.cpp
    Tests/Acceptance/EditorTransformAcceptance.cpp
    Tests/Acceptance/EditorViewAcceptance.cpp
    Tests/Acceptance/EditorPickingAcceptance.cpp
  )
  add_executable(editor_import_choices_tests Tests/Unit/ImportChoicesTests.cpp)
  add_executable(editor_material_choices_tests Tests/Unit/MaterialChoiceTests.cpp)
  hyp_executable(editor_material_choices_tests)
  target_link_libraries(editor_material_choices_tests PRIVATE hyperion_materials)
  target_include_directories(editor_material_choices_tests PRIVATE "${PROJECT_SOURCE_DIR}/Source/Tests")
  set_target_properties(editor_material_choices_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_material_choices COMMAND editor_material_choices_tests)
  hyp_executable(editor_import_choices_tests)
  target_link_libraries(editor_import_choices_tests PRIVATE hyperion_asset_import hyperion_platform)
  set_target_properties(editor_import_choices_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_import_choices COMMAND editor_import_choices_tests)
  add_executable(editor_asset_workspace_tests Tests/Unit/AssetWorkspaceTests.cpp Tests/Unit/AssetMaterialInteractionTests.cpp)
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
  add_executable(editor_asset_preview_contract_tests Tests/Unit/AssetPreviewContractTests.cpp Private/AssetTextureDisplay.cpp)
  hyp_executable(editor_asset_preview_contract_tests)
  target_link_libraries(editor_asset_preview_contract_tests PRIVATE hyperion_asset_editing hyperion_environment hyperion_textures hyperion_tasks)
  set_target_properties(editor_asset_preview_contract_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_asset_preview_contracts COMMAND editor_asset_preview_contract_tests)
  add_executable(editor_asset_document_tests Tests/Unit/AssetDocumentTests.cpp)
  hyp_executable(editor_asset_document_tests)
  target_sources(editor_asset_document_tests PRIVATE "${PROJECT_SOURCE_DIR}/Source/Tests/Assets/EditorAssetFixture.cpp")
  target_link_libraries(editor_asset_document_tests PRIVATE hyperion_asset_editing hyperion_assets hyperion_textures hyperion_scene hyperion_environment hyperion_materials)
  target_include_directories(editor_asset_document_tests PRIVATE "${PROJECT_SOURCE_DIR}/Source/Tests")
  target_compile_definitions(editor_asset_document_tests PRIVATE HYP_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  set_target_properties(editor_asset_document_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_asset_documents COMMAND editor_asset_document_tests)
  set_tests_properties(editor_asset_documents PROPERTIES WORKING_DIRECTORY "${PROJECT_BINARY_DIR}" FIXTURES_SETUP editor_asset_fixture)
  hyp_discovery_test(NAME editor_asset_editors COMMAND $<TARGET_FILE:hyperion_editor>
    --exercise-assets "${PROJECT_BINARY_DIR}/editor-asset-tests"
    --editor-preferences "${PROJECT_BINARY_DIR}/editor-asset-tests/EditorPreferences.ini"
    --layout "${PROJECT_BINARY_DIR}/editor-asset-tests/Layout.ini" --hidden)
  set_tests_properties(editor_asset_editors PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    FIXTURES_REQUIRED editor_asset_fixture TIMEOUT 150 RUN_SERIAL TRUE LABELS "gpu;desktop")
  add_executable(editor_content_tests Tests/Unit/ContentBrowserTests.cpp Private/ContentBrowser.cpp Private/EditorPreferences.cpp)
  hyp_executable(editor_content_tests)
  target_compile_definitions(editor_content_tests PRIVATE HYP_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  target_link_libraries(editor_content_tests PRIVATE hyperion_application_services hyperion_assets hyperion_content hyperion_io hyperion_scene hyperion_textures hyperion_tasks)
  set_target_properties(editor_content_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_content COMMAND editor_content_tests)
  set_tests_properties(editor_content PROPERTIES WORKING_DIRECTORY "${PROJECT_BINARY_DIR}" FIXTURES_SETUP editor_content_fixture)
  hyp_discovery_test(NAME editor_content_transition COMMAND $<TARGET_FILE:hyperion_editor>
    --exercise-content "${PROJECT_BINARY_DIR}/editor-content-tests"
    --editor-preferences "${PROJECT_BINARY_DIR}/editor-content-tests/EditorPreferences.ini"
    --layout "${PROJECT_BINARY_DIR}/editor-content-tests/Layout.ini"
    --capture "${PROJECT_BINARY_DIR}/editor-content-tests/Browser.png" --hidden)
  set_tests_properties(editor_content_transition PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    FIXTURES_REQUIRED editor_content_fixture TIMEOUT 120 RUN_SERIAL TRUE LABELS "gpu;desktop")
  hyp_discovery_test(NAME editor_content_startup COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/Source/Tests/Integration/ContentRootAcceptance.py" $<TARGET_FILE:hyperion_editor>
    "${PROJECT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}/editor-content-tests")
  set_tests_properties(editor_content_startup PROPERTIES FIXTURES_REQUIRED editor_content_fixture TIMEOUT 150 RUN_SERIAL TRUE LABELS "gpu;desktop")
  add_executable(editor_state_tests Tests/Unit/EditorStateTests.cpp)
  hyp_executable(editor_state_tests)
  target_link_libraries(editor_state_tests PRIVATE hyperion_scene_editing)
  set_target_properties(editor_state_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_state COMMAND editor_state_tests)
  add_executable(editor_preferences_tests Tests/Unit/EditorPreferencesTests.cpp Private/EditorPreferences.cpp)
  hyp_executable(editor_preferences_tests)
  target_link_libraries(editor_preferences_tests PRIVATE hyperion_io)
  set_target_properties(editor_preferences_tests PROPERTIES FOLDER "Hyperion/Tests")
  add_test(NAME editor_preferences COMMAND editor_preferences_tests)
  set_tests_properties(editor_preferences PROPERTIES WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
endif()

target_link_libraries(hyperion_editor_plugin PRIVATE hyperion_automation_plugin hyperion_scene_editing hyperion_reflection)

if(NOT BUILD_TESTING)
  target_sources(hyperion_editor_plugin PRIVATE Private/EditorAcceptanceUnavailable.cpp)
endif()
target_compile_definitions(hyperion_editor_plugin PRIVATE HYP_BUILD_TESTING=$<BOOL:${BUILD_TESTING}>)

# Use the target's actual sources for boundary checks with every supported generator.
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/EditorSources-$<CONFIG>.txt"
  CONTENT "$<JOIN:$<TARGET_GENEX_EVAL:hyperion_editor_plugin,$<TARGET_PROPERTY:hyperion_editor_plugin,SOURCES>>,\n>\n")
