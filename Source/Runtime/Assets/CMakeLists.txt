add_library(hyperion_assets STATIC Private/AssetReference.cpp Private/AssetService.cpp Private/Adapters/GltfMesh.cpp Private/Adapters/Images.cpp)
hyp_module(hyperion_assets)
target_link_libraries(hyperion_assets PUBLIC hyperion_reflection hyperion_math hyperion_io hyperion_serialization PRIVATE hyperion_core hyp_miniz)
target_include_directories(hyperion_assets SYSTEM PRIVATE "${HYP_DEPS}/cgltf" "${HYP_DEPS}/stb" "${HYP_DEPS}/tinyexr")
