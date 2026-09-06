add_library(hyperion_platform STATIC Private/Adapters/SDLWindow.cpp)
hyp_module(hyperion_platform)
target_link_libraries(hyperion_platform PUBLIC hyperion_core PRIVATE SDL3::SDL3-static)
