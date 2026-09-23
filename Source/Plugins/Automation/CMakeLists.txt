add_library(hyperion_automation_plugin STATIC
  Private/AutomationServices.cpp
  Private/LocalPlugin.cpp
  Private/ScenePlugin.cpp
  Private/SceneOperations.cpp
  Private/AssetPlugin.cpp
  Private/AssetOperations.cpp
  Private/AssetOperationTypes.cpp
  Private/AssetRegistration.cpp
  Private/ContentRootOperations.cpp
  Private/StdioPlugin.cpp
  Private/Adapters/StdioReader.cpp)
hyp_module(hyperion_automation_plugin)
target_link_libraries(hyperion_automation_plugin PRIVATE hyperion_core)
string(TIMESTAMP AUTOMATION_BUILD_UTC "%Y%m%dT%H%M%SZ" UTC)
target_compile_definitions(hyperion_automation_plugin PRIVATE HYP_AUTOMATION_BUILD="${PROJECT_VERSION}/${CMAKE_BUILD_TYPE}/${AUTOMATION_BUILD_UTC}")
target_link_libraries(hyperion_automation_plugin PUBLIC hyperion_automation hyperion_plugins
  PRIVATE hyperion_application hyperion_content hyperion_asset_editing hyperion_assets hyperion_tasks hyperion_io
  hyperion_scene hyperion_scene_editing hyperion_materials hyperion_textures hyperion_environment)
if(BUILD_TESTING)
  add_executable(automation_local_tests Private/LocalPluginTests.cpp)
  hyp_executable(automation_local_tests)
  target_link_libraries(automation_local_tests PRIVATE hyperion_automation_plugin hyperion_application)
  add_test(NAME automation_local_lifecycle COMMAND automation_local_tests)
  set_tests_properties(automation_local_lifecycle PROPERTIES TIMEOUT 30)
  add_executable(automation_scene_tests Private/SceneOperationTests.cpp)
  hyp_executable(automation_scene_tests)
  target_link_libraries(automation_scene_tests PRIVATE hyperion_automation_plugin hyperion_scene_editing)
  add_test(NAME automation_scene COMMAND automation_scene_tests)
  add_executable(automation_asset_tests Private/AssetOperationTests.cpp)
  hyp_executable(automation_asset_tests)
  target_link_libraries(automation_asset_tests PRIVATE hyperion_automation_plugin hyperion_asset_editing hyperion_application
    hyperion_assets hyperion_content hyperion_scene hyperion_io hyperion_textures)
  add_test(NAME automation_assets COMMAND automation_asset_tests)
  set_tests_properties(automation_assets PROPERTIES TIMEOUT 60 WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
endif()
