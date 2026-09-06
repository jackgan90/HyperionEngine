add_library(hyperion_asset_import STATIC
  Private/Adapters/GltfImport.cpp Private/Adapters/GltfImportInternal.h
  Private/Adapters/GltfAccessors.cpp Private/Adapters/GltfMaterials.cpp
  Private/Adapters/GltfPrimitives.cpp)
hyp_module(hyperion_asset_import)
target_link_libraries(hyperion_asset_import PUBLIC hyperion_assets hyperion_scene PRIVATE hyperion_core hyperion_io hyperion_reflection)
target_include_directories(hyperion_asset_import SYSTEM PRIVATE "${HYP_DEPS}/cgltf")
