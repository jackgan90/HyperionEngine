## Why

当前 `FScene` 只拥有平铺的模型实例，相机运行时状态由 Viewer 插件持有，方向光和环境光由 Application 写入 session 参数，保存时还需要插件补写相机。这使场景无法统一管理、编辑和持久化相机与光源，也缺少父子变换及与并行渲染帧一致的提交协议。

## What Changes

- **BREAKING**：将 Main-owned `FScene` 扩展为统一的逻辑节点树，管理 Group、Model、Camera、DirectionalLight、EnvironmentLight 节点；提供稳定 ID、generation-safe Handle、父子关系、局部/世界变换、启用继承、类型化编辑和增量变更。
- **BREAKING**：相机姿态、FOV、Near/Far，以及光源颜色、强度、启用/投影状态属于场景；View 请求选择场景相机并提供视口/目标，Renderer 从已提交场景状态生成实际渲染视图。SceneViewer、ModelViewer、光源 GUI、CLI 和 benchmark 均通过场景编辑接口操作。
- 扩展现有 Scene bridge/Render mailbox，以同一发布凭据关联模型、相机、光源及层级更新；Render 消费当前帧对应的不可变状态，禁止借用 Main 场景对象或混用后续帧数据。
- 保留 Forward、Deferred、CSM 的既有光照语义和标准 uniform 布局；由场景节点生成内建输入，显式区分光源属性与渲染质量设置，支持无光源和禁用投影。
- 区分逻辑树和 Render-owned 几何 BVH；父级变换只更新受影响模型，相机/光源变化不重建几何索引，也不整体失效模型、材质和批处理缓存。
- **BREAKING**：原生场景记录升级到 v4，以统一节点记录保存层级、相机、光源及默认选择；兼容旧 JSON source v1 与原生 v1-v3，通过显式迁移生成旧外观所需的相机和灯光。新格式不会自动补回已删除的灯光。
- 提供插件无关的保存/加载、可见场景树和类型化属性编辑，并补齐 CPU、GPU、帧重叠、迁移、交互及性能回归验证。

首轮渲染能力保持为一盏选定的主方向光和一个选定的环境光；场景可保存多个候选节点并显式切换。点光/聚光、多灯累加、正交相机、glTF camera/light 导入、完整 ECS 和模型资产内部节点展开不在本次范围。

## Capabilities

### New Capabilities

- `scene-cameras`: 场景相机数据、姿态与镜头验证、默认/逐 View 选择和 Renderer 视图解析。
- `scene-lights`: 场景方向光/环境光、选择与禁用语义、统一光照/阴影输入和无光源行为。

### Modified Capabilities

- `scene-management`: 统一节点树、类型化操作、层级传播及跨类型原子发布。
- `scene-runtime-instance`: 节点加载、异步依赖隔离、统一快照和 v4 迁移。
- `scene-viewer`: 场景树、相机/光源编辑、统一保存及 ModelViewer 迁移。
- `scene-visibility`: 几何索引与逻辑节点分离，按变化类型维护 BVH。
- `material-parameter-binding`: 场景绑定的权威输入、帧解析和受保护内建语义。
- `bounded-cpu-frame-pipeline`: 场景发布凭据与帧输入一致性及过期/失败处理。
- `cascaded-shadow-maps`: 相机与主光源来自同一场景发布，正确处理无灯和禁止投影。
- `static-model-rendering`: 默认光照由真实场景节点初始化，ModelViewer 交互编辑场景相机。

## Impact

- 主要涉及 `Source/Runtime/Scene`、`Source/Runtime/Renderer`、`Source/Runtime/AssetImport`、`Source/Plugins/SceneViewer`、`Source/Plugins/ModelViewer`、`Source/Applications/Viewer`、相应测试及 `assets/Scenes`。
- `FScene`、`FSceneChange`、`FSceneManifest`、`FSceneInstance`、Scene bridge、场景帧/视图请求接口发生调整；保留底层显式 RenderView/primitive 使用能力、材质通用扩展、模型引用与材质共享/覆盖语义。
- 不新增第三方依赖；Scene 继续只依赖 CPU 模块，RHI 不理解场景节点，保留现有资源/fence 所有权。CMake target、plugin ID、已有 CLI/config key 不更名。
- 本 change 的 planner/reviewer 是当前主 agent；后续由另一个 executor agent 严格执行。当前授权仅生成 artifacts，**不开始实现、不启动 executor、不归档或提交 Git**。实现完成后 executor 提交证据交回 planner/reviewer，不能自行宣布审查通过。详细执行契约见 `design.md`、`tasks.md` 和 `verification-plan.md`。
