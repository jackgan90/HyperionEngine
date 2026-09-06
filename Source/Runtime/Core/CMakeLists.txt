add_library(hyperion_core STATIC Private/Adapters/Core.cpp)
hyp_module(hyperion_core)
target_link_libraries(hyperion_core PRIVATE spdlog::spdlog mimalloc-static Tracy::TracyClient)
