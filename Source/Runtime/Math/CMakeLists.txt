add_library(hyperion_math STATIC Private/Adapters/Math.cpp)
hyp_module(hyperion_math)
target_link_libraries(hyperion_math PRIVATE glm::glm)
