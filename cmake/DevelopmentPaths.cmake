find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(hyp_path_environment "")
if(HYP_TOOL_CACHE)
  list(APPEND hyp_path_environment "HYP_TOOL_CACHE=${HYP_TOOL_CACHE}")
endif()
if(HYP_OUTPUT_ROOT)
  list(APPEND hyp_path_environment "HYP_OUT_ROOT=${HYP_OUTPUT_ROOT}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${hyp_path_environment}
  "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/DevelopmentPaths.py"
  OUTPUT_VARIABLE hyp_development_paths OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY)
string(JSON hyp_output_default GET "${hyp_development_paths}" out)
string(JSON hyp_tool_cache_default GET "${hyp_development_paths}" tool-cache)
string(JSON hyp_deps_default GET "${hyp_development_paths}" deps)
set(HYP_OUTPUT_ROOT "${hyp_output_default}" CACHE PATH "Regenerable build and test outputs")
set(HYP_TOOL_CACHE "${hyp_tool_cache_default}" CACHE PATH "Independent locked developer tool cache")
set(HYP_DEPS "${hyp_deps_default}" CACHE PATH "Locked dependency sources (Bootstrap --print-deps)")
set(HYP_TEST_OUTPUT_DIR "${HYP_OUTPUT_ROOT}/tests")
