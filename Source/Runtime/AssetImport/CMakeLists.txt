add_library(hyperion_asset_import STATIC
  Private/AssetImportService.cpp
  Private/AssetImportProducts.cpp
  Private/ModelImport.cpp Private/AssetPublication.cpp
  Private/AssetLibrary.cpp
  Private/AssetPublicationChecks.cpp Private/AssetPublicationGraph.cpp
  Private/Adapters/AssetSourceJson.cpp Private/Adapters/SceneJson.cpp Private/Adapters/GltfPrimitive.cpp
  Private/Adapters/GltfImport.cpp Private/Adapters/GltfImportInternal.h
  Private/Adapters/GltfAccessors.cpp Private/Adapters/GltfMaterials.cpp
  Private/Adapters/GltfPrimitives.cpp)
hyp_module(hyperion_asset_import)
target_link_libraries(hyperion_asset_import PUBLIC hyperion_assets hyperion_scene hyperion_materials hyperion_textures PRIVATE nlohmann_json::nlohmann_json hyperion_core hyperion_io hyperion_reflection)
target_include_directories(hyperion_asset_import SYSTEM PRIVATE "${HYP_DEPS}/cgltf")
