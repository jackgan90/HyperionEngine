add_library(hyperion_scene STATIC Private/Model.cpp Private/ModelReflection.cpp Private/Scene.cpp Private/SceneMaterials.cpp Private/SceneModel.cpp Private/SceneManifest.cpp)
hyp_module(hyperion_scene)
target_link_libraries(hyperion_scene PUBLIC hyperion_asset_types hyperion_math hyperion_reflection hyperion_materials)
