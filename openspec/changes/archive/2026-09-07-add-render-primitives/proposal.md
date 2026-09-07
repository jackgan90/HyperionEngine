## Why

当前 glTF 路径把共享资源、模型实例、材质处理和 draw 生成集中在 `FModelRenderer`，并由应用插件分别组织模型 pass。引擎需要统一的 Render 线程代理协议，使逻辑对象、渲染实例、共享资源和实际 draw 分离，为多模型正确渲染以及后续 instancing、骨骼和其他对象类型建立稳定边界。

## What Changes

- 在 Runtime/Renderer 中建立 `IRenderPrimitive`、Render 线程拥有的 `FRenderScene`，以及 Main 侧不透明绑定和创建、更新、移除命令；资产数据继续独立于 Renderer/RHI。
- 明确 primitive 是持久渲染单元，与 GPU draw 数量不固定对应；通过本帧绘制描述统一组织可见性过滤、材质分组、跨模型透明排序和 pass。
- 引入通用 Main 侧模型实例包装，首版将每个选中场景中的节点实例与 glTF primitive/section 对应关系转为渲染代理；独立实例共享几何及兼容材质资源。
- 定义帧边界快照、批量更新、generation/revision、防止迟到结果复活对象，以及不依赖成功前置任务的清理协议。
- 分离 Render 侧对象析构与 RHI/GPU 资源回收，覆盖未绘制资源、上传中移除、失败、取消、在途绘制和关闭；正常对象移除不使用全局 GPU idle。
- **BREAKING**：替换仓库内部 `FModelRenderer` 整模型绘制接口，并调整场景对象插件的接入方式；ModelViewer 与 Triangle 都通过通用 primitive 流程接入。GUI 和非场景工作继续使用标准 pass 扩展。
- 保留现有 shader/material 语义、CLI、序列化字段、插件 ID、CMake target 和可执行文件名。首版保留 Main 等待 Render 的调度，不实现自动 instancing、骨骼动画或通用离屏资源图。
- 将线程、依赖、数据交接和生命周期契约写入项目架构文档，并提供独立于 viewer 的验收覆盖。

## Capabilities

### New Capabilities

- `render-primitives`: 持久代理与逻辑对象分离，RenderScene 注册表、类型扩展、Main 绑定、命令顺序、帧一致性、收集及完整移除协议。
- `shared-render-resources`: 同一设备上的几何/材质资源共享、实例覆盖隔离、上传就绪发布、资源版本与租约，以及可供未来合批使用的绘制描述。

### Modified Capabilities

- `render-graph-plugins`: 场景对象统一通过 primitive 接入，Renderer 按视图/pass 聚合绘制；保留 GUI 和非场景 pass 扩展、图验证与失败取消。
- `static-model-rendering`: 多个 Main 模型实例通过通用代理显示，共享资产资源，跨模型保持深度和透明排序，并保留现有 glTF 与相机验收。
- `rhi-backend-abstraction`: 补充 Renderer 管理资源的 coordinator 回收契约，区分代理析构、CPU 使用结束和 GPU 完成，保留公共 RHI 的设备身份及现有独立资源生命周期语义。
- `runtime-module-organization`: 固化 Scene/Animation 不依赖 Renderer/RHI，以及 Main 渲染绑定、共享资源与通用代理实现的模块归属。

## Impact

- 主要涉及 `Source/Runtime/Renderer`、`Source/Runtime/RHI`、D3D12 私有实现中的必要生命周期接缝、ModelViewer/Triangle、Viewer 组装与对应测试；Tasks 仅在需要验证执行域时增加最小引擎接口，不改变成功依赖语义或调度后端。
- `Scene`、`AssetImport` 和现有反射资产格式保持 CPU 数据边界；不增加第三方依赖，不引入新的原生图形后端。
- 实现时新增 `docs/RenderPrimitives.md`，同步 `docs/Architecture.md`、`docs/SourceLayout.md` 和 `docs/AssetPipeline.md`，补充可验证的线程与资源生命周期证据。
- 本次交付仅为 proposal、design、delta specs 和未勾选的 tasks。生成及校验完成后停止，等待用户确认 plan；不开始实现、修改正式规范或归档。
