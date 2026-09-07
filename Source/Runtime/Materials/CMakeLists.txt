add_library(hyperion_materials STATIC
  Private/MaterialResources.cpp
  Private/MaterialParameters.cpp
  Private/MaterialSemantics.cpp
  Private/MaterialDefinition.cpp
  Private/MaterialInstance.cpp)
hyp_module(hyperion_materials)
target_link_libraries(hyperion_materials PUBLIC hyperion_core hyperion_math)
