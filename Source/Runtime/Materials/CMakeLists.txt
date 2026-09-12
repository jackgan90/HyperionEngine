add_library(hyperion_materials STATIC
  Private/MaterialBlocks.cpp
  Private/PbrMaterial.cpp
  Private/MaterialAsset.cpp
  Private/MaterialAssetEnums.cpp
  Private/MaterialAssetReflection.cpp
  Private/MaterialStateReflection.cpp
  Private/MaterialResources.cpp
  Private/MaterialParameters.cpp
  Private/MaterialSemantics.cpp
  Private/MaterialDefinition.cpp
  Private/MaterialInstance.cpp)
hyp_module(hyperion_materials)
target_link_libraries(hyperion_materials PUBLIC hyperion_core hyperion_math hyperion_asset_types hyperion_reflection hyperion_textures)
