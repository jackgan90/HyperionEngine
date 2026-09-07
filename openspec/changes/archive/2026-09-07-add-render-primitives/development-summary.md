# Render primitive 开发总结

本 change 将场景提交从整模型渲染器迁移到通用 Runtime render primitive 流程。ModelViewer、Triangle 和其他场景生产者通过同一 Renderer 接口注册，Render 统一收集和提交。开发、37 项任务、验收和两项审计修复均已完成。

## 交付内容

- **线程与类型边界**：Main 的 `FModel` 组合不可变 Scene 资产与多个 move-only binding；Render 独占 primitive 的构造、注册、修改、收集和析构。Main/Render 类型树独立，渲染不依赖 Main 可变逻辑对象。
- **同步协议**：统一 mailbox 的 FIFO 入队顺序、自有状态快照、scene/slot/generation 句柄、递增 revision、完整 batch 校验、移除回执和关闭屏障；没有新帧也能推进控制任务。
- **共享资源**：设备/session 内按不可变 identity/version/configuration 合并准备和上传，多个模型共享几何与材质资源，变换/可见性/材质覆盖保持实例独立；失败可以重试，迟到结果不会复活已删除对象。
- **资源生命周期**：Worker 准备、RHI 0 创建与上传、Render 消费已发布描述。coordinator 持有权威 native 引用，结合上传和提交 fence 在 RHI 0 最后释放，支持无后续帧退休；保留直接 RHI 的正常帧回收、Present 错误恢复与 device ownership。
- **统一场景绘制**：保守 AABB 视锥过滤、全场景透明中心深度排序、聚合 pass 与一次深度初始化、按帧批量常量上传，保持普通 indexed draw。
- **调用方与模块**：迁移 Viewer、ModelViewer、Triangle、DebugUI，移除旧 `FModelRenderer` 提交路径；保留 GUI overlay、相机、glTF 材质、RenderDoc、CLI、序列化字段、插件 ID 和构建目标。
- **开发契约**：同步 Architecture、SourceLayout、AssetPipeline，新增 [RenderPrimitives.md](../../../../docs/RenderPrimitives.md) 作为线程、状态、帧、退休和扩展的项目标准。

## 验证和审计

实现阶段 VS Debug/Release 完整 CTest 各 **35/35**，包含实际 GPU、desktop 与 RenderDoc 捕获/回放。独立审计确认并修复：非法 section 的批量发布/延迟诊断，以及直接 RHI 已完成提交的持续累积；原 reviewer 复审均关闭。

修复后的 Ninja Debug、VS Debug、VS Release 全部目标构建成功，相关 CTest 各 **8/8**；74 编译单元命名、127 文件格式、模块边界及 OpenSpec strict 检查通过。本次归档没有再修改已审核源码，通过哈希检查沿用这些验证结果。详细环境和限制见 [验证记录](verification.md) 与 [独立审计](independent-review.md)。

## 范围边界

共享几何仍提交普通 draws，未实现 GPU instancing 或 instance buffer 分配器。动画/蒙皮、LOD、通用离屏图、新 backend 和 Main/Render 帧重叠留待后续变更。透明中心排序保留相交透明面的限制；替换资源 pending 期间可暂时不绘制，完整新版本就绪后提交。

正式规格同步涉及 `render-primitives`、`shared-render-resources` 两项新增能力，以及 `render-graph-plugins`、`static-model-rendering`、`rhi-backend-abstraction`、`runtime-module-organization` 四项已有能力。审计发现的已知 section 原子拒绝、pending section 无帧错误发布及直接 RHI 正常回收均有对应验收场景。
