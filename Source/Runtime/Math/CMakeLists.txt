add_library(hyperion_math STATIC Private/Adapters/Math.cpp Private/Adapters/AffineTransform.cpp Private/Bounds.cpp Private/Ray.cpp Private/BoundsBvh.cpp)
hyp_module(hyperion_math)
target_sources(hyperion_math PRIVATE Private/StaticBoundsBvh.cpp)
target_link_libraries(hyperion_math PRIVATE glm::glm)
