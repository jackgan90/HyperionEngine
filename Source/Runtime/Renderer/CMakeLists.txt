add_library(hyperion_render STATIC Private/RenderGraph.cpp
  Private/MaterialInterface.cpp Private/MaterialPreparation.cpp Private/MaterialPipeline.cpp Private/MaterialPacking.cpp Private/MaterialBlocks.cpp
  Private/MaterialBindingContext.cpp
  Private/MaterialConstantCache.cpp
  Private/MaterialProviders.cpp
  Private/MaterialGpuCache.cpp Private/MaterialGpuSources.cpp Private/MaterialGpuLifetime.cpp
  Private/RenderMaterial.cpp Private/MaterialResourceService.cpp Private/MaterialResourceProgress.cpp
  Private/RenderResourceClose.cpp Private/SessionMaterialFrame.cpp Private/SessionMaterialBinding.cpp Private/SessionMaterialEvaluation.cpp
  Private/RenderPrimitive.cpp Private/PrimitiveMaterialParameters.cpp Private/RenderScene.cpp Private/RenderSceneClient.cpp
  Private/ModelPreparation.cpp Private/ModelResources.cpp Private/ModelMaterials.cpp Private/RenderResourceLifetime.cpp
  Private/RenderResourceService.cpp Private/SceneVisibility.cpp Private/SceneDraws.cpp Private/MaterialDraw.cpp
  Private/Model.cpp Private/RenderSession.cpp Private/SceneBvh.cpp Private/SceneCollection.cpp Private/SceneRemoval.cpp Private/SceneBridge.cpp Private/SceneMaterialPublication.cpp)
hyp_module(hyperion_render)
target_link_libraries(hyperion_render PUBLIC hyperion_rhi hyperion_tasks hyperion_plugins hyperion_config hyperion_scene hyperion_math hyperion_materials hyperion_shaders PRIVATE hyperion_core)
