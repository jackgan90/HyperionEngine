add_library(hyperion_scene STATIC Private/Model.cpp Private/ModelReflection.cpp)
hyp_module(hyperion_scene)
target_link_libraries(hyperion_scene PUBLIC hyperion_math hyperion_reflection)
