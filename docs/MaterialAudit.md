# 材质系统实现审计

审计日期：2026-09-07。基线 `1847be673f72f9877e62fb2d80556e0f8a10ccc9`，对象是 `add-extensible-material-system` 的未提交实现及直接调用链。主 agent 复核并修复了 **10 项确认缺陷：1 项 P1、9 项 P2**，全部经原 reviewer 定向复审关闭，限定范围内没有待处理的确认缺陷。材质概念拆分总体合理；本次修复没有要求重设模块职责或重写资源生命周期。

## 审查方法与版本

两个未继承实现会话的独立 reviewer 分工检查 Materials/Renderer 与 Shaders/RHI/D3D12；主 agent 另外核对 Model、Scene bridge、插件迁移、批次发布及复现证据。首轮覆盖 157 个变更/新增路径，连同直接依赖固定了 294 个文件的 SHA256。审计修复涉及其中 32 个文件；新增审计文档不属于可执行代码。没有 stage、commit 或 OpenSpec archive。

复审期间冻结共享源码。首轮 9 项修复快照为 `out/MaterialAudit/ReviewFixSnapshot.json`；补充预编译程序身份修复后的最终快照为 `ReviewFinalSnapshot.json`，SHA256 为 `97098afcd7fd4e6d9a6a413864e8d3020716de4e0fac6e3d7fb43a913056150f`。第二次仅改变 MaterialResourceService、RenderResourcesInternal、MaterialResourceTests 和 Materials 文档，未改变 B reviewer 已关闭的底层修复。

原始报告和复审依据：

- [Materials / Renderer 首轮](../out/MaterialAudit/ReviewerMaterials.md)、[五项修复及补充 finding 复审](../out/MaterialAudit/ReviewerMaterialsRecheck.md)、[程序身份修复复审](../out/MaterialAudit/ReviewerProgramSelectionRecheck.md)。
- [Shaders / RHI 首轮](../out/MaterialAudit/ReviewerRhiShaders.md)、[三项底层修复复审](../out/MaterialAudit/ReviewerRhiShadersRecheck.md)。
- Reviewer 独立编译/执行 CPU probe；GPU、Viewer、故障与整仓验证由主 agent 执行。报告中的来源分别标注，不能把实施方 GPU 日志称作 reviewer 独立 GPU 复现。

## 1. 原 Material 概念的拆分

| 层次 | 责任与审查结论 |
| --- | --- |
| Assets / `FModelMaterial` | 保留 glTF/PBR 输入数据。固定五纹理角色、UV、mip、sRGB 和 alpha mode 属于模型适配器，未成为通用材质的资源数量限制。 |
| Materials / definition、instance、snapshot | 定义 shader/pass/参数/表面状态；实例保存独立参数修订，快照不可变。公共契约不依赖 RHI 或 vendor。schema、semantic、默认值与查询在 CPU 可用。 |
| Renderer / compiled interface | 编译反射、逻辑槽映射、目标 ABI 打包及能力适配。声明的 A2C/Shadow pass 与给定目标下可执行的结果分开，没有复制容易失配的全局标志。 |
| `FRenderMaterial` | 线程中立的冻结材质选择租约，普通数值修订可共享静态资源。显式编译程序身份现在参与 program/material record 选择，避免误合并。 |
| Geometry / primitive / view / RHI | Geometry 持有顶点布局和 topology；primitive 持有坐标与 bounds 契约；view/graph 提供目标、viewport 和初始化。最终 PSO 合成这些输入与材质状态，packet 再携带资源、常量 slice 和动态状态。 |

Scene 保持独立 CPU 数据模块，renderer bridge 负责租约与发布。RHI/D3D12 不再识别五张模型纹理、固定模型常量布局或 `bMaterialLayout`。几何默认 section 拥有材质选择是组合关系，不等于材质必须从属于模型。没有发现需要整体推翻该拆分的证据。

## 2. 确认缺陷、最小修复与回归证据

下列项目均在本次材质实现及其直接迁移范围内。原始行号/触发链见 reviewer 报告；这里链接修复后的文件，避免将旧行号误认为现状。

| ID / 原优先级 | 已确认的触发与影响 | 修复及验证 |
| --- | --- | --- |
| A-MAT-001 / P1 | 静态 Object token 不变、相机改变 WVP；默认 Object provider 比较整个输入集合，持续追加活跃历史，内存与每次求值/Collect 成本增长。 | [MaterialProviders.cpp](../Source/Runtime/Renderer/Private/MaterialProviders.cpp) 仅为内置 provider 保存实际 semantic 输入；相同完整 scope key 替换当前记录，自定义 provider 保留其完整声明输入。2048 次相机变化始终 3 entries/3 evaluations；旧资源引用及时释放。 |
| A-MAT-002 / P2 | provider 起初缺值、回退默认值时丢失依赖；之后 Global/Scene 提供值，旧解析结果继续命中。 | absent value 仍携带完整 dependencies，fallback 加上 Material 依赖；覆盖后继续裁剪有效依赖。真实 session 像素验证默认蓝色 .15 → Scene 值 .35 → 默认 .15。 |
| A-MAT-003 / P2 | 作者 `Color -> Shared.Tint` 已绑定反射路径，按 `Pixel:Shared.Tint` 查找却失败。 | [MaterialPreparation.cpp](../Source/Runtime/Renderer/Private/MaterialPreparation.cpp) 在全部 stage/variant 匹配完毕后合并实际目标 aliases；逻辑名及 VS/PS 完整路径命中同槽，真实短名歧义仍拒绝。 |
| A-MAT-004 / P2 | 一个 GPU 缓存对象共享给 U 个不同 owner groups，每次命中扫描全部 U；U 次 draw 命中产生平方成本。 | [MaterialGpuLifetime.cpp](../Source/Runtime/Renderer/Private/MaterialGpuLifetime.cpp) 按完整 shared ownership identity 建有序索引；命中 O(G log U)，清理集中在 Collect。两个共享组分别释放的测试保留组内 AND、组间 OR 和 native 最后引用条件。 |
| A-MAT-005 / P2 | typed float/vector/matrix helper 把 `-0.0f` 改为正零，改变 shader 可观察位模式。 | 直接保存有限 float 原始 bits。CPU pack 输出 `0x80000000`；DXIL 冷/热 GPU shader 用 `asuint` 检查并以 alpha 像素断言，不只检查 pack 次数。 |
| A-MAT-006 / P2 | 同一定义显式准备两个 `Default` 宏变体，第二份资源返回第一份 shader 程序；definition/resource key 和 program key 都漏掉 Compiled 选择。 | [MaterialResourceService.cpp](../Source/Runtime/Renderer/Private/MaterialResourceService.cpp) 先按 definition + immutable Compiled identity 选择 program，再按 program + resource signature 共享 record；auto 路径独立。真实 compiler/service probe 从 `second_uses_B=0` 变为 1。测试覆盖两份程序、数值修改、auto、异目标拒绝和无帧退休。 |
| MAT-B-001 / P2 | `Texture2D<int4/uint4>` 被当成可与当前 RGBA8 浮点视图绑定。 | 反射保存 sampled scalar，Materials 和真实 D3D12 pipeline 准备均拒绝 integer texture。三目标冷/热 metadata、正反例和 Device.CreatePipeline 拒绝测试通过。 |
| MAT-B-002 / P2 | DXIL 多维数组展平导致合法作者嵌套 schema 无法匹配；SPIR-V/MSL 多层递归丢失 bool，暴露为 uint。 | 作者 schema 在叶类型/总数匹配时按真实 DXIL leaf stride 重建地址；SPIR-V 在实际数组递归中保留逻辑 bool。GPU 二维 float/bool/struct/matrix oracle，以及 reviewer 独立三维数组/嵌套结构全部字节与 padding oracle 通过。下文的原生维度及 SPIR-V matrix 限制仍保留。 |
| MAT-B-U01 / P2 | structured buffer shader stride 原生为 16/32，但引擎 metadata 相同；合法范围 view 可带不匹配 stride。 | 将真实目标 stride 贯穿反射、stage 合并、RHI slot、layout hash/equality、binding set 和 pipeline 校验。正确 stride 接受，0/4/32 错误要求拒绝。独立非平凡 struct 验证 DXIL stride=24、SPIR-V/MSL=32，未混用目标 ABI。 |
| MAIN-PUB-001 / P2 | `PublishGroups` 移除 A 后 B 复用槽位；旧 A binding 再 RemoveBatch 错误释放 B 的 Main admission 槽，B 更新丢弃、后续创建撞槽。 | [SceneRemoval.cpp](../Source/Runtime/Renderer/Private/SceneRemoval.cpp) 在清 Active 前检查 scene/slot/generation。primitive 回归验证 B 的 revision 更新到 2，后续新对象使用不同槽。 |

structured stride 的原生依据是微软 [D3D12_BUFFER_SRV](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_buffer_srv) 与 [D3D12_SHADER_INPUT_BIND_DESC](https://learn.microsoft.com/en-us/windows/win32/api/d3d12shader/ns-d3d12shader-d3d12_shader_input_bind_desc)。主 agent 和 reviewer 均以原始 DXIL 容器 probe 核对元数据，而非仅据文档推断。

## 3. 性能结果

以下前后数字由同机 MSVC `/O2` 独立 CPU probe 得到，见 [修复前](../out/MaterialAudit/MainMaterialsRepro.log)、[修复后](../out/MaterialAudit/MainAfter/After.log)。测试直接使用相应版本的 provider/ownership 实现；修复后的程序身份改动未改变这些被测函数。单次计时存在调度噪声，不能推导相同倍数的 Viewer FPS 提升。

| 测量 | 修复前 | 修复后 |
| --- | --- | --- |
| 2048 次相机变化、3 个不变 Object provider 的总求值 | 6144 次，0 reuse | 3 次，6141 reuse |
| 连续四批、每批 512 次的 provider 时间 | 25.12 / 76.41 / 128.49 / 178.20 ms | 1.53 / 1.39 / 1.37 / 1.38 ms |
| 外部释放后仍被无关历史输入保留的 buffer source | 64/64 | 0/64 |
| 512 / 1024 / 2048 / 4096 个共享 owner、五轮纯命中 | 1.24 / 4.92 / 20.92 / 79.60 ms | .065 / .159 / .476 / 1.101 ms |

实际 SceneViewer 仍使用原来的 514 model / 2053 draw / 180 frame、无剔除 fixture 和 45 秒 subprocess 超时；没有增加 timeout 或减少对象数量。最终日志从应用启动到成功退出约为 Release **3.45 秒**、Debug **18.36 秒**，均 514/514 Ready、180 GPU frames、0 validation errors。见 [Release](../out/MaterialAudit/FinalReleaseScene.log)、[Debug](../out/MaterialAudit/FinalDebugScene.log)。这是整个 fixture 运行时间，包含加载及截图，不能当作单帧 GPU 时间。

已复现的两项明显热路径缺陷已消除。以下仍是可选优化而非本轮已测得的帧时回归：constant publication 每次 `reserve(size+1)`、每个 CBV 页内线性验证；immutable IB 的前 32 个 range 之后没有替换，第 33 种范围持续 miss；以及 warm miss 的值复制/部分线性查找。当前普通模型路径没有独立证据证明这些剩余成本构成明显性能问题，本次未顺手重写它们。

## 4. 缓存上限与失效清单

“无硬数量上限”不等于“无失效条件”。下表区分活跃工作集、短期等待收集和真正的历史保留；不能把当前实现描述成全局内存预算系统。

| 缓存 / 持久记录 | 上限或失效条件 | 活跃增长及保留边界 |
| --- | --- | --- |
| schema 名称/alias/semantic lookup | immutable，随 definition/interface 最后引用释放 | 由声明和全部目标映射数决定，不积累 Set 历史。 |
| 标准 semantic 与 PBR definition | 标准表固定；PBR 合法 alpha mode × 双面共 6 个弱缓存键 | PBR 弱值不保活 definition。自定义 frozen registry 的大小由用户声明决定。 |
| model preparation 的 image/program map | 每次 preparation 的局部 map，返回后释放 | image key 包含图像/颜色空间；program 按该次定义复用，结果由资源描述拥有。 |
| instance snapshot | 实例只持有当前一个，旧版本由外部使用者拥有 | 外部有意保留历史会增长；不存在实例内部永久历史表。 |
| Global/Scene/Frame、Views、Families | 输入更新替换 token；Frame 随在途/外部引用；Views 在成功 family 后删未使用项；Families 换 frame 清空 | 当前 views/families 没有固定数量预算，Close 释放；异常 build 留存可由后续成功 build/Close 清理。 |
| provider 求值 | 每 semantic + 完整 scope key 一份当前记录；任一 owner 过期后 Collect 删除 | 已消除同 key 历史增长；不同活跃依赖组合仍增长。暴露 CachedEntries；停止 Render 求值时 CPU cache 可留到下次 Collect/销毁。 |
| Object / resolved evaluation | **各最多 64 entries/primitive**；同 key 替换、primitive publication/remove 释放 | inactive item/view 可保留到有界缓存替换或 primitive 生命周期结束；Frame/Pass/Draw 不能跨边界复用。 |
| compiled program / material records | 任务完成/失败且没有 material record 用户后删 program；无 lease 的 material record 退休 | program key 区分 auto 与显式 Compiled identity，map 的额外强引用随 entry 删除释放。相同内容的不同 Compiled 对象保守地不合并；native 缓存仍可共享兼容内容。 |
| layout / PSO / texture / read-buffer / sampler / binding set | weak owner groups + native CPU/GPU 最后引用；consumers 先于 sources 清理 | 无全局条目预算，随活跃资源/在途工作增长；相同完整 owner 组重复命中不增长，过期组集中 Collect。 |
| prepared native draws | weak params/geometry/surface 过期时收集；相同 key 替换，Close 清空 | 强持 packet；规模间接受 primitive 有界解析缓存、活跃 sections 和在途引用约束，并非独立硬上限。 |
| constant blocks/slices | 完整 layout/mapping/value/scope equality；任一 scope owner 过期后删条目 | session 随内容变更退休 token。直接使用 cache 的调用方必须提供正确的 scope 生命周期并执行 Collect；长期保留同 token 并改变值可能保留历史。 |
| constant pages | 默认 **64 KiB/页**；sole payload ownership 才 reset/delete；最多 **1 个 idle reserve page** | active pages 无全局字节预算；活跃 slice 可能钉住页内空洞。transient/persistent 分开，GPU 未完成不能复用。 |
| native Published metadata | 每 buffer 最多 Size/256 条，默认页最多 256 条 | reset 清列表但不重用单调 publication token；vector 可保留容量到 buffer 释放。 |
| SRV source 与 visible table heaps | 默认各 **4096** 格，配置上限 1,000,000；固定容量不在 draw 时增长 | source/set 最后引用释放；连续范围不足明确失败，部分分配 RAII 回滚。 |
| sampler source 与 visible table heaps | 默认各 **512** 格，上限 **2048** | 同样固定容量与引用退役，失败不泄漏已分配区间。 |
| index range min/max | 每 immutable IB **32** 条，buffer 析构释放 | 当前只保留前 32 种，不替换；不允许为了性能跳过索引边界验证。 |
| upload/submitted batches | fence 完成或成功 Idle 后释放；Signal 失败的已提交批次保留到成功 drain | 无 device 总批次/字节硬上限；正常每 swapchain 2-frame ring、每帧最多 16 recording contexts 提供反压。 |
| shader compiler / RenderGraph 临时记录 | compiler 无额外内存 artifact map；反射临时量和 graph region 随调用释放 | artifacts 由调用者持有；graph region 不跨帧累积。 |
| Shader 磁盘 `.bin` cache | **没有自动容量、数量、TTL/LRU 淘汰**；内容/key 变化只使旧条目不再命中 | 基线已有策略，当前 v7/reflection v4 会留下旧文件；目录所有者负责清理。未在本任务另建磁盘缓存管理系统。 |
| D3D12 debug InfoQueue | 使用 native queue/device 生命周期；本任务未添加应用级容量策略 | statistics 遍历已存消息是基线行为，不将它误称为新材质 cache 泄漏。 |

缓存收集链包括：scope/lease 释放通知 coordinator → RHI0 completion/packet/record/native/cache 收集。故障测试保留 submitted work、descriptor、sampler、page，直到 fence/drain 完成；无新帧仍安排退休。Close 不会强行销毁仍被外部 packet/slice 持有的页面。

## 5. 回归验证与保留边界

| 当前最终版本验证 | 结果 / 原始记录 |
| --- | --- |
| Ninja Debug 构建及针对性资源/材质/Model/Scene 回归 | [5/5 通过](../out/MaterialAudit/ProgramSelectionTestsFinal.log)；[构建](../out/MaterialAudit/ProgramSelectionRebuild.log)。 |
| VS Release 构建 + 完整 CTest | **42/42 通过，74.97 秒**；[日志](../out/MaterialAudit/ProgramFinalVsRelease.log)。 |
| VS Debug 构建 + 完整 CTest | **42/42 通过，119.87 秒**；[日志](../out/MaterialAudit/ProgramFinalVsDebug.log)。 |
| 文件、格式、语义命名 | 225 owned paths、137 translation units 通过；[日志](../out/MaterialAudit/ProgramFinalNaming.log)。 |
| 模块边界 | 220 source files / 25 modules 通过；[日志](../out/MaterialAudit/FinalBoundaries.log)。 |
| OpenSpec | 全部 **26/26 strict** 通过，change tasks 无未勾选项；[日志](../out/MaterialAudit/OpenSpecAll.log)。 |
| 修复前后独立 probe | provider、ownership、缺省依赖、限定路径、负零的 [after 输出](../out/MaterialAudit/MainAfter/After.log)；预编译选择的 [before](../out/MaterialAudit/MainAfter/Variant.log) / [after](../out/MaterialAudit/MainAfter/VariantFixed.log)。 |
| 收尾版本核验 | 294 个冻结文件无漂移，HEAD 未变、staged 为空、tracked diff whitespace 检查通过；[核验记录](../out/MaterialAudit/FinalVerification.json)。 |

完整套件包含 triangle/model-viewer/scene-viewer、GUI、Model alpha/mask/sRGB/UV/mip/镜像/双面像素、Scene 剔除图像等价、共享实例与发布、descriptor 耗尽回滚、上传/Present 故障与 fence 保活、无新帧退休、offline startup，以及实际 RenderDoc capture/replay 验收。新增类型/数组/stride 拒绝检查没有以牺牲现有合法资源路径为代价。

先前失败日志保留，不能单独当作通过证据。测试中“geometry Ready”与“material Ready”分开等待；程序身份 fixture 曾因只等前者而失败，最终使用后者后才核对实际 Compiled 结果。最终整套日志覆盖修复后的当前源码；更早的全套通过不替代这轮验证。

保留以下明确边界：

- DXIL 原生反射不能恢复被展平的原 HLSL 轴大小；显式作者 schema 对维度顺序负责，自动 schema 如实显示 flat[N]。当前 DXC 无法为直接多维 matrix arrays 生成通过验证的 SPIR-V；这类数组只完成 DXIL GPU 验收，SPIR-V/MSL 没有对应 native backend 或该 matrix 验收结论。
- 当前 session 使用所选 Usage 的 `Default` variant。编译准备可以检查其他命名 variant，不意味着已有运行时非 Default 选择器。A-MAT-006 修复了显式程序被覆盖，不扩张到新 variant 调度系统。
- 低层 `PublishGroups` 面向材质 bridge 已验证的单调 revision 更新。独立手工 stale-revision probe 可使更新被忽略、而同批创建/移除仍执行；当前 Model/bridge 使用 `Revision+1` 并在 admission 后确认，没有确认可由本次正常桥接路径触发。它不承诺任意手工批次的通用事务回滚；本次未扩大该 API。此边界与已修复的旧 binding generation 撞槽缺陷不同。
- 没有实现全局 active-cache 内存预算、磁盘自动淘汰、完整阴影调度、MSAA/MRT/bindless 或 Vulkan/Metal 执行后端。这些属于已接受的首版边界，不为关闭 findings 扩大开发范围。

本轮结论限于已列源码路径、独立复现和当前硬件/工具链的测试覆盖；独立审计与全套通过不能证明不存在任何潜在缺陷。
