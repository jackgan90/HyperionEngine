# Render primitive 开发契约

通用逻辑场景、Main 桥接和 Render BVH 提前剔除已接入，使用及更新后的数据流见 [SceneManagement.md](SceneManagement.md)。`FModel` 保留为 Renderer 绑定适配器，Scene 中独立的 `FSceneModel` 表示逻辑实例。`CreateBatch` 对应一个剔除组，先执行组和 primitive 级筛选再 Collect；后续 item 过滤及全场景排序继续沿用本契约。

场景渲染统一通过 `FRenderSession`。Main 的逻辑对象向 `FRenderSceneClient` 提交自有描述；Render 独占 `IRenderPrimitive`，收集当帧 `FRenderItem`，再交 RHI 0 物化 GPU draw。模型、三角形及未来其他场景对象遵守相同协议。GUI、清屏和非场景 pass 仍使用 RenderGraph。

## 类型与依赖

| 类型 | 所有权和职责 |
| --- | --- |
| `FModelAsset` | Scene 中不可变、可序列化的 CPU 数据，无 Renderer/RHI 依赖 |
| `FModel` | Renderer 的 Main 绑定适配器；组合资产、实例状态和多个 binding，保留直接客户端兼容性 |
| `FScene` / `FSceneModel` | Scene 模块中的 Main 逻辑场景与实例，通过桥接器同步到 Render，不依赖 Renderer/RHI |
| `FRenderBinding` | Main 的 move-only 凭证；持有身份和独立状态结果，不持有 proxy 指针 |
| `FRenderPrimitiveHandle` | scene identity、slot、generation，不能当成对象地址使用 |
| `FRenderScene` | Renderer 私有的 Render 注册表，唯一拥有完整 proxy |
| `IRenderPrimitive` | Render 上构造、应用状态、收集及析构，可独立扩展继承树 |
| `FRenderResource` | 跨线程只读租约和状态；原生句柄只保存在资源 coordinator |
| `FRenderSceneSnapshot` / `FRenderItem` | 帧自有状态和资源租约，不借用 proxy 或 Main 对象 |
| `FDrawPacket` | RHI 工作描述；原生 payload 由 coordinator 和录制/提交工作共同保活 |

一个 primitive 是可独立注册、更新、移除的持久实例，收集可以产生零个或多个 item。它不等同于 CPU mesh、Model、GPU draw 或硬件 instance。当前模型按选中节点的实例与 section 注册；同一几何的不同节点拥有独立 proxy。`FStaticMeshRenderPrimitive` 同时支持模型与通用程序化几何。

Main 和 Render 类型树不要求对应，也不共享可变对象。新的 proxy 工厂只能捕获值、不可变共享数据或寿命明确的引擎服务，不能捕获 Main 逻辑对象或可变栈引用。首版不允许 Main 构造完整 proxy；工厂在 Render 内返回 `unique_ptr<IRenderPrimitive>`。已构造 proxy 必须恰好在 Render 析构一次，注册失败同样遵守。

## 消息与帧边界

`Create`、`CreateBatch`、`Update`、`Remove`、`Flush`、`Close` 通过统一 mailbox 进入专用 Render executor。admission 锁内的入队时刻定义逻辑顺序；所有控制任务均不带异步成功依赖，因此这个流不会被资源完成顺序重排。`CreateBatch` 和 `Update` 各在一个 Render 任务中处理。不能把带依赖的任意 `Dispatch` 插进该协议代替业务更新。

每次更新是完整 `FRenderPrimitiveState`，带严格递增 revision；资源请求使用不可变 identity/version/configuration。旧 revision、旧 generation、其他 scene 的句柄不会覆盖当前对象。一个更新 batch 先校验所有成员再应用，非法字段会在返回的 task 上报告异常；调用者应观察返回 task，移除任务仍会执行。 已就绪资源的 section 索引在发布前校验；资源仍在准备/上传时无法确定的索引，在描述就绪后通过 binding 状态报告 Failed，无效 item 不进入绘制，也不阻断其他对象。延迟失败不会回滚已经接收的 revision，后续合法更新或 Remove 仍可处理。

`FRenderSession::BuildViews`（单 view 的 `Build` 委托它）的 Render 任务是帧边界：之前入队的控制任务已经完成，后续控制任务在该任务结束后才执行。收集结果按值持有矩阵、可见性、材质覆盖和资源租约。多次或多视图收集不得推进动画/逻辑状态，也不得直接创建 native GPU 资源。Main 等待当前 CPU 帧只是 Viewer 的调度选择；数据隔离依靠自有快照。

创建、更新和移除无需出帧即可推进。最小化、没有 Present 时仍由专用 Render 队列处理。`Remove()` 立即使 binding 停止接受更新，返回的回执表示 Render 已移除/析构对象；它不表示 GPU 完成。重复 Remove 返回同一回执。关闭后存活的 binding 只释放已经失效的 mailbox/result，不访问 executor。

状态查询通过独立、带锁的结果数据完成：`PendingCreate`、`PendingResources`、`Ready`、`Failed`、`Removed`。设计中的 Retiring 是移除已接收但回执未完成的阶段，Destroyed 对应 `Removed` 回执；不需要把这两个瞬态作为可轮询的额外状态。失败、取消后的异步结果不会重建 proxy；Main 消费者消失也不会阻止结果收尾。

## 共享、就绪与实例覆盖

每个设备/session 创建一个 `FRenderResourceService` 并供所有生产者复用。服务以仍被强引用保活的不可变源 identity、version、configuration 定位资源，记录另外分配唯一 identity。section 的 geometry/material 索引及范围描述子资源。地址只在源对象仍受保活时参与键，不会把复用地址当成旧资产。不同服务/device 不共享 native payload。

模型 preparation 固定顶点布局，按图片索引与 sRGB/线性角色分别生成纹理，并保留材质 sampler、shader、depth/blend/cull 及镜像绕序变体。通用 `Request` 的调用者必须把所有影响布局、shader 或颜色解释的配置纳入 configuration，或提高 version。不能以相同键请求不同内容。geometry 与 material 分别共享和退休；材质资源以不可变 source/view 身份复用。不做独立导入副本的内容去重、LRU 或热重载。

Worker 准备不可变 CPU 顶点、mip、shader 数据；RHI 0 创建 GPU 资源并发起上传；Render 读取已发布的就绪描述。组内所有上传完成前不会输出该组 draw。资源替换提交完整状态和显式新版本；pending 版本不会被物化，旧结果也无法覆盖新请求。首版替换期间可暂时不绘制该组，不提供旧版本持续显示或热重载事务 UI。

兼容请求共享一次 preparation/upload；释放一个消费者不取消其他消费者。失败记录为原消费者保留错误，新请求可重试，重试中的请求继续合并。变换、可见性及当前支持的 base color、metallic、roughness 覆盖只进入实例状态和常量数据，不能修改共享定义。改变 alpha mode/shader 需要新的不可变 material definition，改变纹理/sampler 发布新 material snapshot；两者均不要求重新上传 geometry。

## Render 析构与 GPU 退休

coordinator 权威持有全部受管 native 句柄。租约最后释放只通知 coordinator；proxy、Main 临时值或 frame snapshot 不会成为 native 对象的最后所有者。RHI 0 检查 CPU 租约、上传状态以及 native packet/list 的引用，在工作完成后清除权威句柄。

D3D12 上传批次由上传 fence 保活；成功提交的录制列表进入设备级 fenced submission 集合。`CollectCompletedResources()` 在 RHI 0 非阻塞采集完成记录，因此无需等待下次 frame-ring 复用。 直接 RHI 客户端的 `BeginFrame` 也在既有 fence 等待之后采集完成记录，保留正常运行期自动回收行为。活跃资源无需持续轮询；pending preparation/upload、最后使用者离开或帧常量等待回收时，现有 Worker 调度短周期进度任务，再交 RHI 0 查询。2 ms 只决定查询频率，绝不作为安全退休依据。

普通 Remove 不调用 `WaitIdle()`。上传中移除、从未绘制的资源和最后一帧之后的资源都走同一回收入口。上传失败不阻断移除；设备无法完成 drain 时必须保留所有权并报告错误，不能伪造安全完成。

关闭顺序：停止 Main 生产者及插件；关闭 scene admission 并等待所有 Render 控制/帧工作，销毁 proxy；关闭资源 admission，join 所有 preparation 和进度任务；在 RHI 0 drain GPU、回收 native 句柄；然后销毁 swapchain/device，最后关闭 Tasks。调用 `Close` 前必须释放调用者保留的 raw draw packets；否则关闭会报告错误，不能强制清除仍被借用的资源。CPU 资源租约可在关闭后存活并查询 `Retired`。

只允许 Main 等待 Render/RHI、Render 等待 RHI。RHI 不等待 Render 或 Main。Tasks 的同专用队列等待检查保留；跨队列循环是禁止的调用模式，不新增通用死锁检测器。

## 场景绘制与扩展

Render 先冻结资源组就绪状态，过滤隐藏和未就绪 item，以局部 AABB 的八角做保守视锥测试；缺少有效 bounds 或无法得出有限结果时保留。镜像及非均匀变换直接体现在 clip 变换中。opaque/mask 在前，blend 按全场景中心投影深度稳定排序，不做每模型局部排序。中心排序不能解决相交透明面。

兼容的 item 聚合到场景 pass，每个 view 的输出由独立的 `FRenderPassTargets` 声明，depth/stencil 的初始化来自显式 load action。depth 开关由各 draw 的 pipeline 控制；pass 只因 view/viewport、linear/sRGB 目标或显式用途边界分开，不能按 Model/primitive 数量创建。RHI 0 按真实反射布局打包并共享各 scope 的不可变常量 slice，沿用 RenderGraph 验证、并行录制、提交和错误取消。队列排序仅移动索引，保持 opaque/mask 等深次序与全场景透明顺序。

场景插件实现 `IScenePlugin`：`Start/Update/Stop` 在 Main 执行，注册资源和 binding；`Update` 可生成 owned view/settings snapshot。ModelViewer 管理加载、相机与 `FModel`；Triangle 使用通用 geometry/material 描述和自己的 shader。Renderer 不识别它们的类型或 ID。非场景插件实现 `IRenderPlugin::Build`，在 Render 添加 GUI 等 pass；其 Main 生命周期自行 dispatch RHI 资源工作。

未来 GPU instancing 需要在此基础上实现兼容性键、可见实例压缩、instance buffer 生命周期及 shader/RHI 支持。共享 VB/IB 目前仍提交普通 indexed draws，不宣称已经合批。骨骼、动画、LOD、粒子和通用离屏图需要独立扩展。

## 可执行验收

`render_primitives` 覆盖线程、消息隔离、revision/generation、原子更新、异构 proxy、移除与关闭。`render_resources` 用受控替身覆盖共享上传、失败重试、迟到结果、pending 移除、native 析构域、视锥与聚合。`scene_rendering` 不链接实验插件，以真实 D3D12 像素检查多模型共享/实例隔离、全场景深度、交错透明和 40 个 primitive。`d3d12_frame_failure_recovery` 的真实 fence gate 验证 proxy 先析构，native buffer 仍保活，放开 gate 后没有新帧也会退休。

完整运行入口和环境要求见 [VisualStudio.md](VisualStudio.md)。本次变更的实际验证证据见 [verification.md](../openspec/changes/archive/2026-09-07-add-render-primitives/verification.md)。

## 通用材质入口

完整类型、按 name/semantic 绑定和 GPU 共享契约见 [Materials.md](Materials.md)。`FRenderPrimitiveState.Surface` 是独立的 material lease，`ObjectParameters/SectionParameters` 是 owned typed overrides。`FRenderItem.DrawParameters` 只影响当前 draw；`ObjectInputs/DrawInputs` 单独向 semantic provider 提供输入。

`LocalItemId` 为自定义多 item primitive 提供稳定身份；省略时 ordinal 仅在当前 collection 有效，不能跨 collection 共享 Object/Draw 参数。cache key 同时包含实际 World/参数内容，不能仅以 primitive revision 代替。clip-space 在 primitive 上声明；shader 位移材质未提供保守 bounds 时，组、pre-collection 和 item 层均保持保守。

`FRenderBinding.GetStatus()` 表示静态 readiness，`GetLastDrawResult()` 单独报告带 frame/view/material revision 的上下文准备结果。同一 group 的 packets 全部准备成功才输出，其他 group 继续。Model/Scene bridge 提供 `GetDrawResults()` 给上层 UI。静态 Ready 可在零帧条件达成，缺少动态 provider 的错误可在下一有效 frame 中恢复。

## 显式 pass 与 view

`FRenderView` 只保留相机、剔除、材质 usage/参数及视图标识。`FRenderPassTargets` 单独描述颜色、深度/模板附件和 sampled reads；`Session.FrameTargets(clear)` 生成当前 session 格式的显式帧附件。`Build`/`BuildViews` 同时接收 view 和 targets。直接构造 `FRenderSceneSnapshot` 的调用方也必须填写 `Targets`。

Render 阶段先声明完整的图资源与 graphics pass，再冻结 draw preparation 回调。回调仅返回 `FGraphicsDrawBatch`。材质 pipeline/batch signature 由附件的 ColorCount/DepthFormat 推导；linear/sRGB 分段保持原有顺序及共享 draw 数据。图编译在 RHI 0 解析资源并准备 draw，正常 Viewer 保留单次帧协调边界。规则见 [RenderGraph](RenderGraph.md)。
