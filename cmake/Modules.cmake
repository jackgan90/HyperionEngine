# Owned modules export only their Public directory. Native headers remain private.
# Stage shared DLLs once per build. Per-executable post-build copies race under MSBuild /m.
get_property(hyp_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
set(hyp_runtime_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
if(hyp_multi_config)
  string(APPEND hyp_runtime_directory "/$<CONFIG>")
endif()
add_custom_target(hyperion_runtime_files
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${hyp_runtime_directory}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:TBB::tbb>"
    "${HYP_DEPS}/dxc/bin/x64/dxcompiler.dll" "${HYP_DEPS}/dxc/bin/x64/dxil.dll" "${hyp_runtime_directory}"
  DEPENDS TBB::tbb
  VERBATIM)
set_target_properties(hyperion_runtime_files PROPERTIES FOLDER "Hyperion/Build")
function(hyp_module target)
  target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Public")
  target_compile_features(${target} PUBLIC cxx_std_20)
  target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /EHsc)
  target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  file(GLOB_RECURSE module_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Public/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/Private/*.h")
  target_sources(${target} PRIVATE ${module_headers})
  file(RELATIVE_PATH module_path "${PROJECT_SOURCE_DIR}/Source" "${CMAKE_CURRENT_SOURCE_DIR}")
  set_target_properties(${target} PROPERTIES FOLDER "Hyperion/${module_path}" HYP_OWNED TRUE)
  get_target_property(module_sources ${target} SOURCES)
  set(absolute_sources "")
  foreach(source IN LISTS module_sources)
    get_filename_component(absolute "${source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    list(APPEND absolute_sources "${absolute}")
  endforeach()
  source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${absolute_sources})
endfunction()
function(hyp_executable target)
  hyp_module(${target})
  add_dependencies(${target} hyperion_runtime_files)
endfunction()
