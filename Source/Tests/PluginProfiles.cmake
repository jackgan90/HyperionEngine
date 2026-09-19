# Reduced plugin builds exercise the host and runtime contracts without linking excluded plugins.
hyp_test(core_tests core hyperion_core Core/CoreTests.cpp)
hyp_test(task_tests tasks hyperion_tasks Tasks/TaskTests.cpp)
hyp_test(config_tests configuration_plugins hyperion_config Config/ConfigTests.cpp)
target_link_libraries(config_tests PRIVATE hyperion_plugins)
hyp_test(plugin_tests plugin_runtime hyperion_application Plugins/PluginTests.cpp)
hyp_test(graph_tests render_graph hyperion_render Renderer/GraphTests.cpp)
target_sources(graph_tests PRIVATE Renderer/ComputeGraphTests.cpp)
add_test(NAME plugin_applications COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Integration/PluginAcceptance.py" $<TARGET_FILE:hyperion_viewer> $<TARGET_FILE:hyperion_editor> "${PROJECT_SOURCE_DIR}")
set_tests_properties(plugin_applications PROPERTIES TIMEOUT 180 LABELS "gpu;desktop" RUN_SERIAL TRUE)
add_test(NAME dependency_boundaries COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/CheckBoundaries.py")
add_test(NAME code_style_paths COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/CheckStyle.py" --paths-only)
get_property(hyperion_profile_tests DIRECTORY PROPERTY TESTS)
set_tests_properties(${hyperion_profile_tests} PROPERTIES WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
add_custom_target(hyperion_check
  COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${PROJECT_BINARY_DIR}" -C "$<CONFIG>" --output-on-failure
  DEPENDS core_tests task_tests config_tests plugin_tests graph_tests hyperion_viewer hyperion_editor d3d12_frame_failure_tests
  USES_TERMINAL)
