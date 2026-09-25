# 文档索引

使用说明与接口契约描述当前实现；历史审计和性能记录只说明对应日期、版本与工作负载的结果。`out/` 中的证据是未纳入版本控制的生成产物，新检出仓库不保证存在。历史记录中的命令需要结合当前配置重新选择场景，不能直接复现旧版本画面与计数。

## 开始使用与开发

- [Visual Studio 与构建](VisualStudio.md)、[验证指南](Verification.md)
- [编辑器工作区与场景视口](Editor.md)
- [Agent 自动化：CLI、MCP、schema 与新功能接入](Automation.md)
- [Automation 能力覆盖、共享服务与任务工作流](AutomationCapabilities.md)
- [运行应用附着：连接、live scene、平台与设备扩展](AutomationConnections.md)
- [静态插件、生命周期与扩展](PluginSystem.md)（应用组合、插件与功能接入的必读契约）
- [代码规范](CodingStyle.md)、[源码组织](SourceLayout.md)、[架构契约](Architecture.md)
- [锁定依赖](Dependencies.md)、[功能范围与扩展边界](Roadmap.md)
- [性能分析工具](Profiling.md)、[RenderDoc 抓帧](RenderDoc.md)

## 资产与场景

- [Content 与虚拟文件系统](ContentFileSystem.md)

- [glTF 资产管线](AssetPipeline.md)、[原生资产](NativeAssets.md)
- [共享材质与纹理资产](SharedMaterialAssets.md)
- [场景管理与相机操作](SceneManagement.md)、[Sponza 示例](SponzaMigration.md)
- [场景组件、反射 Inspector 与编辑契约](SceneComponents.md)

## 渲染契约

- [Render primitives 与生命周期](RenderPrimitives.md)、[CPU 帧管线](CpuFramePipeline.md)
- [Render Graph](RenderGraph.md)、[材质系统](Materials.md)、[实例批处理](InstanceBatching.md)
- [Deferred 与线性 HDR](DeferredRendering.md)、[深度约定](DepthConventions.md)
- [选中物体轮廓与多目标对比](SelectionOutlines.md)
- [Compute pipelines](ComputePipelines.md)、[HZB 与 Contact shadows](ContactShadows.md)
- [级联阴影](CascadedShadows.md)、[局部光](LocalLights.md)、[聚簇光照](ClusteredLighting.md)、[天空与 IBL](SkyLighting.md)

## 优化设计和测量

- [渲染诊断、共享配置和 Editor 测量工具](RenderDiagnostics.md)
- [CPU 提交设计](RendererCpuPerformance.md)、[保留渲染准备](RetainedRenderFrames.md)
- [增量更新](IncrementalRenderUpdates.md)、[合批规划](BatchPlanningData.md)、[相机运动准备](CameraMotionPreparation.md)

功能规范位于 [openspec/specs](../openspec/specs)，变更记录位于 [openspec/changes/archive](../openspec/changes/archive)。归档描述对应历史版本，不是当前运行指令。
