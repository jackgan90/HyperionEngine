# 增量视图与批次优化验证

本变更由 `97b22445a070dd7ed5a406f8dbeb7824f14a6962` 开始，按 proposal、design 和 tasks 实现。所有修改保留在工作区，不归档、不提交。

## 实现

- `CollectPrepared` 先在已发布的静态 collection 上进行当前视图的精确可见项选择。场景、资源发布及有序身份相同时复用 item storage；变化时恢复已有局部准备。透明排序、动态 collection、未就绪数据继续走完整路径。
- 静态准备保留保守 world bounds。只有 `FFrustum::Contains` 证明整个包围盒在视锥内时跳过原局部视锥测试；边界、非有限数据和不可用 bounds 不采用该证明。原 `PrepareSceneSnapshot(Collect(...))` 保留为差分对照。
- 内置批次策略维护完整 source identity 到兼容组、局部块和当前索引的映射。成员增删和局部输入变化只重建相关块；当前共享值仍按完整值比较。块的不可变内容身份独立于首成员和其他块的索引移动。
- 共享绑定组只在视图证明成员稳定后建立，持续变化的视图使用已有逐项共享更新。当前值在整组验证成功后发布，旧帧保留原对象。
- RHI 按独立块身份及资源发布版本复用源项准入，继续更新当前共享常量、instances 和每源回执，并保留原分组失败修复事务。
- 增量历史总计最多 4096 个源、512 个块、16 个视图/usage 条目；受影响的 payload 仍归既有 16 MiB 实例缓存预算管理。超过预算时淘汰历史或走完整路径，旧图持有的不可变数据不受改写。
- 已证明局部准备有效且 scene/resource 发布均未变化时，批次缓存的完整 CPU 退役扫描最多每 64 个家族执行一次。源发布变化、显式失效、没有证明或直接传入的无帧快照仍立即完整扫描；查用失效检查、硬容量、每视图 chunk 退役和 native fence 收集始终执行。材料 provider 保留原完整清理：尝试过的数字轮转索引没有表现出收益，已撤回。

## 回归与验证口径

- 准备可见项与完整路径逐项比较，包括 60 次边界变化、外来 previous、独立保留快照。
- 2000 个确定性随机变换验证完全包含证明不会与原精细裁剪冲突；包含镜像、边界和无效数据。
- 384 个实际静态 primitive、每批 128 项：分别改变首项、中项、尾项的可见性，检查一个受影响块、两个保留块、对应准入复用和零重新打包记录，并与普通绘制图像比较。
- 已准备的完整帧与缺少一项的帧倒序执行，分别与各自参考图像比较。
- 专用材质显式允许局部覆盖 view projection；初始共享值相等时合批，随后相机变化使两源共享值不同，要求正确拆批并与普通绘制一致。标准材质锁定规则保持不变。
- 24 个视图轮换、历史淘汰和重新进入规划，检查图像与缓存上限。
- 原有 provider 资源及时释放、旧值独立持有、批次显式失效、源移除和无新帧退役回归。
- 已有 instance、material、scene、CSM、资源退役和异常恢复套件继续覆盖透明顺序、定制策略、设备限制、资源切换、native preparation 失败与组内修复。

## 实测方法

原始文件集中于 `out/incremental-view-batches-20260910/`。`Baseline.json` 和 `baseline-{debug,release}` 固定基线二进制及 DLL；最终测量脚本 `MeasureFinal.py` 会复制候选 runtime 并记录 SHA-256。

普通 Viewer A/B 使用 Scene.json、79/79 模型就绪、GUI 开启但窗口隐藏、VSync 关闭、四级 2048 阴影、正常裁剪。Debug 保持 D3D12 验证开启。每次预热 200 帧、记录 400 帧；静止、小幅 orbit（0.1 像素）、大幅 orbit（10 像素）分别交换顺序测两轮。每配置 4800 个记录帧，共 9600 帧。

逐帧检查完整 frame 范围、source coverage、主视图和阴影视图数量、draw 数、零源失败、零 native validation error，以及 `gpu_sample_frame == frame + 1`。固定眼位 yaw 使用独立 SceneProbe，逐项比较 `CollectPrepared` 和原始完整 collection 的身份；探针计时不代替 Viewer 帧时间。

BatchProbe 为独立 CPU 规划诊断：1536 个准备源、1024 个可见项、测试 shader 每批容量 4，预热 80 次、测量 200 次。合成场景身份固定为 99，使用当前资源发布版本和 Renderer 实际生成的局部准备证明。它比较首/中/尾成员替换和滑动窗口，校验源唯一覆盖及 fallback 打包字节；数值不能直接换算为 Showcase 的 FPS。

Tracy 使用单独构建与捕获，不和关闭 profiling 的 Viewer 计时混算。现有 CPU maintenance 与 GPU fence 完成检查不因 profiling 或性能目标被关闭。

## 最终结果

### 普通 Viewer 帧时间

以下单位为 ms，mean/P95 由每组两轮共 800 帧合并计算；最后一列为平均帧耗时降幅，正数表示更快。这里使用关闭 Tracy 的冻结 runtime。

| 配置 | 相机 | 基线 mean / P95 | 优化后 mean / P95 | mean 降幅 |
| --- | --- | ---: | ---: | ---: |
| Debug | 静止 | 6.207 / 6.949 | 6.171 / 6.843 | +0.57% |
| Debug | 小幅 orbit | 14.249 / 16.314 | 12.923 / 14.807 | +9.30% |
| Debug | 大幅 orbit | 19.762 / 22.521 | 17.340 / 19.523 | +12.25% |
| Release | 静止 | 2.250 / 3.913 | 2.258 / 4.114 | -0.36% |
| Release | 小幅 orbit | 2.508 / 3.291 | 2.509 / 3.483 | -0.04% |
| Release | 大幅 orbit | 3.044 / 3.803 | 2.849 / 3.595 | +6.42% |

Debug 小幅/大幅移动的平均帧耗时分别下降 9.30% / 12.25%，Release 大幅移动下降 6.42%。Release 静止和小幅移动 mean 基本持平，P95 分别由 3.913 → 4.114 ms、3.291 → 3.483 ms，不能宣称这些负载也整体变快。Release 小幅移动的 `pipeline_prepare_ms` 为 0.945 → 0.839 ms，CPU 准备减少没有转化为整帧收益。

两轮大幅移动 mean 分别为 Debug 19.777 → 17.180 / 19.746 → 17.500 ms，Release 3.030 → 2.822 / 3.059 → 2.877 ms。静止和小幅移动的全部轮次、分位数和计数保存在 [Debug Summary](F:/HyperionEngine/out/incremental-view-batches-20260910/delivery-debug/Summary.json)、[Release Summary](F:/HyperionEngine/out/incremental-view-batches-20260910/delivery-release/Summary.json) 及旁边 CSV；[FinalAnalysis.json](F:/HyperionEngine/out/incremental-view-batches-20260910/FinalAnalysis.json) 是统一汇总。

### 可见成员与局部更新

四组 SceneProbe 均完成 200 个测量帧 × 5 个视图，外加第 199 帧作为初始成员集合。新旧二进制的 4020 条逐帧逐视图数量记录完全相同；当前进程内 `CollectPrepared` 与原完整路径逐项比较 primitive 身份和 ordinal，全部通过。以下成员留存比例为主视图相邻帧的 kept 总数 / visible 总数。

| 轨迹 | 主视图成员留存 | 有序成员完全相同的帧 | 平均受影响块 / 保留块 | 主视图外阴影投射项范围 |
| --- | ---: | ---: | ---: | ---: |
| orbit 0.1 | 99.974% | 90.0% | 0.100 / 5.870 | 39–40 |
| orbit 10 | 99.030% | 0.5% | 2.480 / 3.490 | 37–66 |
| yaw 0.1 | 99.962% | 85.0% | 0.150 / 5.850 | 39–40 |
| yaw 10 | 96.947% | 0.5% | 4.425 / 1.575 | 36–170 |

主视图外仍有独立阴影投射者；每一级 CSM 继续使用自身可见集合。大幅运动时成员顺序几乎每帧变化，但主视图仍有约 97%–99% 源保持可见，因此整视图 plan 命中率低不再要求重建所有块。

BatchProbe 两轮合并的 CPU 规划平均耗时如下。无帧探针继续完整执行退役扫描，不借用真实帧的 64 家族维护优化。

| 负载 | 基线 ms | 优化后 ms | 基线重建块/次 | 优化后重建块/次 |
| --- | ---: | ---: | ---: | ---: |
| 成员稳定 | 1.542 | 1.400 | 0 | 0 |
| 首项替换 | 11.502 | 1.800 | 256 | 1 |
| 中项替换 | 10.596 | 1.758 | 128 | 1 |
| 尾项替换 | 8.325 | 1.735 | 1 | 1 |
| 每次滑动 1 项 | 15.249 | 2.312 | 256 | 1 |
| 每次滑动 4 项 | 7.881 | 2.436 | 1 | 1 |

首/中/尾替换时，优化后每次均保留 255 块，只重新访问受影响块的 4 个准备输入，而基线为 1024 个；已有 record 无须重新打包。独立 local-value 探针每次只构造 1 个新输入、打包 1 条新 record、重建 1 块并保留 255 块。每次滑动 1 项时重新打包记录由基线 1 条增至 1.75 条：稳定块布局减少了跨块重组，但不会保证每种历史下 record 缓存淘汰量都最小。这一代价保留在原始计数中。

这些计数来自独立合成规划负载，不能换算为 Viewer FPS。真正走 RHI 的 384 源图像回归也验证首/中/尾移除：1 个受影响块、2 个保留块、2 次批次准入复用、0 条新打包记录；与关闭合批的普通绘制逐像素一致。旧帧倒序执行、共享值拆批和 24 个视图轮换的图像比较均通过。

九项额外回退探针覆盖 duplicate、no-local-id、zero-generation、foreign-scene、zero-publication、custom strategy、device、capacity 和 preparation failure；均验证完整覆盖、没有错误增量命中，且产生的 instance bytes 与原打包器相同。最终 A/B 的增量历史高水位为 864 个源 / 30 块；prepared input 缓存高水位 946 项 / 1,929,920 字节。容量回归继续验证 4096 源、512 块及 16 MiB payload 上限。

### CPU 剖析与剩余开销

最终 Tracy A/B 为单独的 Debug `detail-gpu` 构建：各预热 200 帧、记录 200 帧；两个捕获均通过完整 frame/source/native 校验，GPU 各有 1400 个事件，逐帧 GPU submission identity 正确。下表为 zone 总时长 / 200，单位 ms/帧，含子调用的 zone 不与子 zone 重复相加，也不把跨线程时间相加当成帧时长。

| CPU 工作 | 基线 | 优化后 |
| --- | ---: | ---: |
| 五视图准备整体 | 10.475 | 7.239 |
| 批次规划 | 3.265 | 1.598 |
| 批次完整退役扫描 | 0.469 | 0.009 |
| 材料准备 | 2.455 | 2.810 |
| 材料 provider 清理 | 0.858 | 0.897 |
| 资源维护 | 2.404 | 2.271 |
| native 图准备 | 4.512 | 4.307 |
| GUI 构造 | 2.124 | 2.107 |

基线 `CollectScene` 1.809 ms + `PrepareSceneSnapshot` 1.923 ms 的工作对应新路径的 `SelectPreparedSceneItems` 1.397 ms、变化时的 `MaterializeVisibleSceneItems` 0.363 ms 及保留准备工作。批次完整退役扫描由 199 次降至 3 次，查用有效性、容量和 native fence 收集持续生效。

材料准备在本次大幅移动剖析中由 2.455 增至 2.810 ms，是仍存在的退化；普通 Debug A/B 的主视图 material mean 也由 2.294 增至 2.796 ms。当前值刷新、保留组验证、provider 清理、每源回执、native 图准备和 GUI 仍占明显 CPU 时间。共享组只为成员稳定的视图建立：最终 `RetainMaterialGroups` 共 12 次 / 0.002 ms 每帧，`UpdateRetainedMaterialGroups` 为 0.970 ms 每帧；这不等于已消除材料逐项工作。没有把材料 provider 的完整清理改成轮转清理，也没有关闭 D3D12 验证、GUI 或阴影来取得计时收益。

可见性查询、局部证明检查、索引映射和源回执仍可能遍历当前项。本次实现减少完整求值及不相关块重建，并非整个 pipeline 已变成 O(delta)。上述剖析只定位开销，普通帧性能结论以前一节关闭 profiling 的 A/B 为准。

### 图像与验证边界

1440×900、四级 CSM、GUI 关闭的单独图像检查：静止和小幅 orbit 的 PNG 与冻结基线逐字节一致。首次大幅 orbit 的候选图在 6 个边缘像素上不同，占 0.000463%，最大单通道差 46/255；原始 strict 比较因此失败，原图及差异坐标完整保留。随后四轮相同二进制的大幅 orbit 捕获全部逐字节匹配基线，新旧普通绘制参考也逐字节一致。

该首次差异没有在后续捕获中重现，根因未确认，因此不声明所有跨进程画面恒定逐字节一致，也未放宽既有 C++ 图像回归断言。证据在 [images/Summary.json](F:/HyperionEngine/out/incremental-view-batches-20260910/images/Summary.json)，包括原始差异像素、所有 PNG 的 SHA-256 和匹配结果；额外捕获命令保存在同目录 `RepeatCommands.json` / `AdditionalRepeats.json`。基线及候选的模型均 79/79 ready、源失败为零、native validation error 为零。

### 构建、检查与复现

- 最终 Debug / Release 全目标构建和 Tracy Viewer 构建通过；最终 Debug CTest 48/48、Release CTest 43/43 通过。
- 完整语义命名检查 198 个翻译单元通过；后续两个修改单元重新命名检查通过。最终 316 个源文件的文件名/include/格式检查、307 个源文件与 25 个模块的边界检查通过。
- `git diff --check` 和 `openspec validate optimize-incremental-view-batches --strict` 最终交付检查通过。所有源码与构建冻结时记录的 37 个 SHA-256 相同。
- 最终检查日志为 `Build-delivery-{debug,release,trace}.log`、`Test-delivery-{debug,release}.log`、`Style-naming.log`、`Style-delivery-naming.log`、`Style-delivery-final.log`、`Boundaries-delivery-final.log`；均在上述原始证据目录。

最终候选 `hyperion_viewer.exe` SHA-256：

```text
Debug   0837b0c222e1a00a5ebc4acc2895724bc8c0eda6525c59e3af629290692eec4f
Release fd5411aa18526a3f762e2ef36f894d112ebdbed317b5f11027be95198f9c4c81
```

[DeliveryHashes.json](F:/HyperionEngine/out/incremental-view-batches-20260910/DeliveryHashes.json) 记录源码、独立探针及冻结 trace runtime 的完整身份；每配置 `Summary.json` 记录 Viewer/DLL/CMakeCache 身份和每次运行命令。主要复现入口如下，输出目录需使用新的目录以保留当前证据：

```powershell
# 普通 A/B 的确切参数与交错顺序
Get-Content out/incremental-view-batches-20260910/MeasureFinal.py
# 独立完整场景与局部规划探针
out/incremental-view-batches-20260910/SceneProbe.exe yaw 10 out/SceneProbeRerun.csv
out/incremental-view-batches-20260910/BatchProbe.exe 200
out/incremental-view-batches-20260910/BatchLocalProbe.exe 200
# 保留原始图像差异的最终汇总与证据检查
python out/incremental-view-batches-20260910/AnalyzeFinal.py
# 单独的最终 trace：baseline/candidate 各有 Metadata.json 中的完整命令
Get-Content out/incremental-view-batches-20260910/trace-delivery-candidate-large/Metadata.json
```

冻结输入、脚本及大型原始记录保留于忽略的 `out/`，本 validation 文档随 active OpenSpec change 保留。工作区不暂存、不提交、不归档。


## 独立审计修复（2026-09-10）

上述冻结帧性能与图像证据属于修复前交付版本。随后 quality-audit 确认 IVB-01（停用视图历史保留 compiled/default CPU 数据）及 IVB-02（增量路径漏报 CapacitySplits）。本次仅修复这两项及其直接回归，不扩大到后续材料、RHI、GUI 或 Debug 开销优化。

持久分组改为只持有兼容性结构；完整共享数值 signature 为构建内临时对象，candidate/program/default payload 不进入 history。容量拆分统计按当前有效组和非空批次布局发布。新增真实 session 回归覆盖静态源删除、停止请求旧视图、延迟旧图的图像及拥有关系、强制原有完整 cache sweep 后 CPU 默认纹理和 compiled 的释放；现有首/中/尾回归增加计数断言，并验证完整块退出时拆分次数由 2 降至 1、0。

- 修改前新生命周期回归失败：`Texture.expired() && Program.expired()`；日志为 `fix/Test-red-debug.log`。修复后同一场景通过，保持原图像断言。
- Debug / Release 全目标构建通过：`fix/Build-debug.log`、`fix/Build-release.log`。
- 两种配置各 6/6 定向 CTest 通过：scene_spatial_visibility、instance_batching、scene_rendering、material_bindings、render_resources、cascaded_shadow_rendering。Debug 40.64 秒、Release 21.96 秒；日志为 `fix/Test-debug.log`、`fix/Test-release.log`。
- 完整格式/路径检查 316 文件、边界检查 307 文件 / 25 模块、三个修改翻译单元语义命名及布尔命名检查通过；日志为 `fix/Style.log`、`fix/Boundaries.log`、`fix/Naming.log`。
- 修复证据位于 [audit-incremental-view-batches-20260910/fix](F:/HyperionEngine/out/audit-incremental-view-batches-20260910/fix)。`Manifest.json` 固定复审的 43 文件，`Repair.patch` 记录相对首次审计快照的修复，`BuildHashes.json` 记录新构建的程序身份。
- 原独立 reviewer 针对性复审已关闭 IVB-01 / IVB-02，未发现新增可确认 finding：[ReReview.md](F:/HyperionEngine/out/audit-incremental-view-batches-20260910/fix/ReReview.md)。独立复跑 Debug / Release 的 instance_batching、scene_rendering，分别 2/2 通过，16.35 秒 / 2.31 秒；重新编译的原 CacheProbe 在保留 8 项旧视图结构时已释放 program/default texture，完整与增量路径 capacity_splits 均为 1。额外 GroupProbe 的 11 步多组清空、追加、槽位复用和回退序列全部通过，逐步核对完整当前签名、源覆盖、实例字节和计数。原始日志位于 fix/reviewer。
- 主 agent 独立读取探针及调用链，并重跑 CacheProbe 得到相同释放和计数结果：[RootCacheProbe.log](F:/HyperionEngine/out/audit-incremental-view-batches-20260910/fix/RootCacheProbe.log)。[RootVerification.json](F:/HyperionEngine/out/audit-incremental-view-batches-20260910/fix/RootVerification.json) 核对 43/43 冻结文件、6/6 构建程序身份、HEAD 和空暂存区；最终仅 tasks 5.3 与本复审完成记录作元数据收尾，单独保存最终文档摘要，不覆盖原复审快照。
- 本轮没有重新执行完整 CTest 或完整帧性能/Tracy 矩阵；历史 6 像素差异的根因未确认限制继续保留。修复保持工作区未提交、active change 未归档。
