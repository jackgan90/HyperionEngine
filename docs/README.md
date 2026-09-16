# 文档索引

使用说明与接口契约描述当前实现；历史审计和性能记录只说明对应日期、版本与工作负载的结果。`out/` 中的证据是未纳入版本控制的生成产物，新检出仓库不保证存在。历史记录中的命令需要结合当前配置重新选择场景，不能直接复现旧版本画面与计数。

## 开始使用与开发

- [Visual Studio 与构建](VisualStudio.md)、[验证指南](Verification.md)
- [编辑器工作区与场景视口](Editor.md)
- [代码规范](CodingStyle.md)、[源码组织](SourceLayout.md)、[架构契约](Architecture.md)
- [锁定依赖](Dependencies.md)、[功能范围与扩展边界](Roadmap.md)
- [性能分析工具](Profiling.md)、[RenderDoc 抓帧](RenderDoc.md)

## 资产与场景

- [Content 与虚拟文件系统](ContentFileSystem.md)

- [glTF 资产管线](AssetPipeline.md)、[原生资产](NativeAssets.md)
- [共享材质与纹理资产](SharedMaterialAssets.md)
- [场景管理与相机操作](SceneManagement.md)、[Sponza 示例](SponzaMigration.md)

## 渲染契约

- [Render primitives 与生命周期](RenderPrimitives.md)、[CPU 帧管线](CpuFramePipeline.md)
- [Render Graph](RenderGraph.md)、[材质系统](Materials.md)、[实例批处理](InstanceBatching.md)
- [Deferred 与线性 HDR](DeferredRendering.md)、[深度约定](DepthConventions.md)
- [Compute pipelines](ComputePipelines.md)、[HZB 与 Contact shadows](ContactShadows.md)
- [级联阴影](CascadedShadows.md)、[局部光](LocalLights.md)、[聚簇光照](ClusteredLighting.md)、[天空与 IBL](SkyLighting.md)

## 优化设计与历史测量

这些文档保留基线、测量方法、回退数据和当时的设计取舍。功能边界以以上契约文档为准；表中的旧配置、模型数、测试数和耗时不是当前默认值或性能保证。

- [SceneViewer 性能定位](ScenePerformance.md)
- [材质性能](MaterialPerformance.md)、[实例批处理性能](InstanceBatchPerformance.md)
- [Renderer CPU 提交](RendererCpuPerformance.md)、[保留渲染帧](RetainedRenderFrames.md)
- [增量渲染更新](IncrementalRenderUpdates.md)、[合批规划数据](BatchPlanningData.md)
- [运动相机准备](CameraMotionPreparation.md)、[Deferred 性能](DeferredPerformance.md)

## 历史审计

- [ModelViewer](ModelViewerReview.md)、[RenderDoc](RenderDocReview.md)
- [材质](MaterialAudit.md)、[实例批处理](InstanceBatchAudit.md)、[级联阴影](CascadedShadowAudit.md)
- [Renderer CPU](RendererCpuAudit.md)、[合批规划](BatchPlanningAudit.md)、[Deferred](DeferredAudit.md)

功能规范位于 [openspec/specs](../openspec/specs)，历史提案、任务与交付证据位于 [openspec/changes/archive](../openspec/changes/archive)。
