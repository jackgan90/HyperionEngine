add_library(hyperion_render STATIC Private/RenderGraph.cpp
  Private/RenderPrimitive.cpp Private/RenderScene.cpp Private/RenderSceneClient.cpp
  Private/ModelPreparation.cpp Private/ModelResources.cpp Private/RenderResourceLifetime.cpp
  Private/RenderResourceService.cpp Private/SceneVisibility.cpp Private/SceneDraws.cpp
  Private/Model.cpp Private/RenderSession.cpp)
hyp_module(hyperion_render)
target_link_libraries(hyperion_render PUBLIC hyperion_rhi hyperion_tasks hyperion_plugins hyperion_config hyperion_scene PRIVATE hyperion_core)
