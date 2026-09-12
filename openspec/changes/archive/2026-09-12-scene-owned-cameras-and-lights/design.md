## Context

本设计是给后续 executor 的实施契约，不是已实现能力。规划基线为 `ee585cc88fd60a22a29dc2103cd4dafe6d234221`。本轮只产生此 change 下的 Markdown/YAML artifacts；代码、正式 specs、资产、构建配置均未改动。

### 工作分工与阅读顺序

- 当前主 agent 负责 planner/reviewer；executor 负责按本设计实现、测试、修复自身实现问题并提交证据，不得自行更换架构或宣称 reviewer 已通过。
- 阅读顺序：`proposal.md` → 本文 D1-D12 → `specs/*/spec.md` → `verification-plan.md` → `tasks.md`。
- `tasks.md` 的任务必须按依赖执行，所有复选框初始未完成。executor 发现基线变化、实现契约互相冲突或需要扩大范围时，将具体源码证据和建议交回 planner；不得靠删测试、吞错误、保留另一套权威相机/灯光来完成任务。
- 局部实现选择（容器、私有函数拆分等）可自行解决，但必须维持本文的所有权、提交版本、格式和行为契约。建议类型/文件名允许因实际重名作等价调整，需在交付说明建立映射。
- 当前不启动 executor；后续得到启动指令后才开始。executor 完成后提交 review handoff，等待 planner/reviewer 审查。归档与 Git commit 不属于此 proposal 的自动执行步骤。

### 已核对的源码基线

| 位置 | 当前行为 | 本次改造落点 |
| --- | --- | --- |
| `Source/Runtime/Scene/Public/Hyperion/Scene/Scene.h`、`Private/Scene.cpp` | `FScene` 的 Slot/Change 仅包含 `FSceneModel`，单 Main owner，单同步消费者 | 通用节点存储、关系、类型化编辑、world/enabled 派生和变更分类 |
| `Scene/Public/Hyperion/Scene/Model.h` | `FModelAsset::Nodes/Roots` 为模型内部层级 | 保留，不展开成运行时节点 |
| `Scene/Public/Hyperion/Scene/SceneManifest.h`、`Private/SceneManifest.cpp` | `hyperion.scene` v3：Assets、Instances、Eye、Target、Near、Far | v4 节点记录及 v1-v3 迁移 |
| `AssetImport/Private/Adapters/SceneJson.cpp` | plain JSON source v1 与 reflected source；importer version 1 | source v2 解码、v1 转换、importer revision 更新 |
| `AssetImport/Private/AssetPublication.cpp`、`Applications/AssetTool/Private/AssetCommands.cpp` | `--scene` 用 model 包装旧 manifest；inspect 输出 instances 数量 | v4 包装及显式默认节点；保留 instances= 的 model-only 含义并新增 node counts |
| `Renderer/Private/SceneInstance*.cpp` | FSceneInstance 拥有 FScene、Bridge、manifest、模型/材质请求及模型 ID 映射 | 先安装全部节点，再启动模型依赖；稳定 ID 归节点；从当前场景保存 |
| `Renderer/Private/SceneBridge.cpp`、`SceneMaterialPublication.cpp` | Main 准备模型批次，一次 Render 任务发布；Flush 后确认 admission | 扩展跨类型 publication 和整场景 receipt |
| `Renderer/Private/SceneRemoval.cpp` | 单 logical scene attachment；detach 当前只清 Main mailbox ID | 新增 attachment epoch，Render 元数据清空及失败路径 |
| `Renderer/Private/RenderSceneInternal.h`、`SceneCollection.cpp` | Render 的 primitive/group/BVH、几何 collection revision | 增加场景元数据旁路，不能混入几何 revision |
| `Renderer/Private/SessionMaterialFrame.cpp` | Main FreezeFrame 复制 session 输入；Scene scope 用 logical scene identity qualifier | 场景帧种子与 Render 最终解析；自定义输入和场景光照分离 |
| `Renderer/Private/RenderSession.cpp` | 构造器默认设置方向光/环境光 | 默认灯只能保留在未绑定场景的低层兼容路径 |
| `Renderer/Private/SceneRenderPipeline.cpp`、`ForwardRenderPipeline.cpp`、`ScenePipelineFullscreen.cpp` | 光照、CSM、Deferred 通过共同语义取值 | 新场景解析入口、投影开关和无灯行为 |
| `Renderer/Private/SessionViewCache.cpp`、`SessionMaterialBinding.cpp` | 分离 collection/pass/view 条件和共享值缓存 | 保留局部缓存，绑定当前相机及光照依赖 |
| `Plugins/SceneViewer/Private/SceneViewer*.cpp` | 自持轨道相机，固定 FOV `.729224966f`；SaveAsync 补写 Eye/Target | 输入和保存均操作场景节点，增加层级/类型化 GUI |
| `Plugins/ModelViewer/Private/ModelViewerPlugin.cpp` | 已持有 FScene/Bridge，但相机仍在插件里 | 初始化真实相机/默认灯，迁移相机交互 |
| `Applications/Viewer/Private/ViewerShadows.cpp`、`ViewerFrame*.cpp` | 光源 GUI/benchmark 写 Application ShadowLight，每帧 SetSceneParameters；帧输入已值捕获 | 应用场景编辑、捕获发布凭据、Render 解析 |

路径表中省略的前缀均为 `Source/Runtime/`，Plugins/Applications 路径前缀为 `Source/`。实施前用 `rg --files` 校对实际文件，不根据历史文档猜测。

## Goals / Non-Goals

**Goals:**

1. 模型、相机、方向光、环境光是同一 FScene 管理的节点，支持增删、父子组织、运行时修改和持久化。
2. 每帧先完成 Main 场景更新，再发布；Renderer 从该帧指定的场景发布解析相机、灯光和几何，不借用 Main 指针。
3. SceneViewer 和 ModelViewer 不再保存另一份持续覆盖场景的相机/灯光状态。
4. 保持既有材质/资源/实例批处理/阴影/深度和异步加载行为，变化只失效实际相关缓存。
5. executor 获得明确的接口、错误、迁移、验证及交付边界。

**Non-Goals:**

- 不引入完整 ECS、Actor/UObject 系统、动画模块、任意脚本组件、undo/redo 系统或多 session 共享一个可变场景。
- 不实现点光、聚光、多方向光累加、IBL/天空盒或光源网格聚类。多个候选灯可保存/编辑，内建渲染使用显式选中的一个方向光和一个环境光。
- 首轮只支持现有透视相机；正交相机、物理镜头、立体渲染不在范围。多相机数据和独立 View 选择必须可用。
- 不展开模型资产内部节点，不导入 glTF camera/light，不改变模型资产的既有 DAG/occurrence 语义。
- 不改变 RHI 场景无关契约，不引入新 GPU fence/idle wait，不降低 Debug validation、CSM 质量或更新频率。
- 保留不绑定 FScene 的显式 `FRenderView`/primitive/material 测试与扩展入口；生产 SceneViewer/ModelViewer 必须使用场景路径。

## Decisions

### D1. 统一节点，payload 与通用属性分离

采用轻量 tagged payload，而不是五个互不关联的实体表或继承对象树。所有节点使用现有 `FSceneHandle { Scene, Slot, Generation }`，通用槽位拥有节点值；同一 live slot 只能有一个 payload 种类。

建议 CPU 数据契约：

| 类型 | 权威字段 | 约束 |
| --- | --- | --- |
| `FSceneNode` | `Id`、`Name`、`Parent`、`Local`、`bEnabled`、payload | Id 非空且在同一场景唯一；Parent 为空表示根；Local 为有限 affine matrix |
| `FSceneModelComponent` | `Asset`（manifest asset ID，可空）、`Data`、`bVisible`、现有 Material/Surface/SectionSurfaces | Data 可为空占位；材质共享和覆盖顺序不变 |
| `FSceneCamera` | `VerticalRadians`、`Near`、`Far`、`FocusDistance` | 位置/朝向只从节点变换来；FocusDistance 仅为可持久化的导航焦点距离，不参与投影 |
| `FSceneDirectionalLight` | 线性 RGB `Color`、`Intensity`、`bCastShadows` | emission forward 为局部 -Z；颜色和强度均非负且有限 |
| `FSceneEnvironmentLight` | 线性 RGB `Color`、`Intensity` | 不随位置/朝向改变；仍参与节点启用继承 |
| `FSceneSettings` | `DefaultCamera`、`MainDirectionalLight`、`EnvironmentLight` 的 optional Handle | 类型和 scene/generation 必须正确；允许未选择 |

`ESceneNodeKind` 为 Group/Model/Camera/DirectionalLight/EnvironmentLight。可用 variant 实现运行时 payload，或用经验证互斥的 optional 字段；持久化采用 D8 指定的 optional records，不为本次工作改造通用反射 variant 支持。Add允许空Id表示请求生成，插入前生成未用的`node-<monotonic serial>`；生成器避开加载过的所有live IDs，复制节点清空输入Id以取得新Id。调用者提供的非空Id若冲突直接拒绝；live节点Id不提供重命名setter。

`World`、`bEffectiveEnabled`、children/roots 索引及版本是派生缓存，不允许调用者直接修改，也不序列化。children 按创建顺序/加载时 nodes 顺序保存，GetNodes/遍历保持确定性；不要用 BVH 内部次序作为 UI 或持久化顺序。

新light payload的CPU字段默认Color为white、Intensity为1、bCastShadows为true；这不创建节点，兼容默认内容helper仍显式设置D4的旧radiance。source v2允许字段/最小例子见`source-format-example.md`。

`FSceneModel` 可保留为已有 Renderer `FModel` 接口使用的 transfer value。Bridge 从 model component + node 派生 World/Name/visibility 构造它；不得在节点旁另存可独立写入的 `FSceneModel::World`。Scene 的旧模型专用 Find/Update 调用点迁移到类型化节点接口，不能通过两个可写副本实现兼容。

稳定 ID 归 FScene 节点，不再仅由 `FSceneInstanceModel` 持有。保留 `GetModels()` 兼容诊断视图时，Handle/Id/Asset 必须由当前节点投影得到或严格随节点事务更新，不能成为身份/变换权威。AssetImport/Assets 的路径解析和模型请求仍由 FSceneInstance 协调；FScene 不依赖 IO、Assets、Renderer 或 RHI。

### D2. Main 编辑、层级和变更协议

建议公开能力（最终签名可按仓库返回类型习惯细化，语义不得弱化）：

| API | 必须实现的行为 |
| --- | --- |
| `AddNode(FSceneNode)` | 生成或验证稳定 ID；校验 parent/kind/payload；返回 generation-safe Handle |
| `FindNode(Handle)` / typed const getters | 只读 Main 借用；无效/foreign/stale 返回空；无可写引用 |
| `GetNodes()`、`GetRoots()`、`GetChildren(Handle)`、`GetNodes(Kind)` | 包含相机、光源和空节点；确定次序 |
| `SetLocalTransform(Handle, Matrix)` | 校验并更新受影响子树，标记变更 |
| `SetWorldTransform(Handle, Matrix)` | 用 `Inverse(ParentWorld) * DesiredWorld` 得局部矩阵；根直接使用 DesiredWorld；不可逆父级显式拒绝 |
| `SetEnabled`、`SetName`、`SetModelVisible`、`SetCamera`、`SetDirectionalLight`、`SetEnvironmentLight`、模型内容编辑 | 相应类型校验，保留不涉及的属性；不接受错误 payload kind |
| `Reparent(Handle, optional Parent, ESceneReparentMode)` | 显式 KeepLocal 或 KeepWorld，无隐式默认模式；禁止自父、环和跨场景引用 |
| `RemoveSubtree(Handle)` | 明确删除整棵子树，清除指向已删节点的 settings 引用并产生所有 tombstone |
| `RemoveNodeKeepChildren(Handle)` | 将直接子节点移到被删节点的原父级，保留它们的世界矩阵；先验证全部新局部矩阵，失败则不删除任何节点 |
| `SetSettings(FSceneSettings)` | 原子校验三种选择；无效目标不偷偷选另一节点 |
| `Update()`、`GetChanges()`、`Acknowledge()` | Update 归并有效变化、完成派生状态；Bridge 读取自上次 admission 后最新增量；ack 只确认已接纳 revision |

节点种类在其生命周期内固定；转换种类使用删除/新增，不能保留旧 generation 使异步模型结果挂到 camera/light。无效 handle 返回失败；非法数值/层级/类型抛可诊断错误。失败不得部分修改节点、关系、settings、revision 或待发布 changes。

世界矩阵规则为 `World = ParentWorld * Local`；`EffectiveEnabled = ParentEffectiveEnabled && bEnabled`。模型绘制启用为 `EffectiveEnabled && Model.bVisible`。`bVisible=false` 只隐藏本节点模型几何，不隐式关闭子相机/光源；禁用整个子树使用 `bEnabled=false`。

实现推荐：用稳定 slot、父引用和 children 索引；对一次编辑先收集受影响节点、预计算候选世界矩阵/有效状态、验证、预分配提交所需条目，再提交。传播使用迭代遍历，禁止依赖输入节点的父先子后顺序或用无界 C++ 递归。Update/World getter/Snapshot 共用派生状态实现；未变子树不重复计算。

修改 parent/local/enabled 会影响后代；修改 name 或 FocusDistance 不影响渲染。相同值重复写入是 no-op（不增加相应版本）。新增后又删除、删除后复用 slot 的合并以完整 Handle 为键，不能仅按 slot 丢失 tombstone。`BeginSynchronization` 仍只允许一个消费者并为重挂接发布现存完整状态。

引入 change mask，至少区别 Structure/Transform/Enabled/Model/Camera/Light/Metadata/Settings。logical Revision 反映任何真实持久化改变；它不能直接充当所有 Render 缓存的版本。D7 指定消费分类。

### D3. 相机姿态、镜头、选择和控制器

约定：列向量、右手世界，相机局部 -Z 为 Forward，+Y 为 Up；节点 Local/World 保留完整 affine matrix。Renderer 采用 CPU Scene 工具提取的 pose，不让 Scene 包含 RHI 深度或 viewport 类型。

姿态提取统一在 Scene 私有实现/CPU 公共工具中完成：

1. Eye = World 对 `(0,0,0,1)` 的变换。
2. Forward = normalize(World 对 `(0,0,-1,0)` 的方向变换)。
3. UpCandidate = World 对 `(0,1,0,0)` 的方向变换。
4. Right = normalize(cross(Forward, UpCandidate))；Up = normalize(cross(Right, Forward))。
5. Eye、方向长度、叉积长度均须有限且大于明确的 epsilon（建议 `1e-6f`）；用独立数值 tests 覆盖临界值。镜像/非均匀缩放保留变换后的 forward/up 趋势，重建正交基，不能把剪切/缩放直接带进 view matrix。导致 basis 退化的祖先变换在编辑事务中拒绝；不可逆但仍有有效 basis 的矩阵不因 model 限制而一概拒绝，只有需要反求 local 的操作必须检查逆矩阵。

投影首轮固定透视；`0.01f < VerticalRadians < 3.0f` 与当前 CSM 支持范围一致；Near > 0、Far > Near、FocusDistance > 0，全部有限。viewport 宽高为 0 时按现有 skipped-frame 路径处理，不能构造除零 aspect。`Aspect = effective viewport width / height`，没有 viewport 则用 target width/height；Near/Far/FOV 取相机。`ViewProjection = Perspective(...) * LookAt(Eye, Eye + Forward, Up)`；深度 convention 来自 View。

默认 CPU `FSceneCamera` 使用 SceneViewer 兼容镜头 `.729224966f`、Near `.01f`、Far `1000.f`、FocusDistance `12.f`。ModelViewer 的初始化明确使用自身现有 FOV `1.f` 和原有 fit/near/far 公式；不要因为共用类型改变现有 ModelViewer 构图。

新增 `FSceneViewRequest`（Renderer 公共类型），拥有目标尺寸、viewport、depth convention、稳定 View identity、culling/batching/debug 配置及 optional Camera Handle。它不包含可写 Eye/Forward/FOV/VP。空 Handle 表示使用 FSceneSettings.DefaultCamera。显式指定 stale/disabled 相机可退回有效默认相机，返回结构化 fallback 原因；若默认也无效，返回 `NoActiveCamera`，不能取“第一个相机”或上帧缓存。foreign-scene Handle 属于调用错误，直接拒绝。

无相机不等于 scene/model loading failure：Viewer 保持输入/GUI和 clear 输出，跳过场景/阴影 draw，并显示状态。CPU frame tick 仍推进。相机恢复后正常渲染；benchmark/capture 的“可测场景”条件必须另检查有 active camera 和非空 ready 几何。

多 View 使用独立 request/identity；相机切换增加相应 view dependency 或改变 qualifier（包含相机完整 Handle 和有效 lens/pose 版本），不更改模型。自动 CSM 派生视图无需变成持久化相机节点。

控制器为输入解释器：持有被控 Camera Handle、鼠标状态、速度等，不每帧发布自身旧姿态。每次操作先读取当前世界 pose 和 FocusDistance；pivot=`Eye+Forward*FocusDistance`。轨道旋转对当前 forward/up/offset 应用 world-Y yaw 和相机 right pitch，保留有限正交姿态及现有极角限制；dolly 调整距离/位置，平移同时移动 Eye/pivot。Fit 仅统计有效可见的 model 世界 bounds，使用当前 lens 与 viewport aspect，修改 camera transform/FocusDistance；不修改灯光/模型。有父级时经 SetWorldTransform 写回，失败显示原因。外部编辑/reparent/相机切换后读新状态，绝不被缓存的 Yaw/Pitch/Target 覆盖。

### D4. 光源数据、选择和默认策略

首轮可存在任意多个 DirectionalLight/EnvironmentLight 节点，但内建管线只消费 `FSceneSettings` 明确选中的一个每类节点。未选中节点仍可组织/保存/编辑；GUI 明示当前参与渲染的主光源，避免让用户误以为本次实现多灯累加。

方向光 emission forward 使用与相机相同的 -Z pose 约定，现有语义 `Engine.Scene.MainDirectionalLightDirection` 是 **surface-to-light**，因此上传值为 `-Forward`，必须测试符号。只旋转改变光照方向，平移不改变方向；父级旋转必须影响它。环境光的变换不影响辐射值。

有效 radiance = Color * Intensity（线性 RGB、有限、非负，乘积也须有限）。不在此次工作引入物理单位换算或 tone-map/exposure 变化。

| 状态 | 主光 radiance | ambient radiance | CSM |
| --- | --- | --- | --- |
| 选中且 enabled 的节点 | 来自选中节点 | 来自选中节点 | 按下述组合开关 |
| 未选择、选中节点被删除、或 ancestor/node disabled | 对应项为零 | 对应项为零 | 没有有效方向光则关闭 |
| 主光 Intensity/Color 为零 | 零 | 不受影响 | 无需阴影视图，关闭 |
| 主光 `bCastShadows=false` | 保持直射光 | 不受影响 | 关闭 |

没有有效方向光时仍发布有限 unit direction（固定 `{0,1,0}`，仅防 shader normalize 零向量），但 color 为零且有效灯标志为 false；不得用该方向伪造默认灯。`EffectiveShadows = Pipeline.bEnabled && SelectedLightEnabled && NonzeroRadiance && bCastShadows`。用本帧 settings 副本传给 CSM，不能回写持久 Pipeline settings。关闭后 neutral shadow bindings 和 preview 状态同步，旧 map 不再被当作有效阴影。

CPU Scene 提供显式默认内容 helper，插入真实相机/灯节点并设置选择。旧场景迁移以及 ModelViewer 新建临时场景时调用；FScene 构造器保持空场景，不自动创建灯。兼容默认为 surface-to-light normalize(`{-.45f,.8f,.65f}`)、方向光 Color `{3.f,2.85f,2.7f}`/Intensity `1.f`、环境光 Color `{.22f,.25f,.3f}`/Intensity `1.f`、主光投影 true。

`FRenderSession` 现有默认参数仅能用于 **未绑定 logical scene** 的低层显式测试/实验。绑定新场景时忽略这些保留兼容值，并完全用场景发布生成受保护输入；关闭/重载不能继承旧场景灯。低层 provider tests 无需伪造场景节点，但新增业务验收必须走真实场景。

### D5. publication token 与帧边界

不把 Main 的 `FScene*` 交给 Render，不建立每帧全场景深拷贝或无界历史。沿用单 Main producer、单 ordered Render consumer，在同一队列上保证如下顺序：

```text
Main:  交互/动画/加载完成 → Scene.Update → Bridge.Flush(Pn)
       → FreezeSceneFrame(Pn) → FramePipeline.Submit(Frame n)
       → 允许下一 tick 的 Scene.Update/Flush(Pn+1)

Render: Apply(Pn) → Resolve/Build(Frame n) → Apply(Pn+1) → Resolve/Build(Frame n+1)
RHI:    已冻结的 Graph n / draw 数据，可与 Main n+1 并行
```

引入 `FScenePublicationToken`：`LogicalSceneIdentity`、`AttachmentEpoch`、单调 `PublicationSerial`、`LogicalRevision`。epoch 每次 attach 分配新值（包括同一 FScene close/reload/reattach），解决同一 scene identity 的 ABA；publication serial 在仅 editable material 改变时也递增。所有字段进行完整比较，禁止 hash-only 判等。

Bridge.Flush 返回/公开最新成功接纳的 token 和 scene-wide receipt；未变化复用 token。每次attach后首次Flush必须发布完整状态，即使场景完全为空也发布serial=1的空metadata，不能因为Changes为空跳过初始化。首次Flush之前GetToken明确返回未初始化/失败，不构造serial=0的可用seed；scene加载未完成的tick也必须到达该Flush。`FRenderSceneClient::PublishGroups` 扩展为可包含 camera/light/settings 元数据的同一次 publication（可新增统一方法并让旧 PublishGroups 包装）。即使没有任何 primitive，纯相机/光源发布也必须提交。metadata 完全为 CPU immutable values，不能包含 FScene、FMaterialInstance、插件或 GUI 的可变指针。

Main 预先完成：node validation、受影响模型的冻结 material snapshots、primitive updates/removals/new groups、camera/light/settings 候选元数据和 receipt 所需内存。只有 Render admission 成功后才 ack logical changes；分配/dispatch 失败不吞 changes、不更新 latest token，可重试。

单一 Render publication 任务完成 primitive work 后，安装 scene metadata 和 AppliedToken；本任务内不调用会 pump 其他 Render publication 的等待。模型 Data 为空/资源未 ready 是既有 per-model pending/failure，不阻止独立相机/灯光存在。意外的发布任务异常可能已部分改变 render internals：**不声称具备任意 Render 异常回滚**。记录 scene publication failure，拒绝后续 scene frame build，允许 cleanup/Close/reattach；不能继续渲染“新 metadata + 半旧 geometry”。所有 receipts 包括无模型发布的错误都必须可由 Main 观察。

新增 `FreezeSceneFrame(Token, Time, CustomFrameValues)`（Main）产生 immutable frame seed，冻结 Global/Frame/custom Scene 输入，分配既有 frame serial并一次 Freeze providers；seed 保存 token，尚未提供 Renderer 场景灯光。随后 `ResolveSceneFrame(Seed, Request)`（Render）要求 AppliedToken **精确等于** seed token，取当前 Render 场景 metadata，派生 view 和最终 `FMaterialFrameContext`。所有视图共享本帧最终 scene inputs。

该 token 是队列绑定的提交凭据，不是任意历史版本查询 API：若调用者 Freeze 后先提交了另一 scene publication，再尝试 Build 旧 seed，应在产生图/绘制前报告 `ScenePublicationMismatch`，不能取“最新”状态补救。正常 Viewer 必须将 Flush/Freeze/Submit 放在同一次非等待 Main admission 区段；若需要等待就先完成等待再捕获 token。FramePipeline.Submit 当前在 dispatch 后才限制 Main lead，保留此顺序。

Resolve 得到的 frame、view、prepared scene snapshots 通过 shared immutable ownership保留，后续 RHI prep只读这些值。Graph extension callbacks、deferred lighting、CSM及共享材质上下文不得在 RHI 执行时再读取 Render 当前 metadata。原有 GPU resources/constant pages 继续由既有 frame leases/fences 保活。

新增场景请求入口到 SceneRenderPipeline；可先在入口 Resolve 后复用其原有 build-body，也为 ForwardRenderPipeline 提供相同入口/内部 helper。低层旧 explicit-view overload保留。业务场景使用新入口；绑定场景时直接用旧 FreezeFrame/高层 explicit-view build绕开 token 必须明确拒绝。Renderer 内部派生 CSM views 的 BuildViews 继续使用已解析的最终 frame，不再二次解析。

### D6. 材质语义接入与保护范围

不修改 `HyperionViewV1`（80 bytes）或 `HyperionSceneV1`（48 bytes）标准布局，不新增 camera/light RHI API。Scene CPU 模块不构造 FMaterialValue；转换只在 Renderer 完成。

由同一个 resolved scene frame 提供三个 Scene 光照语义；view 解析提供 `Engine.View.ViewProjection` 和 `Engine.View.CameraPosition`。Forward材质、Deferred全屏 Lighting、CSM主光源方向使用完全一致的数值。相机数据先解析，再做 shadow preparation、BVH query、transparent sorting、view provider preparation。

为避免继续有多个权威输入源，绑定场景的 frame seed 验证以下五个 canonical semantics：三个光照语义和两个 View 相机语义。

- 添加 provider registry 的只读查询/验证能力；在 FreezeSceneFrame 前发现这五项的 custom provider registration，显式报告冲突并拒绝本次场景帧。未绑定场景的通用 registry 及其现有 override tests不变。
- SetSceneParameters 在绑定场景时拒绝含内建三项灯光的 batch，整个 batch不更新；自定义 scene 参数仍允许。绑定前遗留的兼容灯光值在 scene Resolve 时剥离/替换。
- 场景绑定的 Global/Frame/custom Scene/View request参数中不允许写入上述受保护语义，使用 canonical name 检查并在生成快照前拒绝；不做 setter-order 覆盖。其他语义/手工参数仍走原先逻辑。
- 不全局禁止自定义材质、混合 scope providers或原来允许的 material override。标准内建 view/scene block保持 locked来源；自定义 shader显式使用自己的参数可以实现不同效果，但不能改变 Renderer 的场景相机/主光/CSM。
- 保留现有未绑定场景的光源 provider允许 Global/Frame/Scene、排除 View/Pass/Material/Object/Draw 的 CSM tests；绑定场景的灯光值来自 scene发布而不走自定义 provider覆盖。

实现方式：frame seed和final frame应有不同类型或明确不可伪造的 resolved标志/owner凭据；`BuildViews`只接受 final frame。FreezeFrame 未绑定场景分支继续直接产生 final frame。Scene scope key包含 scene identity/epoch及 **有效光照+自定义 scene输入版本**，不使用每次 camera motion都会改变的 logical Revision。final frame增加 publication token/provenance用于诊断，不让它参与不相关GPU布局缓存。

### D7. 增量发布、逻辑树与 BVH 的边界

维持 Render 的模型组索引；camera/light/group 不创建零几何 primitive、不进入 unbounded geometry集合。模型组边界仍由现有成员 world bounds 计算，Scene tree不能替换它。逻辑节点数与 model/group/primitive/item/draw计数分开。

| 变化 | Scene/Bridge工作 | Render失效范围 |
| --- | --- | --- |
| 无变化 | 不重新遍历全部 model/node；无 publication | 复用既有状态 |
| rename / camera FocusDistance | 更新持久化/编辑元数据 | 不更新 geometry/BVH/material blocks |
| camera pose/lens | 更新相机有效记录 | 相关 view/culling/CSM依赖；geometry revision不变 |
| light color/intensity | 更新有效 light radiance | Scene共享常量；不重建 geometry/BVH；方向不变时不无故重算级联矩阵 |
| light direction / cast switch | 更新主光方向/开关 | 光照及阴影视图；geometry/BVH不变 |
| unselected light/camera参数 | 保存节点变更 | 未使用视图/光照不重复打包 |
| ancestor transform/enabled | 迭代传播受影响后代，分别输出模型/相机/灯光变更 | 只对实际变化的模型组refit/visible state刷新 |
| model数据/材质/几何变换 | 继续现有材料冻结、绑定、发布 | 保持既有资源、prepared view及batch规则 |
| model增删 | 成员变化 | 沿用 BVH批量rebuild策略 |

Render metadata可采用 immutable camera table与独立 light/settings snapshot；只有相应内容改变才重建对应表，不能相机变化时复制模型数据/材质列表。首轮允许复制相机小表，不允许以此扫描所有模型。metadata table不保留历史链；旧帧直接持有需要的 immutable值，cache拥有量仍按既有容量限制。

保持 `FRenderScene::Revision`/`GetCollectionRevision()` 对几何收集的意义：camera/light-only publication不得调用触发 `InvalidatePreparedViews()` 的通用 geometry `OnChanged`。可以增加单独的 Camera/Lighting/Metadata版本；逻辑提交serial只用于顺序/一致性校验。shadow cache的 scene key仍为几何/resource revision，camera/light变化已在其 preparation key中体现。

每个 shadow view继续独立查询同一 geometry BVH，包含主相机视锥之外的caster。冻结剔除只是 View保存的诊断 VP，不改 scene camera；绘制、透明排序与CSM使用当前渲染相机。

### D8. 原生 v4、source v2 和稳定 ID

原生 `hyperion.scene` TypeId保持，版本从3升到4。`FSceneManifest` 新权威内容：`Assets`、`Nodes`、`DefaultCamera`、`MainDirectionalLight`、`EnvironmentLight`（后三者序列化为稳定节点 ID字符串，空表示未选择）。删去顶层 Eye/Target/Near/Far及Instances的运行时权威用途，迁移后不同时保存两套描述。

新增 `FSceneNodeEntry` record（建议 TypeId `hyperion.scenenode`，v1）：

| serialized key | 含义 |
| --- | --- |
| `id`, `name`, `parent` | parent为空是根；引用节点ID，非runtime Handle |
| `transform` | 完整局部 FMat4，保留 lossless affine 数据，不强行分解TRS |
| `enabled` | 本节点启用，默认true |
| `model` | optional payload：asset ID、visible、现有Material/Surface/SectionSurfaces |
| `camera` | optional FSceneCamera：verticalRadians、near、far、focusDistance |
| `directionalLight` | optional payload：color、intensity、castShadows |
| `environmentLight` | optional payload：color、intensity |

四个 payload最多一个；无payload为Group。新 CPU/record成员布尔名遵循 b/bIn规则；既有材质/asset/ref字段不重命名。旧 `FSceneInstanceEntry` descriptor（v3）保留供历史迁移/读取，不作为新场景实时存储。共享payload record可复用 Scene CPU类型；模型持久化payload不含 Data/指针，继续使用 `FSceneMaterialAsset`。

原生迁移：保留已有root/instance/asset/material v1-v3路径，新注册 `FSceneManifest` 的3→4 migration。须注意 root Migration执行时nested instance可能仍是早期版本：逐个用 `ReadValue<FSceneInstanceEntry>`执行其自身migrations，再生成新node，不直接假定archive中的transform已经存在。已有empty v1→2→3 root migrations不用重写历史记录。

3→4步骤：

1. 验证旧assets/instances与eye/target/near/far，恢复各旧instance为root Model node，Id/Name/完整Transform/visible/material/surface/sectionSurfaces保持。旧 visible迁移到model.visible，node.enabled=true。
2. 由 `Eye/Target`构造相机 World=`Inverse(LookAt(Eye,Target,{0,1,0}))`，FocusDistance=Length(Target-Eye)，FOV=`.729224966f`，Near/Far保留。旧缺失字段用旧默认值。
3. 插入D4默认directional/environment nodes，设置三个settings选择。
4. 新增ID从 `camera-main`、`light-main`、`light-environment` 开始，与全部旧node IDs冲突时按 `-1`、`-2`递增找未用值，保证重复迁移确定性；不得覆盖用户已有ID。
5. 写入新fields，删除旧instances/eye/target/near/far，不接受同时存在新旧冲突权威字段的输入。

plain source继续接受 `schema_version:1`：先解码旧source到legacy描述，再调用同一兼容转换逻辑；不能先构造新FSceneManifest再指望不存在的Eye成员。新增 `schema_version:2`：assets沿用id/path形式，nodes使用上述payload和parent ID；局部变换允许旧风格translation/rotation/scale或16元素column-major `transform`，二者互斥。rotation使用现有normalized `[x,y,z,w]`约束。root选择字段为 `defaultCamera`、`mainDirectionalLight`、`environmentLight`。model payload中的材质值通过现有材质解码adapter/反射表示读取，不发明第二种覆盖优先级。

v2和原生v4 **不注入默认内容**。合法空场景或无灯场景保持为空。资产表继续只接受model references；camera/light payload不会产生异步模型依赖。新kind/投影类型没有实现时明确拒绝，不当作group吞掉。

校验整个文档后再安装任何新场景节点；node顺序任意，分两步建立ID→Handle和parent关系。保留既有Load先Close的语义：新manifest失败可以留下明确的加载错误和空场景，不要求本次实现后台保留旧活动场景。拒绝重复/空ID、悬挂parent/选择、选错类型、环、多个payload、非法lens/light/matrix和冲突变换表示。未选择相机合法；指定不存在ID不合法。

提高 `hyperion.scene-json` importer revision（建议1→2），并核对native upgrade路径及content fingerprint，使schema变更后缓存确实重导入。保持importer/plugin/type IDs稳定。更新shipped场景source到v2时显式写入原相机和默认灯，尤其Sponza固定构图；保留专用v1fixtures验证兼容。不修改out/content二进制作为source，不重写OpenSpec archive历史。

除SceneJson外必须迁移`AssetPublication.cpp`中的model→scene包装：生成一个model节点，显式调用兼容camera/light初始化，保持其旧默认构图而不自动新增fit策略。AssetTool inspect继续输出`instances=<model count>`供现有脚本使用，可追加nodes/cameras/lights；依赖数量仍由实际FAssetRef决定，camera/light不得伪造依赖。测试`PublicationTests.cpp`、`SharedAssetPublicationTests.cpp`和反射VisitRecord对optional node.model内部material/texture引用的遍历与rebasing。

清查Python脚本对旧`["instances"]`的直接访问，至少包括`Integration/FramePipelineAcceptance.py`、`ScenePerformance.py`、`tools/BatchPlannerComparison.py`和`tools/MeasureShadows.py`。读取更新后的shipped v2文件时按model payload计数/编辑；显式生成v1兼容fixture的SceneAcceptance/DepthAcceptance/GenerateShadowFixtures/CpuSubmissionBenchmark可以保留v1。性能脚本的fixture选择必须与实际`--scene`参数一致，不能把Sponza默认配置和Showcase计数混用，也不能删除现有coverage断言掩盖错误。

### D9. FSceneInstance加载、编辑及保存

FSceneInstance保留在Renderer作为Main生命周期协调器，不为本次工作搬动整个模块。新增 typed node/transform/settings委托、node enumeration及publication token访问；只读访问不泄露可变Scene/Bridge internals。模型Add/Update/Remove调用点改为节点操作；新model数据替换必须清除旧pending asset association（沿用当前保护）。

加载顺序：读取并验证完整manifest → 创建所有CPU节点/关系/settings → 开始model/material异步请求 → 在Tick中附加已完成Data/材料 → Scene.Update → Bridge.Flush。camera/light立即存在，不等待模型ready。manifest请求尚未ready时保留空scene，但Tick不能在首次Scene.Update/Bridge.Flush之前return；这样加载中和合法空场景也有有效publication token并走NoActiveCamera/clear路径。异步回调通过完整Handle+load epoch检查：删除/reload/替换后不得复活节点，不得覆盖加载期间对transform/enabled/camera/light/material的编辑。移除父子树必须清理所有对应model pending maps和计数。

structured status保留Models/ReadyModels/FailedModels语义，并新增Nodes/Cameras/DirectionalLights/EnvironmentLights、scene publication错误及可解析相机状态。model readiness不能混入camera数量；无有效camera用单独状态报告。加载一个model失败不删除其他节点，不能无条件阻止camera/light操作。

Snapshot从当前节点与settings生成manifest，不先复制旧manifest中的camera/light再补字段。保存前完成Main派生状态更新/验证，但无需等待Render或GPU。仅序列化Local、payload、稳定ID和当前引用；不写World缓存、Handle/epoch/revision、资源或GPU指针。Snapshot是独立值，后续编辑不改变正在写入的版本。

Save As：沿用已有资产和material/texture引用rebasing（`Assets.Resolve` +相对目的目录），保留pin/type/revision信息；只保留被live model payload使用的assets。无模型场景可保存空assets。source-less model、无法持久化的材料或仍在pending/failed material选择的情况继续显式拒绝；camera/light不因没有model资产而不可保存。保存失败不得改变活动场景、主光或相机选择。

Close：取消并join全部已admit加载/preparation；发布全部节点移除/空metadata，观察所有scene级receipt，然后detach epoch。cleanup必须在publication失败后也能执行，不能依赖失败任务的成功continuation。已build的旧Graph继续靠immutable snapshot/leases存活。再Load使用新epoch并从新manifest建立选择，不使用上次Viewer缓存。

### D10. Viewer集成和实际可用的场景树

两种Viewer公开窄的场景交互入口（例如Get/Set当前camera、Get/Set选定light、获取node tree/settings），Application光源面板通过它们委托FScene。避免通过raw FScene*让应用绕过FSceneInstance pending-load bookkeeping。

SceneViewer GUI至少提供：所有kind的层级列表/缩进、稳定选择（Handle而非model数组index）、新增Group/Camera/DirectionalLight/EnvironmentLight、现有model添加/复制、删除子树/保留子节点两种明确动作、父级选择和KeepLocal/KeepWorld重挂接、局部变换与enabled、model visibility、相机FOV/Near/Far/FocusDistance、光源Color/Intensity/CastShadows，以及三个default/main选择。使用现有FGui接口；必要的通用tree/input wrapper实现在Gui私有adapter，不在插件直接调用ImGui。可用已有Select/Slider/Button组合实现，不为美化引入大型GUI系统。

复制camera/light/group时分配新稳定ID；复制子树需重新映射内部parent，保留相对关系。UI默认仅复制选中节点（不自动复制后代），明确按钮含义；model数据和显式material instance sharing保持现有语义。清除所有模型后仍可看到/编辑camera/light/group，模型添加和Fit空场景行为不崩溃。

光源GUI角度是从当前scene世界方向派生的临时显示值，只在用户真的改变slider时转为node edit；Application不再存权威ShadowLight/azimuth/elevation。`--shadow-light`在scene CPU节点初始化完成后执行一次，修改选中的方向光；无选中有效kind节点时显式创建一个并设为main（仅因用户显式override，不是每帧fallback）。benchmark每tick修改同一个scene节点，停止benchmark后也不恢复应用默认光。`--benchmark-camera`仍走相同输入/场景编辑接口。

帧顺序中所有业务编辑（输入、GUI、动画、benchmark、已ready加载结果）在Scene.Update/Flush之前；避免当前 `P.Scene.Tick()`后再`UpdateCamera(InFrame)`的绕路。GUI保存按钮捕获当时已应用的编辑状态，不依赖稍后渲染回填。

SceneViewer.SaveAsync仅调用当前Scene.Snapshot和Assets.SaveAsync，删除补Eye/Target/Near/Far逻辑。ModelViewer为打开的model建立临时scene的model/camera/default lights，保留其原initial fit与加载界面。显式底层Triangle/overlay示例不强制创建scene camera。

保留现有CLI、plugin/config key、executable、target；新增选择控件可以增加文本/GUI，不更名既有性能列。诊断结果关联原始frame token，避免Main显示当前scene revision配旧GPU结果。

### D11. 文件分工建议

| 模块 | 建议新增文件 | 修改重点 |
| --- | --- | --- |
| Scene | `SceneNode.h`、`SceneCamera.h`、`SceneLight.h`、私有`SceneHierarchy.cpp`、`SceneCamera.cpp`、`SceneLight.cpp`、`SceneNodeReflection.cpp`、`SceneMigration.cpp` | Scene.h/cpp、SceneManifest.h/cpp、CMake；小函数、CPU边界 |
| Renderer | `SceneFrame.h`、私有`SceneFrame.cpp`、`SceneView.cpp`、必要的scene-publication helper | SceneBridge、SceneInstance、RenderScene、SceneMaterialPublication、SessionMaterialFrame/Binding、两条Pipeline |
| AssetImport | 视长度拆分私有scene JSON decoding helper | SceneJson importer版本和source v1/v2处理 |
| SceneViewer/ModelViewer | 控制器/树/属性面板私有文件 | 不把逻辑树实现或序列化放插件 |
| Viewer | 继续现有Frame/Shadows/Benchmark分工 | 移除相机/灯光权威值，Submit捕获token |
| Tests | Scene CPU node/camera/light测试、Renderer scene frame测试、Integration验收 | 见verification-plan.md的固定fixture矩阵 |

文件名按实际规模拆分，避免将scene更新、序列化、GUI和render binding塞进一个文件。新直接依赖在CMake声明；Scene不得间接带入RHI/Renderer。无需更改 native D3D12场景接口；若executor发现必须在D3D12识别节点类型，应停止并交回planner。

### D12. 验证和review交接

plain source v2的最小合法示例及字段约定见 `source-format-example.md`。完整验收矩阵、命令、性能判据及交付模板见 `verification-plan.md`。至少包含纯CPU树/lens/light/迁移、插件无关save/load、真实Render/RHI像素、gated跨帧、两条管线+CSM、BVH模式对照和移动性能。

executor交付必须包含：实际HEAD与文件列表、任务完成证据、源码接口映射、完整测试命令/结果/日志、raw benchmark与图像比较、已知问题/未执行项。所有待review任务保持未勾选；planner/reviewer独立检查真实代码和证据后给出findings，executor负责后续修复。executor不能以“能编译/截图看起来正常”替代版本/生命周期/迁移与缓存验证。

## Risks / Trade-offs

- [统一节点导致大面积调用迁移] → 保留Renderer FModel transfer接口和未绑定场景低层入口，集中改Scene/SceneInstance typed访问；通过编译器与rg枚举所有旧调用点，禁止维护两套可写world/camera/light。
- [parent操作使camera basis退化或KeepWorld逆矩阵无效] → 事务预检全部受影响后代，拒绝时无部分编辑；使用独立数值oracle验证变换。
- [Main比Render超前，旧frame拿到新灯光/相机] → 精确token/epoch、连续admission、Render解析后immutable handoff；gated测试必须故意触发错误队列顺序。
- [统一logical revision摧毁局部缓存] → publication serial与geometry/camera/light effective revisions分离；纯相机/光源变化不得走geometry invalidation callback。
- [旧灯光默认值污染新无灯场景] → 默认只在显式初始化/legacy迁移或unbound兼容路径；绑定场景输入全量替换受保护语义。
- [schema v4漏掉旧嵌套material/instance迁移] → old nested records先ReadValue再转换；source v1/native v1-v3独立golden fixtures，强制检查实际importer缓存失效。
- [默认主光选择隐藏多灯能力限制] → 显式settings选择和GUI显示；测试多个候选仅选中一盏，文档不宣称多灯累加。
- [相机pose矩阵转换引入小数差异] → CPU按epsilon检查，GPU用预先确定容差并报告max/mean；不能为过测试提高阈值或改变灯色/FOV。

## Migration Plan

1. executor在实现前冻结源码/二进制/场景及配置基线，记录原画面、CSM、计数和性能。
2. 先实现CPU node/层级/lens/light/record contracts与其测试，再接FSceneInstance加载/保存。
3. 扩展bridge/publication/token及Render解析，保留低层explicit路径；随后迁移全部业务调用者。
4. 更新source样例到显式v2内容，触发native content重导入；保留old fixtures而非删除compatibility。
5. 完成verification-plan中所有相关检查，executor交回planner/reviewer。修复review findings后再由用户决定归档/commit。

不存在外部服务部署。旧source输入可重导入，v4文件对旧程序不兼容；Save As不覆盖原fixture。回退实现时恢复对应源码及由旧source重新导入的native content，不试图让旧程序直接加载v4，也不删除用户原始场景。

## Open Questions

没有阻塞executor的产品选择：本次固定透视相机、单选主方向光+环境光、多个候选节点、逻辑树和分离BVH、schema v4/source v2。点光/聚光/正交/多灯累加/完整ECS须另行规划。若实际源码在执行前已偏离本基线、严格矩阵验证无法保留已有有效fixture，或需要改变上述契约，executor提供具体证据交回planner，不自行扩展或降级。
