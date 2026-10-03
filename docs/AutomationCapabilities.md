# Automation 能力与领域接入

本页按 Editor 的现有人类任务组织能力。精确参数、可用性、示例和完成语义以目标的 `api.search` / `api.describe` / `types.describe` 为准，不要求启动时枚举所有声明。连接方式见 [AutomationConnections.md](AutomationConnections.md)，通用契约见 [Automation.md](Automation.md)。

## Editor 进程日志

引擎关键操作的诊断由共享领域服务记录，GUI 和 agent 操作走同一日志路径；沿用 `application.log.read` 即可读取导入、保存、内容切换和场景准备等诊断，无需独立诊断接口。独立 CLI 的日志继续输出到 stderr，不占用 JSONL/MCP stdout。

`application.log.read` 与 **Window > Log** 共用当前进程的日志历史。`after` 为上次读到的序号（十进制字符串，默认 `"0"` 从启动开始），`limit` 默认为 100，范围 1–256。响应包含 `entries`、`next` 和 `total`；按字节预算可能提前结束一页，继续传 `next`，直到等于 `total`，之后可继续读取新增日志。

每行带 sequence、time、thread、level、source、message。等级 wire 值保持 Info=0、Warning=1、Error=2，并新增 Debug=3；schema 包含可读名称。stdout/stderr 使用 Info/Error，thread 是采集线程标识。多行或超过 8192 字节的文字按显示行/UTF-8 边界分段。关闭面板不影响读取，重启后序号重新开始；游标不能跨实例或重启复用。无 history provider 时返回 unavailable；非法游标或 limit 无副作用。能力由 `automation-log` 注册，可独立禁用。窗口显隐、拖拽和停靠属于呈现，不作为模拟输入操作暴露。

## 推荐调用顺序

1. 连接明确的应用实例，搜索当前任务需要的操作；查询场景文档 `scene.info` 或资产页签 `asset.info`。
2. 场景使用返回的 document/revision；资产打开后使用 document/generation。64 位数值按十进制字符串传递。
3. 先查询目标和当前值，再提交修改；批量修改保持一个请求中的全部目标合法。不要自动重试 stale revision 的修改。
4. 异步返回 job 时在同一连接轮询 `jobs.get`。编辑完成、资源准备、GPU 展示和磁盘保存是不同完成点。
5. 调用 `scene.save` / `asset.save` 才持久化草稿。关闭/切换脏内容需要先保存或明确 discard。

“保存全部资产”由分页查询 `asset.documents.list`、逐个调用 `asset.save` 并等待完成组成；“关闭全部”同样逐个使用 `asset.close`。保留每份文档的冲突和失败结果，不承诺跨文件原子事务。Editor 删除后清空对应选择，不自动选择替代物；GUI/agent 使用同一策略。

## 任务映射

| 人类任务 | 操作族 | 共享服务与范围 |
|---|---|---|
| 查找资产、场景 | `content.assets.list` | Content/Assets 原生索引；query/type、offset/limit、root generation。写入内容后重新开始分页 |
| 选择、清空资产目录 | `content.root.get/set/clear` | `FContentRootService`；Editor 与 GUI 同样检查 busy/dirty、关闭旧内容并保存 Preferences |
| 打开、新建、关闭场景 | `scene.open`、`scene.status` | `ISceneDocumentHost`；Editor 空 path 表示空文档，旧场景 ID 失效。失败不声称恢复旧场景 |
| 查询与编辑层级 | `scene.nodes.list/reparent`、`scene.node.get/create/reparent`、`scene.nodes.set_metadata/set_transform` | `FSceneEditDocument`；metadata/transform 最多 128 个目标，事务先全量校验。批量 reparent 接收 document/revision、非空唯一 handles 和 parent（null 表示根），固定 KeepWorld，选中祖先覆盖后代，保留内部层级和选择；一次历史事务，全部无变化时不增加 revision/history；单节点旧模式兼容 |
| 多选、全选、删除、历史 | `scene.selection.get/set/select_all/delete`、`scene.undo/redo/save` | 显式有序选择；末项为 primary。select_all 与 Ctrl+A 共用领域服务，包含全部逻辑节点、保留已有 primary，返回 document/revision/count/primary 摘要；count 为 64 位十进制字符串。set 的数量受 wire 预算而非 128 编辑批次限制；选择不改变 revision/dirty/history。Outliner 范围按显示顺序转换为显式集合后共用校验提交，agent 通过 set 表达集合，不模拟面板输入。Editor 删除后不自动选择替代物；undo 恢复并重新选择新 handle |
| 复制与保留子节点删除 | `scene.selection.duplicate/remove_keep_children` | SceneEditing 共享事务；操作 primary，复制共享资源，删除保留子节点世界变换，支持 undo/redo 和选择恢复；GUI 入口待后续 Outliner/viewport 交互设计 |
| 对象剪贴板 | `scene.selection.copy`、`scene.clipboard.info/paste` | 与 Ctrl+C/Ctrl+V 共用系统剪贴板和不可变子树快照；同一 Editor、同一文档；数字后缀、原父级/local、一次粘贴一个历史事务；普通文本替换使旧对象不可粘贴。返回摘要，新 handle 通过 selection.get 或 nodes.list 查询 |
| 放置 primitive/light/native model | `scene.placement.list/place/place_model` | `IScenePlacement` 与 Editor 共用候选、类型化资源准备和提交准入；内部 Ready/Pending/Failed 与显示文案独立，Pending 保持异步运行，Failed 返回 `load_failed`。`place` 使用注册预设 ID，`place_model` 使用原生模型 `FAssetRef`，显式 position 为世界坐标 pivot。异步准备后创建并选中一个节点，记录一次历史，需显式保存；过期 document/revision 或 busy 请求拒绝。Sky Light 默认使用 SkyAsset 与 Engine Cloudy；放置或新增天空光组件后按 Priority 和持久对象 ID 推导生效天空光 |
| Inspector 组件 | `scene.components.list`、`scene.component_types.list`、`scene.components.edit_structure`、`scene.component.<type>.get/set/set_batch` | `SceneEditing` 与组件反射；组件实例 ID 来自 list，不必等于类型 ID。保留资源绑定，完整候选经文档/场景校验；资源模型组件必须通过 prepared placement 创建 |
| 默认相机、初始视图 | `scene.settings.get/set` | 文档设置；get 后保留不修改的字段，set 是完整替换 |
| 灯光 Priority 与生效诊断 | `scene.component.<type>.get/set/set_batch`、`scene.lighting.get` | 与 Details 共用组件、校验、事务、历史和保存；Priority 为有符号 32 位整数，默认 0，越大越优先；诊断返回阴影方向光、天空光、顶层同优先级冲突、有效启用状态和天空资产错误。天空 `asset.state` 仍为字符串（空、unrequested/loading/uploading/ready/failed），`asset.error` 为独立字符串；无独立主光设置接口 |
| 浏览相机和临时视口选项 | `view.get/set/frame_scene/frame_selection/preview_camera` | `ISceneViewport` / SceneCameraController；`frame_selection` 与 F 共用服务，读取当前选中子树的世界范围，保留朝向/FOV，空选择不移动；需当前 document/revision、idle 与浏览视角，RMB 导航或普通弹出菜单期间返回 busy。`frame_scene` 为全场景重新拟合临时裁剪面。null option 表示此 host 不支持。patch 中 null 保留原值；曝光、Visualizer 0–6、独立状态/profiling HUD 和 0–255 分类掩码均为临时视口状态 |
| 用浏览视角编写相机 | `view.save_initial/create_camera/apply_to_camera` | Editor 与 GUI 同一文档操作；与临时相机移动区分 |
| 非场景资产页签 | `asset.open/info/documents.list/activate/close/save/undo/redo/rename` | Editor 发布 `IAssetWorkspace`，GUI 和多个 agent 使用同一草稿、历史与 busy 状态；其他宿主可使用独立 CPU 文档 |
| 模型属性 | `model.nodes.get/set`、`model.material_slots.get/set`、`model.primitives.get/set`、`model.roots.get` | AssetEditing 共享校验；节点/primitive 身份、几何和拓扑固定；引用先加载校验再提交 |
| 材质参数 | `material.parameters.get`、`material.passes.get`、`material.values.get/set` | 共享可编辑性、类型/维数、PBR clamp 和引用图校验；不允许修改系统/声明参数 |
| 纹理 | `texture.dimension.get/format.get/encoding.get/sample`、`texture.set_encoding` | 编码修改与 GUI 使用同一个 AssetEditing 异步工作流，共用 snapshot、busy、generation 检查和历史提交。sample 返回 mip/face 信息与源像素 RGBA，不返回 bulk |
| 天空属性 | `sky.radiance.get/specular.get/brdf.get/irradiance.get/convention.get` | GUI 中这些产品为只读；天空重新生成使用 import；名称使用 asset.rename |
| 资产预览 | `asset.preview.get/set` | `IAssetPreviewWorkspace`；模型/材质/天空 camera、曝光、形状、yaw；纹理 mip/face/channel/EV/zoom/pan/fit/checker。shape 为 0=Sphere、1=Plane、2=Cube；channel 为 0=RGBA、1=R、2=G、3=B、4=A，由 AssetEditing 的类型化选项映射校验，独立于 GUI 顺序/标签；重复设置不重建，临时预览不增加资产 generation/history |
| 渲染与配置 | `render.settings.get/set/save`、`render.shadows.get/set` | `IRenderSettings`；完整候选校验、revision 和原子保存；get/set version 2 的 activeReversedZ 表示已提交给后续场景/三维资产预览帧的实时值，完成不等待 GPU 呈现；save version 1 独立持久化；不修改场景/资产 history；shadow 参数为会话默认值，Priority 推导出的阴影方向光组件参数（若配置）优先 |
| 灯光阴影属性 | `scene.component.hyperion.scenedirectionallight.get/set` | `shadowSettings` 可选嵌套组件字段；SceneEditing 验证、历史和原生保存；point/spot 尚不支持阴影 |
| 渲染结果和诊断 | `render.statistics`、`render.component_diagnostics`、`render.screenshot` | 完成帧统计、分页 primitive 诊断和现有 readback。PNG 成功返回时文件已写入，返回目标本地路径、尺寸、frame 和 bytes |
| RenderDoc | `renderdoc.status/capture/open/set_preference`、`renderdoc.hud.get/set` | 复用 host capture/replay；HUD 默认隐藏、独立持久化、已加载时即时生效；HUD 返回 preference 与实际 enabled，运行库不可用时 enabled 为 null，仍可保存偏好；抓帧不可用时返回 unavailable |
| GUI scale / Profiling | `gui.scale.get/set`、`profiling.get/set` | 共用 GUI scale 和 Core profiling 控制。scale 下一 GUI frame 生效并沿正常偏好路径保存；profiling 取决于构建和 collector |
| 导入与发布 | `asset.import`、`asset.import.capabilities/validate/tasks/task` | `FAssetImportWorkspace` 共用 GUI 请求、校验和任务；GUI 自动校验，提交时再次校验，完成后以模态框显示成功或失败原因，不提供独立 Validate 按钮；底层复用 importer/publication lease；不静默覆盖已打开草稿 |
| 导入输出目录选择 | GUI Save as / Browse；agent 使用导入请求的 `output` | 原生目录选择器与上次目录偏好仅为 GUI 适配，暂不提供自动化偏好接口；选择后转换为 `/Game` 路径，仍使用共享导入校验与发布 |
| 导入属性草稿 | `asset.import.draft.prepare/get/edit/history/submit/discard`、`asset.import.drafts` | 同一 workspace 的未发布快照、版本、属性限制与历史；GUI/agent 共享 ID；GUI 在 Import Asset 的 Asset properties 折叠区按资产类型编辑，保留 Undo/Redo/Reset；无 GPU 预览 |

## 数值与内容发现

`material.numeric.get` 根据参数 name 返回有效数值（override 或 default）、type、semantic、editable 和 overridden。`material.numeric.set` 接收当前 document/generation 和 1–100 个 `{name, values}`，支持顶层 Numeric 标量、向量和矩阵，每项长度为 rows×columns，按逻辑行优先排列。名称从 material.parameters.get 取得；Float 转 float32，Int/Uint 检查 32 位范围及整数性，Bool 使用 0/1。非有限值、重复 name、维度错误或只读参数使整批拒绝。

转换后仍经过 GUI 共用的 AssetEditing 校验、PBR clamp 和一次文档事务。numeric.get 读回实际存储值，一次 undo 恢复整批，asset.save 才持久化。聚合与资源参数继续使用原有 typed material.values；raw words 保留兼容。

`content.directory.list` 回答“目录有哪些候选项”，content.assets.list 回答“索引有哪些已识别资产”。前者包括未索引 native 文件、空目录和访问诊断，可逐级分页发现未知坏文件的路径，再加载诊断；不把未索引直接判断为损坏。两者使用当前 root generation，不提供跨内容修改的分页快照。

`asset.workspace.policy` 明确失败条目策略：下文 loading/failed 页签指附着 Editor workspace；standalone/无 workspace 宿主只保留 ready 草稿，打开失败由 job 返回。常规轮询优先用 application.health 查询场景就绪和错误，完整渲染计数仍由 render.statistics 提供。

## 值与完成语义

`view.frame_selection` 在 Main 提交独立浏览相机，后续渲染帧使用新状态，不修改 selection、场景 revision、dirty 或历史。Group 包含后代并去重；隐藏或停用模型使用已有几何边界，灯光/相机/空 Group 使用世界位置与半边长 0.5 的范围。选中模型仍加载时返回 busy，终止资源失败可使用其位置取景。相机预览返回 unavailable，过期 document/revision 拒绝且不移动镜头。现有 `view.frame_scene` ID 和输入/结果契约保持不变。

资产序列字段 get 支持 offset/limit（1–100），返回 total/next；set 可使用 offset 替换现有范围，不隐式 resize。每页带当前 generation，修改后从第一页重新查询。字段类型来自成员指针和现有反射，不另写 JSON schema。

材质 `value.type` 描述 numeric 的 scalar/rows/columns、texture 维数或递归数组。`words` 保留现有 32 位存储表示：float 使用 IEEE-754 float32 的位模式，int/uint 使用对应 32 位整数；GUI 与 agent 都经过同一参数校验。数组使用 elements，纹理使用 texture 引用。先查询 parameters 的 source、overridePolicy、overrideScopes 与 semantic，再编辑 values；PBR normalized 值按 GUI 相同规则限制在 0–1。shader/pass/参数声明是查询接口。

场景组件 set 提交逻辑状态后，渲染资源可能仍在准备；使用 `scene.info` / `scene.status` / `render.statistics` 与 component diagnostics 查询。资源终止性失败可被后续编辑修正，不永久挡在 busy；加载中的工作仍遵循共享准入。加载失败和截图失败不会伪装为成功。

`render.screenshot` 支持 main，Editor 另支持 assets。必须显式指定 PNG 路径；覆盖需要 overwrite。窗口需要可绘制，不要求场景或 preview ready，因此可捕获加载及失败界面。捕获输出写在目标机器，不代表自动传输文件到客户端。RenderDoc 的 capture 等到完成的 capture 计数前进，open 才启动目标侧 replay。

导入支持 glTF/GLB 及其图像依赖、独立 PNG/JPG/JPEG、HDR/EXR 天空；不支持 `.hasset` 原生资产或任何自有资产 JSON 导入，sky 设置仅用于 HDR/EXR 烘焙。sourceRoot/sourceId 成对使用，输出与源分离，force 仅跳过增量判断，不绕过写权限和身份检查。导入、发布、保存和截图接收后不支持取消；jobs 返回真实 cancellable 状态。保存后的草稿若继续编辑仍 dirty，导入不偷偷重载已有草稿。

`textureEncoding` 仅用于独立图片，0 为 Linear、1 为 sRGB，省略默认 sRGB。可选 `sky` 对象含 radianceSize/specularSize/samples，指定 HDR/EXR 全景图的烘焙设置，省略时使用默认值。`name` 用于模型、模型场景节点或独立图片；天空名称默认来自源文件名，也可通过导入草稿修改。旧字段与 `asset.import` 异步调用方式保持兼容。

`createFolder: true` 使用与 Editor 默认导入相同的分组策略：在 output 父目录下按来源文件名创建目录，主资产保留 output 文件名，附属资产与其同目录。相同来源与请求输出位置跨会话复用；不同来源或无关内容占用名称时追加数字后缀。省略该选项保留原有精确路径行为；启用时不另指定 library。准备草稿不会创建目录，结果 asset.path 与 task.output 返回实际发布路径。已有资产不自动移动。

`asset.import.validate` 不写文件，校验 generation、路径、格式、参数及挂载权限；最终转换、身份与依赖检查仍在导入时执行。结果 folder 为实际目标文件夹候选（含重名后缀），与 GUI Save as 共用；output 保留请求路径语义，结果不代表文件预留。`asset.import.tasks` 按最新优先分页，limit 为 1–32，最多保留 128 条应用任务，换根清空；`asset.import.task` 接收其 task ID，可查询 GUI 或其他连接发起的任务。它们不是 session job ID，不改变 jobs.get 的会话隔离。结果保留 asset/writtenAssets/upToDate，并增加 task/warning；warning 表示发布已经提交、后续索引刷新失败，不应盲目重试。

`asset.import` 的可选 `rootId` 与 AssetTool `--root-id` 共用导入校验，支持以固定的 32 位小写十六进制 ID 重建缺失或损坏的根资产。已有有效目标必须匹配该 ID，同一发布库中其他资产已占用该 ID 时拒绝发布；该选项不修改既有资产身份。`sourceRoot`、`sourceId` 和 `rootId` 仅供自动化及资产工具使用，导入 GUI 不暴露这些字段，使用共享服务的默认来源标识和身份处理。

`asset.import.draft.prepare` 接收相同导入请求，返回独立的 draft ID 和 preparing 状态；轮询 `draft.get` 到 ready 或 failed。准备和属性编辑不发布文件。应用最多保留四份草稿；`asset.import.drafts` 列出跨连接和 GUI 的草稿。`draft.get` 的 offset/limit 分页节点、primitive、材质参数、材质槽、依赖、产品与诊断，limit 为 1–64，默认 32；返回各列表总数及完整属性覆盖集，不返回几何或像素 bulk。

纹理草稿的 get 结果保留 `width`、`height`、`mips`、`format`、`encoding` 和展示用 `details`，并增加反射字段 `dimension` 与 `pixelBytes`。`dimension` 使用 Texture2D/Cube 枚举；`pixelBytes` 是全部 mip/face 像素 payload 的总字节数，以 64 位十进制字符串编码。非纹理草稿该字节数为零，dimension 缺省值不表示存在纹理。GUI 直接消费这些字段，不解析 details；导入类型与设置按能力中的稳定 type ID 选择，列表位置只用于呈现，支持的来源格式不变。

`draft.edit` 必须带最新 generation，properties **替换完整覆盖集**，请从 get 保留其他已修改字段。name 修改模型/贴图/天空/材质根名称；nodes 按稳定 id 修改 name/local，primitives 按 id 修改 name/material，material 按参数 name 修改 values。只接受已有模型元素与材质允许的 numeric 值；场景结构和生成依赖只读。`draft.history` 的 action 为 undo/redo/reset，最多 64 步，reset 可撤销。无效修改不会改变版本。所有 mutation 的 generation 是草稿版本，不是内容根 generation；后者只用于 prepare 请求。

`draft.submit` 发布当前快照并返回应用 import task ID，通过 `asset.import.task` 查询实际完成结果，draft 同时进入 publishing；结束后重新查询 draft 获取新 generation。发布前和提交时重新检查所有捕获来源指纹，force 或 up-to-date 不能绕过。成功后 dirty 清除，后续编辑可继续提交。属性覆盖写入导入 provenance，相同请求和覆盖保持重复导入零写入。源或转换参数变化需显式 discard 旧草稿并 prepare，不自动迁移编辑；新草稿从源重建。`draft.discard` 对 dirty 草稿要求 discard=true，准备/发布期间返回 busy；隐藏窗口不会 discard。draft ID、import task ID 与 session job ID 不可混用。

可选 provider 缺失、未编译 RenderDoc/Profiling、关闭 GUI 等配置会返回明确 unavailable，并保留其他分支。`--no-instance-batching` 是初始值，GUI/agent 可通过相同视口服务调整。

`view` 的既有 `animate` 可空字段保留 wire schema 兼容性。Editor 返回 null，对非空修改返回 unavailable；通用动画播放服务及其 GUI/Automation 适配尚未实现，固定演示动画不再作为支持能力。

## 失败状态、批量修改与退出

- `asset.documents.list` / `asset.info` 包含 loading/failed 页签的稳定 ID、state/error；未构造草稿的 generation 为 `"0"`。可 activate 或 close 后重新 open，不必清空整个 workspace。
- `scene.component.<type>.set_batch` 的 handles/components/values 三个数组一一对应，长度为 1–128。读取各对象后分别保留未修改字段，完整批次验证后一次提交和 Undo；旧 set 的广播语义保持不变。
- `scene.lighting.get` 只读返回当前推导结果。编辑灯光通过 typed component 操作和 Transform 操作，与 GUI 相同；旧 `light.main.get/set` 已移除。相机浏览、剔除和渲染诊断设置为临时状态。
- `render.statistics.sceneError` 优先报告当前 scene producer 的终止错误，否则报告场景准备错误。
- `application.close.status/request` 由 Editor 发布正常关闭服务。action 为 0（默认：拒绝未保存内容）、1（保存后退出）、2（明确丢弃后退出）、3（取消退出，不取消已接收保存）。Editor 未命名脏场景保存退出须提供 scenePath。
- 未发布导入属性修改参与 close 的 dirty 与内容根保护；action=1 拒绝 dirty 导入草稿，须先 submit 或显式 discard，action=2 明确丢弃。准备/发布中的草稿参与 busy；正常退出按插件生命周期排空工作。
- close.request 返回的是接受状态，不是进程已经退出；保存期间状态为 saving，失败为 failed 并保留应用。成功后目标正常排空、撤回发现记录并断开连接。关闭时最多排空应答 2 秒；断连/强杀不保证远端收到应答，不应自动重试破坏性请求。
- `render.statistics` 同时返回帧间隔、CPU hooked bytes 和按执行域累计任务数，GUI 使用同一快照；这些是诊断计数，不代表操作耗时或进程全部内存。

## 新功能维护方式

- CPU 事务和校验放 SceneEditing / AssetEditing；纹理编码与引用异步编辑由 FAssetEditWorkflow 统一 admission、准备、身份/generation 检查和历史提交，workspace/automation 负责 polling 与关闭排空。Renderer 提供渲染、相机、capture/readiness 的 engine-owned 接口；宿主发布当前实例的 typed service。不要为 agent 新建第二份 Editor 文档。
- 新属性优先复用反射成员；新组件注册类型与 typed operation。临时状态、保存状态和 generation/revision 的语义必须写进说明。
- GUI 的候选值和 agent 的候选值最终调用同一领域方法。Inspector 提示不是授权依据；不可变身份由业务规则保证。
- DTO 不直接复用只用于 Inspector 的 transient-only record；API 投影须显式声明可序列化字段，验证真实返回值。
- 可选服务用 Find，不能在只有 Optional 声明时 Require。注册失败必须撤回 owner；已接收 job 在 provider 销毁前排空。
- 验证 discovery/describe/call、正常和非法输入、跨连接可见性、history/save/reopen、失效/缺 provider/退出。`automation_capability_parity` 是真实 CLI/MCP 验收，GUI 回归独立覆盖鼠标路径。

保留现有 transport、catalog、session、平台通信及插件架构；连接关闭时新增最多 2 秒的已排队应答排空，避免正常退出丢失接受结果。窗口停靠/大小/按键属于呈现，不通过模拟输入暴露；已有 AssetTool 的库迁移、Engine 内容生成、测量和 envelope 导出仍是独立维护 CLI。未来 Public C++ 的新能力仍按领域任务接入，不能将本页理解为所有 C++ 方法的自动 RPC。

未启用 `createFolder` 时，导入省略 `library` 即将新增附属资产直接保存到 `output` 的父目录；已有共享资产继续复用原位置。兼容工具调用仍可显式指定 `library`，该目录同样不自动追加类型子目录。

导入 workspace 的输出与可选 library 必须解析到当前可写 `/Game` 内，未设根、越界及目录链接指向根外均拒绝；根内本地绝对路径会规范化为 `/Game` 路径。GUI Browse 更改父目录后复用同一校验更新只读 Save as 并显示错误；完整导入仍检查来源、转换与发布条件。

Import Asset 在来源、输出或转换设置编辑结束后自动更新属性；已有属性修改时先确认重置，取消保留原设置和修改。纹理尺寸、格式、色彩编码和 mip 层数使用对齐的只读属性行，关闭使用窗口标题栏按钮。
