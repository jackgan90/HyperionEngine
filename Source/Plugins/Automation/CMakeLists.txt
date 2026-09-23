add_library(hyperion_automation_plugin STATIC
  Private/AutomationServices.cpp
  Private/AssetPlugin.cpp
  Private/AssetOperations.cpp
  Private/AssetOperationTypes.cpp
  Private/AssetRegistration.cpp
  Private/ContentRootOperations.cpp
  Private/StdioPlugin.cpp
  Private/Adapters/StdioReader.cpp)
hyp_module(hyperion_automation_plugin)
target_link_libraries(hyperion_automation_plugin PUBLIC hyperion_automation hyperion_plugins
  PRIVATE hyperion_application hyperion_content hyperion_asset_editing hyperion_assets hyperion_tasks hyperion_io
  hyperion_scene hyperion_materials hyperion_textures hyperion_environment)
if(BUILD_TESTING)
  add_executable(automation_asset_tests Private/AssetOperationTests.cpp)
  hyp_executable(automation_asset_tests)
  target_link_libraries(automation_asset_tests PRIVATE hyperion_automation_plugin hyperion_asset_editing hyperion_application
    hyperion_assets hyperion_content hyperion_scene hyperion_io hyperion_textures)
  add_test(NAME automation_assets COMMAND automation_asset_tests)
  set_tests_properties(automation_assets PROPERTIES TIMEOUT 60 WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
endif()
