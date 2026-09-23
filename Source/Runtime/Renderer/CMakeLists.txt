add_library(hyperion_render STATIC Private/SceneCameraController.cpp Private/SceneNavigation.cpp Private/SceneFrame.cpp Private/LocalLights.cpp Private/LightVolumePass.cpp Private/LightVolumeResources.cpp Private/ScenePipelineLocalLights.cpp Private/SceneMetadata.cpp Private/SceneBridgeMetadata.cpp Private/SceneDiagnostics.cpp Private/SceneInstanceSnapshot.cpp Private/SceneInstanceNodes.cpp
  Private/SceneMaterialAssets.cpp Private/EnvironmentParameters.cpp Private/ScenePipelineSky.cpp Private/ClusteredLights.cpp Private/ScenePipelineClusters.cpp
  Private/SceneRefreshResources.cpp Private/SceneInstanceRefresh.cpp Private/SceneInstanceMaterials.cpp Private/SceneInstanceSky.cpp Private/SceneInstance.cpp Private/SceneInstanceLoading.cpp Private/RenderPass.cpp Private/RenderPassDeclaration.cpp Private/RenderGraph.cpp Private/RenderGraphResources.cpp Private/RenderGraphValidation.cpp Private/RenderGraphContent.cpp
  Private/RenderGraphPreparation.cpp Private/RenderGraphExecution.cpp Private/FramePipeline.cpp
  Private/CascadedShadowMap.cpp Private/ShadowParameters.cpp Private/ForwardRenderPipeline.cpp Private/SceneRenderPipeline.cpp Private/ScenePipelineTargets.cpp Private/ScenePipelineViews.cpp Private/ScenePipelineFullscreen.cpp Private/DepthPreview.cpp Private/FullscreenPass.cpp
  Private/MaterialInterface.cpp Private/MaterialPreparation.cpp Private/MaterialPipeline.cpp Private/MaterialPacking.cpp Private/MaterialBlocks.cpp Private/MaterialInstances.cpp
  Private/MaterialBindingContext.cpp Private/MaterialProfiling.cpp
  Private/MaterialConstantCache.cpp
  Private/RenderBatch.cpp Private/RenderBatchStats.cpp Private/BatchCache.cpp Private/BatchPlanCache.cpp Private/IncrementalBatchHistory.cpp Private/IncrementalBatchPlan.cpp Private/BatchPreparation.cpp Private/BatchCompatibility.cpp Private/InstanceBatchData.cpp
  Private/MaterialProviders.cpp Private/MaterialInputValues.cpp
  Private/MaterialGpuCache.cpp Private/MaterialGpuSources.cpp Private/MaterialGpuLifetime.cpp
  Private/RenderMaterial.cpp Private/MaterialResourceService.cpp Private/MaterialResourceProgress.cpp
  Private/RenderResourceClose.cpp Private/SessionMaterialFrame.cpp Private/SessionMaterialBinding.cpp Private/MaterialBindingGroups.cpp Private/SessionMaterialEvaluation.cpp
  Private/SessionViewCache.cpp Private/SharedMaterialEvaluation.cpp Private/MaterialSharedBinding.cpp Private/LocalMaterialPreparation.cpp
  Private/ScenePassCache.cpp Private/ScenePassRefresh.cpp
  Private/InstanceDataCache.cpp Private/RenderItemList.cpp
  Private/RenderPrimitive.cpp Private/PrimitiveMaterialParameters.cpp Private/RenderScene.cpp Private/RenderSceneClient.cpp
  Private/ModelPreparation.cpp Private/ModelResources.cpp Private/ModelMaterials.cpp
  Private/NativeModel.cpp Private/RenderResourceLifetime.cpp
  Private/RenderResourceService.cpp Private/SceneVisibility.cpp Private/SceneDraws.cpp Private/SceneBatchDraws.cpp Private/MaterialDraw.cpp
  Private/Model.cpp Private/RenderSession.cpp Private/SceneBvh.cpp Private/SceneCollection.cpp Private/ScenePreparedCollection.cpp Private/SceneCollectionReuse.cpp Private/SceneRemoval.cpp Private/SceneBridge.cpp Private/SceneMaterialPublication.cpp)
hyp_module(hyperion_render)
target_sources(hyperion_render PRIVATE Private/SceneViewport.cpp)
target_sources(hyperion_render PRIVATE Private/RenderOutput.cpp)
target_sources(hyperion_render PRIVATE Private/RenderCaptureControl.cpp)
target_sources(hyperion_render PRIVATE Private/RenderDiagnostics.cpp)
target_sources(hyperion_render PRIVATE Private/ShadowControls.cpp)
target_sources(hyperion_render PRIVATE Private/SceneLightControls.cpp)
target_sources(hyperion_render PRIVATE Private/SceneEditTarget.cpp)
target_link_libraries(hyperion_render PUBLIC hyperion_scene_editing)
target_sources(hyperion_render PRIVATE Private/ViewportRay.cpp)
target_sources(hyperion_render PRIVATE Private/TransientGeometry.cpp)
target_sources(hyperion_render PRIVATE Private/SelectionOutline.cpp Private/OutlineMaterials.cpp Private/OutlinePasses.cpp Private/ScenePrimitiveSubset.cpp)
target_sources(hyperion_render PRIVATE Private/SceneInstanceAssets.cpp Private/ViewportPlacement.cpp)
target_sources(hyperion_render PRIVATE Private/TransformGizmo.cpp Private/TransformGizmoGeometry.cpp)
target_sources(hyperion_render PRIVATE Private/SceneInstanceBatch.cpp)
target_sources(hyperion_render PRIVATE Private/TransformGroup.cpp)
target_sources(hyperion_render PRIVATE Private/RenderPlugin.cpp)
target_sources(hyperion_render PRIVATE Private/RenderPipelineFrame.cpp Private/PipelineShadows.cpp Private/InstancePacking.cpp)
target_sources(hyperion_render PRIVATE Private/ScenePipelineOutput.cpp)
target_sources(hyperion_render PRIVATE Private/RenderGraphBuffers.cpp)
target_sources(hyperion_render PRIVATE Private/ComputePass.cpp Private/ComputeResources.cpp)
target_sources(hyperion_render PRIVATE Private/HierarchicalDepth.cpp)
target_sources(hyperion_render PRIVATE Private/ContactShadows.cpp Private/ScenePipelineContactShadows.cpp)
target_link_libraries(hyperion_render PUBLIC hyperion_platform hyperion_rhi hyperion_tasks hyperion_plugins hyperion_config hyperion_scene hyperion_math hyperion_materials hyperion_shaders hyperion_reflection PRIVATE hyperion_core hyperion_assets hyperion_io)

target_sources(hyperion_render PRIVATE Private/RenderFeatures.cpp)
