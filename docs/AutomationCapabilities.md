# Automation 能力与领域接入

本页按 Editor / Scene Viewer / ModelViewer 的现有人类任务组织能力。精确参数、可用性、示例和完成语义以目标的 `api.search` / `api.describe` / `types.describe` 为准，不要求启动时枚举所有声明。连接方式见 [AutomationConnections.md](AutomationConnections.md)，通用契约见 [Automation.md](Automation.md)。

## 推荐调用顺序

1. 连接明确的应用实例，搜索当前任务需要的操作；有场景文档的宿主查询 `scene.info`，ModelViewer 查询 `view.get`。
2. 场景使用返回的 document/revision；资产打开后使用 document/generation。64 位数值按十进制字符串传递。
3. 先查询目标和当前值，再提交修改；批量修改保持一个请求中的全部目标合法。不要自动重试 stale revision 的修改。
4. 异步返回 job 时在同一连接轮询 `jobs.get`。编辑完成、资源准备、GPU 展示和磁盘保存是不同完成点。
5. 调用 `scene.save` / `asset.save` 才持久化草稿。关闭/切换脏内容需要先保存或明确 discard。

“保存全部资产”由分页查询 `asset.documents.list`、逐个调用 `asset.save` 并等待完成组成；“关闭全部”同样逐个使用 `asset.close`。保留每份文档的冲突和失败结果，不承诺跨文件原子事务。Editor 删除后清空选择；Viewer 保留原有的删除后选择第一个剩余模型策略。两个宿主各自的 GUI/agent 使用同一策略。

## 任务映射

| 人类任务 | 操作族 | 共享服务与范围 |
|---|---|---|
| 查找资产、场景 | `content.assets.list` | Content/Assets 原生索引；query/type、offset/limit、root generation。写入内容后重新开始分页 |
| 选择、清空资产目录 | `content.root.get/set/clear` | `FContentRootService`；Editor 与 GUI 同样检查 busy/dirty、关闭旧内容并保存 Preferences。Viewer 仅 get |
| 打开、新建、关闭场景 | `scene.open`、`scene.status` | `ISceneDocumentHost`；Editor 空 path 表示空文档，旧场景 ID 失效。失败不声称恢复旧场景 |
| 查询与编辑层级 | `scene.nodes.list`、`scene.node.get/create/reparent`、`scene.nodes.set_metadata/set_transform` | `FSceneEditDocument`；metadata/transform 最多 128 个目标，事务先全量校验 |
| 多选、删除、历史 | `scene.selection.get/set/delete`、`scene.undo/redo/save` | 显式有序选择；末项为 primary；Editor 删除后不自动选择替代物；Viewer 选择剩余模型。Editor undo 恢复并重新选择新 handle |
| Viewer 结构操作 | `scene.selection.duplicate/remove_keep_children` | 复用 Viewer 原有文档操作；Editor 没有对应的 history 实现，目录标为 unavailable |
| 放置 primitive/light 或 Viewer 已加载模型 | `scene.placement.list/place` | `IScenePlacement`；Editor 原有 registry 和准备/提交路径，Viewer 原有 Add Model 资源集。显式 position 是世界坐标 |
| Inspector 组件 | `scene.components.list`、`scene.component_types.list`、`scene.components.edit_structure`、`scene.component.<type>.get/set/set_batch` | `SceneEditing` 与组件反射；组件实例 ID 来自 list，不必等于类型 ID。保留资源绑定，完整候选经文档/场景校验；资源模型组件必须通过 prepared placement 创建 |
| 主相机、主灯、初始视图 | `scene.settings.get/set` | 文档设置；get 后保留不修改的字段，set 是完整替换 |
| 浏览相机和临时视口选项 | `view.get/set/frame_scene/preview_camera` | `ISceneViewport` / SceneCameraController；null option 表示此 host 不支持。patch 中 null 保留原值 |
| 用浏览视角编写相机 | `view.save_initial/create_camera/apply_to_camera` | Editor 与 GUI 同一文档操作；与临时相机移动区分 |
| 非场景资产页签 | `asset.open/info/documents.list/activate/close/save/undo/redo/rename` | Editor 发布 `IAssetWorkspace`，GUI 和多个 agent 使用同一草稿、历史与 busy 状态；其他宿主可使用独立 CPU 文档 |
| 模型属性 | `model.nodes.get/set`、`model.material_slots.get/set`、`model.primitives.get/set`、`model.roots.get` | AssetEditing 共享校验；节点/primitive 身份、几何和拓扑固定；引用先加载校验再提交 |
| 材质参数 | `material.parameters.get`、`material.passes.get`、`material.values.get/set` | 共享可编辑性、类型/维数、PBR clamp 和引用图校验；不允许修改系统/声明参数 |
| 纹理 | `texture.dimension.get/format.get/encoding.get/sample`、`texture.set_encoding` | 编码修改与 GUI 同一重建算法。sample 返回 mip/face 信息与源像素 RGBA，不返回 bulk |
| 天空属性 | `sky.radiance.get/specular.get/brdf.get/irradiance.get/convention.get` | GUI 中这些产品为只读；天空重新生成使用 import；名称使用 asset.rename |
| 资产预览 | `asset.preview.get/set` | `IAssetPreviewWorkspace`；模型/材质/天空 camera、曝光、形状、yaw；纹理 mip/face/channel/EV/zoom/pan/fit/checker。临时预览不增加资产 generation/history |
| Viewer 渲染与配置 | `application.settings.get/set/save`、`render.shadows.get/set` | 同一设置 inventory、校验和配置保存；标注 restart 的选项不热加载。阴影控制是临时状态 |
| 渲染结果和诊断 | `render.statistics`、`render.component_diagnostics`、`render.screenshot` | 完成帧统计、分页 primitive 诊断和现有 readback。PNG 成功返回时文件已写入，返回目标本地路径、尺寸、frame 和 bytes |
| RenderDoc | `renderdoc.status/capture/open/set_preference` | 复用 host capture/replay；Editor preference 与 GUI 同样持久化；未编译/缺 DLL/不可绘制等情况返回 unavailable |
| GUI scale / Profiling | `gui.scale.get/set`、`profiling.get/set` | 共用 GUI scale 和 Viewer profiling 控制。scale 下一 GUI frame 生效并沿正常偏好路径保存；profiling 取决于构建和 collector |
| 导入与发布 | `asset.import` | `FAssetImportService`、现有 importer 和 publication lease；完成后刷新索引，不静默覆盖已打开草稿 |

## 值与完成语义

资产序列字段 get 支持 offset/limit（1–100），返回 total/next；set 可使用 offset 替换现有范围，不隐式 resize。每页带当前 generation，修改后从第一页重新查询。字段类型来自成员指针和现有反射，不另写 JSON schema。

材质 `value.type` 描述 numeric 的 scalar/rows/columns、texture 维数或递归数组。`words` 保留现有 32 位存储表示：float 使用 IEEE-754 float32 的位模式，int/uint 使用对应 32 位整数；GUI 与 agent 都经过同一参数校验。数组使用 elements，纹理使用 texture 引用。先查询 parameters 的 source、overridePolicy、overrideScopes 与 semantic，再编辑 values；PBR normalized 值按 GUI 相同规则限制在 0–1。shader/pass/参数声明是查询接口。

场景组件 set 提交逻辑状态后，渲染资源可能仍在准备；使用 `scene.info` / `scene.status` / `render.statistics` 与 component diagnostics 查询。资源终止性失败可被后续编辑修正，不永久挡在 busy；加载中的工作仍遵循共享准入。加载失败和截图失败不会伪装为成功。

`render.screenshot` 支持 main，Editor 另支持 assets。必须显式指定 PNG 路径；覆盖需要 overwrite。窗口需要可绘制，不要求场景或 preview ready，因此可捕获加载及失败界面。捕获输出写在目标机器，不代表自动传输文件到客户端。RenderDoc 的 capture 等到完成的 capture 计数前进，open 才启动目标侧 replay。

导入支持 glTF/GLB 及其图像依赖、HDR/EXR 天空、已注册的 typed JSON/sky recipe/native upgrade；不新增独立 PNG/JPEG importer。sourceRoot/sourceId 成对使用，输出与源分离，force 仅跳过增量判断，不绕过写权限和身份检查。导入、发布、保存和截图接收后不支持取消；jobs 返回真实 cancellable 状态。保存后的草稿若继续编辑仍 dirty，导入不偷偷重载已有草稿。

Viewer 没有 GUI 场景切换/root 编辑或 Editor 相机编写/资产预览窗口时，对应操作不存在或明确 unavailable。启动 `--no-instance-batching` 时不能从 agent 重新启用 batching。未加载 scene provider、未编译 RenderDoc/Profiling、关闭 GUI 等可选配置不会让其他分支假成功。

## 失败状态、批量修改与退出

- `asset.documents.list` / `asset.info` 包含 loading/failed 页签的稳定 ID、state/error；未构造草稿的 generation 为 `"0"`。可 activate 或 close 后重新 open，不必清空整个 workspace。
- `scene.component.<type>.set_batch` 的 handles/components/values 三个数组一一对应，长度为 1–128。读取各对象后分别保留未修改字段，完整批次验证后一次提交和 Undo；旧 set 的广播语义保持不变。
- ModelViewer 的 `view.set/frame_scene` 使用 `document=""`、`revision="0"`；不支持 scene-camera authoring。`light.main.get/set` 与 Viewer 主光源面板共用校验和原子提交，使用返回的 handle/revision；该模式的相机和灯光为临时状态。
- `render.statistics.sceneError` 优先报告当前 scene producer 的终止错误，否则报告场景准备错误。
- `application.close.status/request` 由 Editor/Viewer 发布正常关闭服务。action 为 0（默认：拒绝未保存内容）、1（保存后退出）、2（明确丢弃后退出）、3（取消退出，不取消已接收保存）。Editor 未命名脏场景保存退出须提供 scenePath。
- close.request 返回的是接受状态，不是进程已经退出；保存期间状态为 saving，失败为 failed 并保留应用。成功后目标正常排空、撤回发现记录并断开连接。关闭时最多排空应答 2 秒；断连/强杀不保证远端收到应答，不应自动重试破坏性请求。
- 实际帧间隔/FPS、CPU hooked allocation、线程任务计数等 GUI 性能指标尚未全部进入结构化 diagnostics，记录为后续只读指标适配范围。

## 新功能维护方式

- CPU 事务和校验放 SceneEditing / AssetEditing；Renderer 提供渲染、相机、capture/readiness 的 engine-owned 接口；宿主发布当前实例的 typed service。不要为 agent 新建第二份 Editor 文档。
- 新属性优先复用反射成员；新组件注册类型与 typed operation。临时状态、保存状态和 generation/revision 的语义必须写进说明。
- GUI 的候选值和 agent 的候选值最终调用同一领域方法。Inspector 提示不是授权依据；不可变身份由业务规则保证。
- DTO 不直接复用只用于 Inspector 的 transient-only record；API 投影须显式声明可序列化字段，验证真实返回值。
- 可选服务用 Find，不能在只有 Optional 声明时 Require。注册失败必须撤回 owner；已接收 job 在 provider 销毁前排空。
- 验证 discovery/describe/call、正常和非法输入、跨连接可见性、history/save/reopen、失效/缺 provider/退出。`automation_capability_parity` 是真实 CLI/MCP 验收，GUI 回归独立覆盖鼠标路径。

保留现有 transport、catalog、session、平台通信及插件架构；连接关闭时新增最多 2 秒的已排队应答排空，避免正常退出丢失接受结果。窗口停靠/大小/按键属于呈现，不通过模拟输入暴露；已有 AssetTool 的库迁移、Engine 内容生成、测量和 envelope 导出仍是独立维护 CLI。未来 Public C++ 的新能力仍按领域任务接入，不能将本页理解为所有 C++ 方法的自动 RPC。
