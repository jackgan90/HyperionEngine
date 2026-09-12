## 1. Executor启动与基线固定

- [x] 1.1 [EXECUTOR，后续获准启动后执行] 阅读proposal、design D1-D12、全部delta specs、verification-plan以及AGENTS.md、docs/CodingStyle.md、docs/SourceLayout.md、docs/VisualStudio.md；记录接口约束和实际基线HEAD。当前planner只生成artifacts，不执行本清单。
- [x] 1.2 记录工作区已有改动并建立本change文件清单；用rg枚举FScene/FSceneModel/FSceneManifest/FreezeFrame/SetSceneParameters/相机直接赋值调用点。HEAD偏离规划基线时核对差异，不reset或覆盖用户文件。
- [x] 1.3 按V0固定baseline二进制、依赖、shader、config与source/native内容及hash，保存Sponza/Showcase和ModelViewer原相机/光照/CSM截图与计数；不能只复制会读取新shader/content的旧exe。
- [x] 1.4 运行足以建立当前行为的scene/material/frame/shadow/deferred相关基线检查并保存原始结果；按V3采集实现前性能。已有失败单独记录原因，不通过删断言或改阈值处理。

## 2. CPU节点和基础编辑（依赖1）

- [x] 2.1 在Scene增加D1的Group/Model/Camera/DirectionalLight/EnvironmentLight类型及互斥payload；通用Id/Name/Parent/Local/bEnabled与model-specific数据分离，FSceneModel仅作Renderer transfer value而非第二份权威world。
- [x] 2.2 扩展FScene slot为统一node存储，保留scene/slot/generation校验；实现stable ID索引和空Id生成、typed const getters及all/kind/root/children确定性枚举，无mutable指针外泄。（N01）
- [x] 2.3 实现类型化Name/Enabled/ModelVisible/model-content/camera/light编辑，非法kind/handle拒绝、相同值no-op；修改前完成值验证，失败不改变revision或pending changes。（N01、N04）
- [x] 2.4 实现FSceneSettings的三个typed optional选择、校验及删除引用清理；没有选择合法，不能自动选第一个节点。保留stable ID与runtime Handle不同用途。（N04、C03、L02）
- [x] 2.5 扩展FSceneChange为typed node/tombstone和change mask；保持按完整Handle归并、monotonic revision、ack admission和单同步消费者语义；重attach完整发布现存节点。（N01）

## 3. 层级传播与CPU相机/光源（依赖2）

- [x] 3.1 实现parent/children/roots索引、迭代cycle验证与World=ParentWorld*Local、effective enabled传播；任意序nodes可建树，4096层无递归溢出，只访问受影响后代。（N02、N03）
- [x] 3.2 实现SetLocalTransform和SetWorldTransform、候选子树world/pose预检及原子提交；world编辑必须检查父逆矩阵，非法编辑保留全部旧值。（N03、N04）
- [x] 3.3 实现显式KeepLocal/KeepWorld Reparent；禁止self/descendant/foreign/stale parent；KeepWorld保留world并计算new local，不对模型完整矩阵强制TRS分解。（N03、N04）
- [x] 3.4 实现RemoveSubtree与RemoveNodeKeepChildren：后者预验证全部子节点world-preserving reparent，再提交；两者产生所有tombstone并清settings引用。（N03、N04）
- [x] 3.5 实现D3统一camera pose提取（-Z forward、+Y up、orthonormalization）、lens/focus验证和有效版本；用独立数值oracle测试镜像/非均匀/退化case，不引用RHI。（C01）
- [x] 3.6 实现D4灯光验证、surface-to-light=-Forward转换所需CPU pose、Color*Intensity有限校验、显式选择和零贡献规则；环境光不受姿态影响。（L01、L02）
- [x] 3.7 实现显式默认内容helper，使用既有SceneViewer镜头/默认radiance，空FScene不注入任何节点；ModelViewer可显式传自己的lens参数，不混用FOV。（C01、L01）
- [x] 3.8 运行N01-N04、C01、L01-L02的CPU tests并通过Scene独立依赖/格式检查，整理新API供后续桥接和加载使用；此阶段不引入GUI或GPU节点对象。

## 4. v4记录、source v2及迁移（依赖3）

- [x] 4.1 按D8增加node/camera/light/model-persistence records；root hyperion.scene升v4，Nodes和stable-ID选择取代顶层Instances/Eye/Target；保留旧instance descriptor供nested迁移。（P01）
- [x] 4.2 实现native root3→4迁移，并保留v1→2→3路径；nested legacy instance先ReadValue执行其migrations，完整保留material/surface/sectionSurfaces和affine矩阵。（P01）
- [x] 4.3 将旧Eye/Target/Near/Far转换为camera transform/focus/FOV及真实默认lights；新增ID按D8确定后缀避碰，删除旧权威字段，拒绝新旧冲突数据。（P01）
- [x] 4.4 增加全manifest预验证：ID、parent/selection类型、cycle、payload互斥、镜头/灯光/变换合法性、任意node顺序；合法v4空场景不补默认。（P02）
- [x] 4.5 在AssetImport私有adapter解码plain v2 nodes、matrix或TRS互斥、camera/light payload和stable-ID选择；plain v1调用共享legacy转换，reflected source沿用反射路径。非法/未支持payload显式拒绝。（P02）
- [x] 4.6 提升scene-json importer revision并验证native upgrade/cache fingerprint；更新AssetPublication.cpp model→scene包装为真实v4 model/camera/light，保持旧默认构图；检查AssetTool inspect instances=仍只数模型。（P05）
- [x] 4.7 验证optional node.model中的FAssetRef依赖遍历、material/texture引用、原有pin/revision；新增/迁移PublicationTests、SharedAssetPublicationTests和v1/v2/v3/v4 golden fixtures，不删旧兼容tests。（P01、P02、P05）

## 5. FSceneInstance节点生命周期与统一保存（依赖4）

- [x] 5.1 扩展FSceneInstance typed node/transform/settings入口和诊断枚举；stable ID归FScene；GetModels兼容视图和pending-load association不成为另一份可写身份/world。（N01、P03）
- [x] 5.2 重写BeginManifest：验证后一次安装全部nodes，再解parent/settings，再开始模型/材质异步请求；相机/光源不等待模型ready。保留Load先Close的既有错误语义。（P02、A01）
- [x] 5.3 将model Data/material完成附加限定为完整Handle+load epoch且不覆盖场景编辑；删除子树清理全部model pending maps，显式替换Data继续脱离旧asset association。（A01）
- [x] 5.4 扩展结构化状态，保留Models/ReadyModels/FailedModels model-only含义，增加node kind counts/scene receipt错误；NoActiveCamera与加载/模型失败分开。（C03、A01）
- [x] 5.5 改Snapshot仅从当前nodes/settings生成v4记录，保存完整Local与所有payload、stable IDs和选择；无需caller补camera/light，也不等待Render/GPU。（P03、A02）
- [x] 5.6 沿用SaveAs asset/material/texture引用rebasing和不可持久化状态拒绝，空assets的camera/light/group场景可保存；保存快照独立于后续编辑。（P03、P04、A02）
- [x] 5.7 更新插件无关SceneInstance tests，覆盖typed编辑/parent/多camera候选/多light候选/save/reload/pending移除；记录P03-P04、A01-A02证据。

## 6. Bridge与Render场景原子发布（依赖3和5）

- [x] 6.1 增加FScenePublicationToken及attachment epoch；attach/reload/reattach分配不同epoch，完整identity/epoch/serial/revision用于凭据，不复用几何cache key。（F02）
- [x] 6.2 扩展Bridge PrepareChanges以按change mask分类；仅受影响model构造FSceneModel transfer/frozen material，camera/light/settings准备独立immutable metadata；不扫描全部model。（M02、B02）
- [x] 6.3 扩展统一Render publication：一次admission携带primitive new/update/remove和metadata，即使只有camera/light也有任务；所有内存/验证在admission前尽量准备，无Main可变指针跨线程。（F01、F04）
- [x] 6.4 在单一Render任务中应用primitive变更和metadata，成功后安装AppliedToken；保留camera/light表和geometry revision独立，不对metadata-only调用几何OnChanged。（M02、B02）
- [x] 6.5 Bridge公开latest admitted token及scene-wide receipt，成功admission才ack；每次attach首次Flush即使空scene也发布serial=1，loading Tick不能提前跳过；以后无变化复用token，pure metadata失败也能Main观察；Main验证/dispatch失败保留重试增量。（F04）
- [x] 6.6 处理Render publication异常为可观察scene失败，禁止半更新scene继续Build；cleanup/Close可执行，不依赖失败prerequisite成功body，保留既有per-model pending/failure隔离。（F04、A01）
- [x] 6.7 Close发布移除及空metadata，join已admit CPU工作，detach epoch；无presentation同样进展，旧prepared Graph保留原leases；补F05和旧frame清理tests。（F03、F05）

## 7. 场景frame seed、View解析和材质输入（依赖6）

- [x] 7.1 增加FSceneViewRequest：Camera Handle、View identity、目标/viewport/depth/culling/batching配置，不接收业务可写Eye/FOV/VP；与最终FRenderView区分。（C02）
- [x] 7.2 实现Main FreezeSceneFrame(token/time/custom values) seed，冻结providers与Global/Frame/custom Scene输入；与已解析FMaterialFrameContext区别，保留unbound FreezeFrame兼容分支。（M01、F01）
- [x] 7.3 实现Render ResolveSceneFrame，验证AppliedToken精确相等及epoch、失败状态；mismatch在生成scene draw前报错，不能取新state补旧token。（F01、F02）
- [x] 7.4 从published camera按D3解析选择/fallback/NoActiveCamera、viewport aspect、Eye/VP/FRenderCamera；同一帧多View用不同identity并共享scene输入。（C02、C03）
- [x] 7.5 从selected lights生成三个既有Scene语义，保持unit direction/zero radiance规则与cast标志，创建immutable final frame；所有RHI延迟工作只捕获resolved值。（L01-L03、F03）
- [x] 7.6 实现五项受保护语义的bound验证：provider冲突、SetSceneParameters batch原子拒绝、Global/Frame/View外部注入拒绝；剥离绑定前session默认灯；保留custom semantics/material override/unbound tests。（M01）
- [x] 7.7 将Scene scope cache key与有效lighting/custom values关联，将View key与camera/pose/lens/viewport关联；publication provenance不导致unrelated Object/Material/resource失效；标准uniform ABI不变。（M02）
- [x] 7.8 明确旧高层explicit scene入口不能绕过seed/token；内部CSM BuildViews接受resolved final frame，未绑定logical scene的explicit primitive/view/provider fixtures保持可用。（M01、F02）

## 8. Forward/Deferred/CSM和BVH接入（依赖7）

- [x] 8.1 给FSceneRenderPipeline和FForwardRenderPipeline接入共同scene解析入口，在shadow setup/visibility/sort前完成camera/light解析；复用现有pipeline build-body，不复制三套取灯逻辑。（R01）
- [x] 8.2 Forward标准material block、Deferred Lighting全屏参数、CSM方向引用同一final frame，surface-to-light符号不反；保留标准Z/ReversedZ和view相关cache key。（R01、R02）
- [x] 8.3 组合pipeline shadow enable与light enabled/nonzero/cast；无灯或cast关闭时关active cascades、绑定neutral、清preview有效状态；direct/ambient/emissive独立。（L02、L03、R02）
- [x] 8.4 NoActiveCamera走clear+extension/GUI输出与正常CPU completion，不能将上帧scene target当新画面；scene恢复后重新解析，不因旧错误永久停更。（C03、F05）
- [x] 8.5 geometry BVH仅接受model groups，parent传播更新受影响bounds；camera/light/group无dummy primitives/unbounded groups；保留独立shadow caster query、透明排序和None/Linear/BVH规则。（B01、R02）
- [x] 8.6 校对SceneCollection/SessionViewCache/CSM key及geometry OnChanged，camera/light-only不增geometry revision、不全量InvalidatePreparedViews，metadata编辑不打包GPU数据。（M02、B02）
- [x] 8.7 增加真实gated F01-F05 tests与R01-R02像素tests，保留旧frame constants/pages/资源所有权及无新GPU wait；记录每种失败路径的真实结果。

## 9. 交互层迁移（依赖8）

- [x] 9.1 SceneViewer移除权威Target/Distance/Yaw/Pitch和固定lens生成路径，改为读取/编辑scene camera；保留transient mouse/gesture，按D3实现orbit/dolly/pan/fit，外部编辑后不回写旧值。（C04）
- [x] 9.2 ModelViewer创建model/camera/default light节点并选择，迁移input/fit到scene API，保留原FOV 1和near/far公式、加载/error、plugin/config/CLI行为。（U02）
- [x] 9.3 两插件向Application暴露窄场景编辑/selection/token接口；Application不通过raw mutable Scene绕过pending bookkeeping，不再持权威ShadowLight。（U02）
- [x] 9.4 将ViewerShadows GUI临时角度从scene方向读取，只在用户操作时写node；color/intensity/cast改scene，resolution/distance/bias/preview仍是pipeline quality。（L03、U01）
- [x] 9.5 `--shadow-light`在nodes ready后执行一次scene edit，缺失selected节点时显式创建并选中；benchmark light每tick改同节点，benchmark camera仍通过同输入路径；不再每帧SetSceneParameters。（U02）
- [x] 9.6 调整Viewer tick为全部业务编辑/完成加载→Scene.Update/Flush→FreezeSceneFrame→立即FramePipeline.Submit，等待发生在捕获token前或dispatch后；Frame input捕获owned request/seed。（F01、F05）
- [x] 9.7 SceneViewer SaveAsync直接Snapshot/Assets.SaveAsync，删除Eye/Target/Near/Far补写，保留可见保存状态/CLI SaveScene及响应性。（A02、U02）
- [x] 9.8 迁移冻结剔除、bounds overlay和diagnostics：frozen VP是View调试副本，不改scene camera；透明排序/CSM用当前camera，结果携带原frame token。（C02、B01）

## 10. 可用场景树与typed属性面板（依赖9）

- [x] 10.1 GUI树列出全部node kind/parent，选择改为generation-safe Handle；保留独立model/primitive/item/draw统计，删除选中节点后无数组越界。（U01）
- [x] 10.2 增加Group/Camera/DirectionalLight/EnvironmentLight创建和单节点复制；复制取新stable ID，model共享data/material规则不变，不暗中复制整个子树。（U01）
- [x] 10.3 增加parent选择、显式KeepLocal/KeepWorld重挂接、RemoveSubtree/RemoveNodeKeepChildren区分按钮；失败显示真实原因，不吞掉非法inverse/cycle。（N03、U01）
- [x] 10.4 属性面板修改local transform、enabled、model visibility、camera FOV/Near/Far/FocusDistance和light Color/Intensity/CastShadows；同步三个scene default/main选择，明确只选中主灯参与渲染。（U01）
- [x] 10.5 全部控件使用FGui wrappers；需要的通用tree/input接口只在Gui私有adapter接触ImGui；清空model后仍可编辑/保存camera/light并重新添加model。（U01）

## 11. 样例、全部消费者和回归收口（依赖10）

- [x] 11.1 将assets/Scenes的Sponza/Showcase/Shadows/SharedAssets样例迁到明确v2节点，保留原model/local材质/相机FOV/默认灯，不改变构图/曝光；重导入真实native content并核对v4。（P02、U02）
- [x] 11.2 迁移所有FSceneManifest旧Instances/Eye字段消费者及模型专用Scene调用；包括AssetTool、publication tests、SceneInstance/Viewer/shared-asset tests，不能仅让Viewer编译通过。（P05）
- [x] 11.3 更新读取shipped JSON的FramePipelineAcceptance/ScenePerformance/BatchPlannerComparison/MeasureShadows按model payload计数，显式匹配--scene与fixture；保留生成v1的兼容test脚本和原coverage/像素断言。（P02、B01）
- [ ] 11.4 完成verification-plan V1全部N/C/L/P/A/F/M/B/R/U项，测试映射写入implementation.md，未运行项如实标出；只修本change引入或直接阻塞的相关问题。
- [x] 11.5 运行相关target后完成Debug和Release全CTest、style、naming、boundaries和OpenSpec strict/diff checks；日志记录实际测试总数、退出码、配置与失败，不预填“通过”。
- [x] 11.6 按V3完成static/small-camera/large-camera/moving-light的Debug/Release和Forward/Deferred交错A/B，报告所有样本和退化；确认camera/light-only零geometry rebuild/refit及unchanged local reuse。
- [x] 11.7 完成至少3000帧持续更新与old-frame/gated资源检查；资源有界、PSO/descriptor稳定、无profiling-only GPU idle waits或validation降级。（F03、B02、V3）
- [x] 11.8 更新docs/SceneManagement.md、SourceLayout.md及相关camera/material/asset/CLI说明，说明v4/source v2、单选灯限制、无camera/light行为、保存和交互；不回写archive历史。

## 12. Executor交回审查并停止（依赖11）

- [x] 12.1 自查是否残留应用/插件权威camera/light、每帧builtin SetSceneParameters、双重model world、全scene cache invalidation、legacy默认灯复活；用源码和定向tests验证，不能只用rg零命中判断正确。
- [x] 12.2 生成verification-plan V4要求的review-handoff.md，列明实际HEAD/全部改动文件、D1-D12实现映射、V1各ID证据、测试/图像/raw performance、限制与待解决项；不stage、不commit、不archive。
- [x] 12.3 将完成情况与handoff交给当前planner/reviewer，停止executor工作。第13组只能由planner/reviewer操作；executor不得为了让OpenSpec显示全完成自行勾选或执行reviewer任务。

## 13. Planner / Reviewer专属交接门（不是executor任务）

- [x] 13.1 [PLANNER/REVIEWER ONLY] 接收executor固定代码/证据快照，审查双重状态、层级/迁移、帧一致性、失效范围、资源生命周期、真实GPU验证和scope遵循，记录有源码依据的findings。
- [x] 13.2 [PLANNER/REVIEWER ONLY] 如有findings，交executor实施修复后复核；未验证或未解决项保持未完成，不由planner直接实施plan。
- [x] 13.3 [PLANNER/REVIEWER ONLY] 给出最终review结论与未完成事项，向用户报告并等待后续归档/commit指令；本proposal生成回合到artifacts验证结束即停止。

## 实现交接状态（2026-09-12）

第 1–12 组的代码和本轮证据收集已交回，独立 review 按用户要求暂不执行，不 stage/commit/archive。11.4 保持未完成：dispatch 分配失败注入和逐控件原生 GUI 鼠标回放未执行；已有 Main validation retry、Render publication failure、IO failure、typed controls 和实际 GUI 截图证据。

最终 Debug/Release 各 62/62 通过，style/naming/boundaries/OpenSpec strict/diff checks 通过。完整交错 A/B 与 3000 帧、旧帧 lifetime 检查已完成；8 个性能格触发审查线、8/10 跨版本截图最大差异超限、早期 Debug 阴影回收间歇失败原因未确认。3000 帧 GPU 资源和 baseline 逐帧一致，但未额外证明更长运行的平台期。完整数据、限制和精确文件快照见 review-handoff.md；勾选实现任务不代表这些验收限制已经通过 reviewer 审查。

## 开发质量审计状态（2026-09-12）

用户已授权当前实现的独立审计及局部修复。无上下文继承的 reviewer 初审确认 3 个 P2；主 agent 逐项独立复核、修复并补充回归，原 reviewer 复审全部关闭。审计后 Debug/Release 相关回归各 19/19，reviewer 独立 CPU 2/2，style/naming/boundaries/OpenSpec strict/diff checks 通过。第 13 组审查门完成；11.4 以及用户暂缓的五项 follow-up 保持未完成，不将其视为验收通过。另记录基线已有的加载中复制材质问题，不扩展本轮修复。完整范围、逐项处置及固定快照见 audit-report.md。未 stage/commit/archive。
