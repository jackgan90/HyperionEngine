add_library(hyperion_asset_import STATIC
  Private/ImportSettings.cpp Private/ImageImport.cpp
  Private/ImportPreparation.cpp Private/ImportDraftProperties.cpp Private/ImportDraftTypes.cpp
  Private/ImportDraftWorkspace.cpp Private/ImportDraftInspection.cpp
  Private/ImportWorkspace.cpp Private/ImportWorkspaceTypes.cpp Private/ImportValidation.cpp
  Private/ImportRules.cpp Private/ImportRules.h
  Private/AssetImportService.cpp Private/ImportDestinations.cpp
  Private/AssetImportProducts.cpp
  Private/ImportProductKey.cpp Private/ImportProductKey.h
  Private/ModelImport.cpp Private/ModelImportInternal.h Private/AssetPublication.cpp
  Private/AssetLibrary.cpp
  Private/AssetPublicationProducts.cpp
  Private/AssetPublicationChecks.cpp Private/AssetPublicationGraph.cpp Private/AssetPublicationCommit.cpp
  Private/Adapters/AssetSourceJson.cpp Private/Adapters/SkyImport.cpp Private/Adapters/GltfPrimitive.cpp
  Private/Adapters/GltfImport.cpp Private/Adapters/GltfImportInternal.h
  Private/Adapters/GltfAccessors.cpp Private/Adapters/GltfMaterials.cpp
  Private/Adapters/GltfPrimitives.cpp)
hyp_module(hyperion_asset_import)
target_sources(hyperion_asset_import PRIVATE Private/ImportStateReflection.cpp)
target_link_libraries(hyperion_asset_import PUBLIC hyperion_content)
target_link_libraries(hyperion_asset_import PUBLIC hyperion_asset_editing)
target_link_libraries(hyperion_asset_import PUBLIC hyperion_assets hyperion_scene hyperion_materials hyperion_textures hyperion_environment PRIVATE nlohmann_json::nlohmann_json hyperion_core hyperion_io hyperion_reflection)
target_include_directories(hyperion_asset_import SYSTEM PRIVATE "${HYP_DEPS}/cgltf")
