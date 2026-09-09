# Renderer 增量更新优化

2026-09-09，OpenSpec `optimize-incremental-render-updates`，基线 `5895bd7f9409a752b868bc46c34e3e89c9f7fca3`。小幅/大幅相机运动的 CPU 准备均值，Debug 分别降低 **35.8% / 32.6%**，Release 降低 **37.0% / 36.9%**。最初提出的 Debug 60% / 50% 工程目标没有达到；下面保留静止路径和整帧分布中的回退，不将 CPU 阶段收益等同于所有场景的帧率提升。

## 交付实现

- 稳定 local 材质结果与不可变 `FMaterialSharedParameters` 分开保留。兼容 engine 参数按组刷新；batch、instance packing 和实际 draw 都读取当前有效 shared/local 值。混合依赖、default/缺失输入、覆盖、资源变化、Draw 依赖与自定义策略保留正确 fallback。
- 静态 primitive 缓存完整就绪的 collection 和材质/几何元数据；`FRenderItemList` 在裁剪、排序、重用时转移 item 拥有句柄，显式复制则深拷贝。旧 snapshot 不被后续 view 消耗。自定义 collection 继续使用现有 vector 发射接口，透明排序和回执保持原语义。
- 实例记录按完整反射布局、参数映射与有效值验证。不可变块按布局及有序记录身份复用，可以跨兼容 view/pass 共享 CPU 字节与 GPU slice。Object 变化只重新打包对应记录、拼装 Object 块；未变 Surface 块继续复用。实例 draw 跳过随后会被替换的单实例常量准备。
- Batch signature 分离不可变结构和共享数值列表。结构池只在完整兼容性比较成功后合并身份，哈希仅选择候选；共享列表要求覆盖全部非实例数值。自定义策略仍获得完整当前 signature。
- View 历史最多 64 份、120 个未使用 frame；静态 primitive collection 最多 64 项；记录、块、chunk、layout 与 GPU block 历史均有预算及退休路径。常量发布仍不可变，GPU fence 保活和 native 验证没有削弱。

接口与预算详见 [Materials.md](Materials.md) 和 [InstanceBatching.md](InstanceBatching.md)。`FRenderSceneSnapshot::Items` 现在是支持范围/索引访问的 `FRenderItemList`，不承诺连续 `FRenderItem` 存储；`FInstanceConstantBlock::Bytes` 为共享不可变字节，`FRenderBatchSignature` 引用结构与值列表。这些 C++ 接口随仓库调用方一并更新；未改变资产、shader CBV ABI、CLI 既有参数、插件 ID 或构建目标。

## 测量条件

Windows / NVIDIA RTX 5080 / D3D12 debug layer enabled，1440×900，4 worker / 2 RHI。Showcase 79/79 ready、0 failed；CSM 为四级、每级 2048²、每帧更新。静止时主视图 195 items / 6 draws，阴影合计 643 items / 24 draws。Forward 固定可见集关闭阴影和裁剪，235 items / 6 draws。小幅移动使用实际相机输入轨迹、每步 0.1 像素；大幅为 10 像素，包含可见集合及 cascade 变化。

Debug 保留 `/Od /Ob0 /RTC1`；Release 保留 `/O2 /Ob2`。A/B 的 Tracy 关闭；Debug RenderDoc 支持已编译但没有进行捕获。VSync 关闭；除“可见 UI”外均为 hidden/no-ui。未通过降低画质、关闭验证、减少绘制或降低更新频率获得收益。

每个 workload/version 先 warmup 200 帧，再采样 400 帧；两轮交换 baseline/candidate 执行顺序，每个版本合计 800 样本。全部测量串行，GPU 测试/编译/trace 不与计时交叉。表中分位数从合并的 800 个原始样本计算，未平均两轮分位数。`pipeline_prepare_ms` 使用既有 Viewer CPU 准备计时；`frame_ms` 包含 Present/调度影响。Trace 单独用于阶段归因，不参与 A/B 表。

## 完整 A/B 结果

单位 ms；所有箭头均为基线 → 当前。“准备降低”为正表示改善，为负表示回退。

| 配置 / 负载 | 准备均值 | 准备降低 | 整帧均值 | 整帧 P95 |
|---|---:|---:|---:|---:|
| Debug / CSM / 大幅移动 | 33.132 → 22.330 | 32.6% | 34.482 → 23.656 | 37.642 → 26.223 |
| Debug / CSM / 静止 | 1.480 → 1.453 | 1.8% | 3.428 → 2.897 | 5.436 → 4.560 |
| Debug / CSM / 小幅移动 | 21.177 → 13.599 | 35.8% | 22.535 → 14.909 | 28.755 → 18.246 |
| Debug / Forward 固定可见集 / 静止 | 0.548 → 0.555 | -1.4% | 2.468 → 2.562 | 4.686 → 4.699 |
| Debug / Forward 固定可见集 / 小幅移动 | 4.871 → 3.238 | 33.5% | 5.714 → 4.020 | 6.271 → 4.689 |
| Debug / CSM 可见 UI / 静止 | 1.822 → 1.845 | -1.2% | 6.101 → 6.179 | 8.092 → 8.004 |
| Debug / CSM 可见 UI / 小幅移动 | 23.187 → 15.667 | 32.4% | 27.469 → 19.866 | 36.052 → 25.202 |
| Release / CSM / 大幅移动 | 3.031 → 1.913 | 36.9% | 3.984 → 3.622 | 5.109 → 5.600 |
| Release / CSM / 静止 | 0.084 → 0.100 | -18.9% | 2.726 → 2.527 | 5.098 → 4.745 |
| Release / CSM / 小幅移动 | 1.736 → 1.094 | 37.0% | 3.482 → 3.168 | 4.912 → 5.149 |
| Release / Forward 固定可见集 / 静止 | 0.039 → 0.043 | -9.4% | 2.110 → 2.299 | 4.712 → 5.113 |
| Release / Forward 固定可见集 / 小幅移动 | 0.478 → 0.341 | 28.7% | 2.711 → 2.837 | 4.992 → 5.058 |
| Release / CSM 可见 UI / 静止 | 0.094 → 0.131 | -38.8% | 3.552 → 3.632 | 5.034 → 5.047 |
| Release / CSM 可见 UI / 小幅移动 | 1.922 → 1.246 | 35.2% | 4.062 → 3.988 | 5.306 → 5.375 |

主要运动负载的补充分位数：

| 配置 / 负载 | 准备 P50 | 准备 P95 | 准备 P99 | 整帧 P50 | 整帧 P99 |
|---|---:|---:|---:|---:|---:|
| Debug / CSM / 大幅移动 | 32.912 → 22.148 | 36.245 → 24.870 | 39.192 → 28.583 | 34.250 → 23.460 | 40.496 → 29.860 |
| Debug / CSM / 小幅移动 | 22.093 → 14.157 | 27.443 → 17.003 | 29.589 → 18.436 | 23.400 → 15.439 | 30.944 → 19.875 |
| Release / CSM / 大幅移动 | 2.977 → 1.840 | 3.845 → 2.677 | 4.172 → 3.248 | 3.946 → 3.695 | 5.671 → 6.513 |
| Release / CSM / 小幅移动 | 1.702 → 1.049 | 2.457 → 1.624 | 2.901 → 1.890 | 3.706 → 3.392 | 5.428 → 5.876 |

静止 Release 的准备耗时增加约 0.004–0.037 ms，静止可见 UI 的相对增幅达到 38.8%。Release 固定可见集移动的整帧均值由 2.711 增至 2.837 ms；Release CSM 小幅/大幅移动的整帧 P95 也有回退。这里只有两轮本机测量，不能将这些差异判定为可忽略，也不能据此声称所有整帧分布都改善。总体收益集中在运动时的 CPU 准备。

## 更新粒度与字节

以下为 Debug 每帧平均；Release 的该表实例上传与常量写入差分相同。`constant_bytes_written` 是累计量，表内使用各轮 `(末值−首值)/399`，不能把累计量均值当作每帧工作。main/shadow upload 是每帧量。

| 负载 | 全部常量写入 B/帧 | 主视图 instance 上传 B/帧 | 阴影 instance 上传 B/帧 | 当前新打包 / 复用记录数 | 当前拼装复制 B/帧 |
|---|---:|---:|---:|---:|---:|
| Debug / CSM / 大幅移动 | 36553.50 → 2549.89 | 22289.400 → 1111.200 | 13420.440 → 682.200 | 0.0550 / 278.8875 | 1793.40 |
| Debug / CSM / 小幅移动 | 2038.02 → 770.77 | 1068.000 → 53.400 | 262.800 → 13.320 | 0.0025 / 10.7225 | 66.72 |
| Debug / Forward 固定可见集 / 小幅移动 | 80.00 → 80.00 | 0.000 → 0.000 | 0.000 → 0.000 | 0.0000 / 0.0000 | 0.00 |

小幅运动主/阴影 instance 上传合计从 1330.80 降至 66.72 B/帧，大幅从 35709.84 降至 1793.40 B/帧。固定可见集运动仍只写入每帧 80 B View 常量，instance 上传保持 0；这个场景的 CPU 改善来自减少准备/发布工作，不能归因于减少 GPU 字节。

当前小幅/大幅运动分别记录约 714.825 / 835.410 次 shared item update 每帧；shared provider 数值是按组求值，item 仍需检查和关联当前不可变更新。静止 CSM 每帧复用 5 份 prepared view 和 838 个 item 存储；运动时大部分 item 存储仍可复用。记录/块计数只统计实际进入 packing cache 的工作，完整 plan/packet 命中会跳过它们。`packed_bytes` 只计新记录打包（失败修复 fallback 仍计完整重打包），不能代替 `assembled_bytes` 或 GPU upload。

28 组 A/B 的采样窗口内，PSO、descriptor、native list **累计创建数均不增长**。启动阶段累计 descriptor 数因发布顺序在部分运行间不同，不用其绝对存量宣称运行期退化或改善。绑定仍按现有 native 规则执行；本轮未改写 D3D12 录制代码。

## 剩余瓶颈与中途修正

最终 Debug detail trace 单独 warmup 200 / capture 200 帧，Tracy 开启，单位为各同名 zone 总时长除以采样帧数。父子 zone 不应相加；RHI 并行 zone 之和不是整帧关键路径。Fixed 列的 collection/snapshot 为 0 表示复用路径没有执行该 zone。

| Zone | CSM 小幅 | CSM 大幅 | Forward 固定可见集移动 |
|---|---:|---:|---:|
| CollectScene | 2.025 | 2.489 | 0.000 |
| PrepareSceneSnapshot | 1.301 | 1.527 | 0.000 |
| PrepareMaterials | 2.742 | 3.726 | 1.030 |
| PlanBatches | 1.441 | 6.895 | 0.409 |
| PrepareDraws | 3.111 | 3.452 | 0.877 |
| RecordNativeDraws | 0.238 | 0.250 | 0.067 |

最终小幅 trace 中，普通 item 增量 refresh 为 0，full evaluation 为 0.01/帧，shared update 为 712.725/帧，provider evaluation 为 23.675/帧。已经去掉共享变化导致的逐 item 完整 local 结果发布，但 shared 身份/依赖检查、collection admission、回执和 draw 准备仍随 item/view 数量增长。

大幅运动的 `PlanBatches` 仍为 6.895 ms，其中 fresh grouping 4.288 ms、plan publication 1.764 ms；fresh grouping 内 instance data 准备为 2.773 ms。当前 CBV ABI 需要连续实例数组，成员变化仍要组装受影响块。下一步若继续追求最初目标，需要针对计划/可见成员与回执进一步解耦并重新测量；持久 GPU slot/visible-index ABI 尚未实现，不能宣称只更新单个 GPU 元素。

实施过程保留了不成功的阶段，完整记录见 OpenSpec [design.md](../openspec/changes/archive/2026-09-09-optimize-incremental-render-updates/design.md)：

1. 只分离 shared/local publication 和缓存元数据没有达到目标；stage 5 交替 A/B 仅降低准备均值 15.5%。trace 显示全量 item 容器的移动/析构和排序发布仍昂贵，因此引入稳定 item 拥有句柄及一次连续排序索引。
2. stage 6 小幅降幅达到 32.0%，stage 7 大幅仅 22.7%。大幅 trace 中 planning 9.512 ms，推动了完整 batch 结构与共享数值的分离，以及完整比较后的结构身份复用；独立 record/block cache 此前已实现。阶段数据只作演进依据，最终结论使用本页冻结 A/B。
3. 全场景运行曾暴露 geometry 先于 material 就绪的 metadata 空 program；已增加重新查询/明确失败处理，后续 GPU 回归及画面矩阵通过。失败运行保留日志但排除计时样本。
4. 实例历史现在共享原有总预算；计划退休测试提高其字节空间以单独验证 16-item plan 限额，另一个 64 KiB 压力测试继续校验总字节上限。独立第二 view 保留一份 View block，退休测试的稳定块数由 6 改为 7，仍验证 64 次对象更新不积累历史。
5. 最终 native recording 只有约 0.238–0.250 ms，证据不足以支持在本轮引入新的 native command-stream ABI。保留既有验证、不可变提交和 fence 退休，没有以移除检查换取时间。

## 正确性、稳定性与审查

- A/B 共 28 对运行、11,200 对采样帧。逐帧比较主视图/阴影 items、draws、instance/single 数、失败数及 64 MiB CSM 载荷，全部一致。该检查是工作量计数等价；逐源索引独占由 `instance_batching` 的策略/覆盖测试另行验证。
- Debug/Release × Forward/CSM × 静止/小幅/大幅共 **12 对画面**，frame 219 捕获的 RGBA 字节完全一致；所有运行 79/79 ready、validation errors 0，CPU/GPU 样本帧对应有效。
- Debug/Release 的小幅/大幅相机各运行 1800 帧，warmup 后覆盖 40 次运动周期。最后 400 帧 PSO/descriptor/native list 累计数稳定，GPU allocation 范围见下表。该试验覆盖持续相机变化，不等同于所有长时资源创建/销毁场景；后者另由退休/失败恢复测试覆盖。
- 两种配置均运行真实可见 UI 窗口，Win32 按本次 Viewer PID 验证可见 client area，并保留截图；没有把 hidden 离屏输出当作可见窗口验收。
- Debug 完整 CTest **48/48** 通过。Release 首次 **42/43** 通过；`scene_viewer_acceptance` 在启动 Viewer 前因 sandbox C: alias 与 F: root 的 `os.path.relpath` 跨盘失败。用相同脚本/二进制、显式 F: 工作目录重跑通过，覆盖 514 instances、none/linear/bvh 等价、GUI 与局部失败；因此 43 个 Release 用例均已成功执行，但首次整套运行不是 43/43。
- Profile 的三个 `profiling_*` 测试全部通过，包含实际 Tracy 连接、重连、嵌套/GPU scope、Viewer 与类别控制。Debug/Release/Profile 构建通过。Style/format、187 个编译单元的 semantic naming、291 个源文件/25 模块边界及 `git diff --check` 通过。
- 独立 reviewer 不继承实现会话，审阅全部冻结候选修改和新增文件，核对 hash、共享参数、快照/多 family、布局/值完整比较、自定义策略、弱 ownership 与 fence。未发现可确认的引入缺陷；其未自行运行 GPU 测试，最终测量和图像由主流程验证。源码在该审查及最终 builds/tests/A/B 之间保持相同，后续仅同步文档。

| 持续运行 | 尾部 GPU allocation MiB 范围 | PSO 累计 | descriptor 累计 | native list 累计 |
|---|---:|---:|---:|---:|
| sustained-debug-step0.1 | 70.973–70.973 | 4 | 136 | 12 |
| sustained-debug-step10 | 71.660–71.723 | 4 | 136 | 12 |
| sustained-release-step0.1 | 70.973–70.973 | 4 | 136 | 12 |
| sustained-release-step10 | 71.660–71.723 | 4 | 136 | 12 |

关键回归包括 `CheckIndependentFamilies`、`CheckMovingViews`、`CheckProviderFallback`、`CheckViewResourceRefresh`、两种 shared overlay 的 `CheckSharedRefresh`、旧帧像素、custom strategy/collection/ordering、跨 view block/slice 复用、单对象仅打包一个记录及 Object 块、容量/总预算压力与失败 Present 退休。

## 复现与证据索引

本机原始证据位于 [out/incremental-render-updates-20260909](../out/incremental-render-updates-20260909)，不纳入 Git。`FinalAggregate.json` 保存完整 mean/P50/P95/P99/min/max、28 对 coverage；各 `final-*/Summary.json` 保存精确命令、设置、hash 和逐轮指标，CSV/log 为原始样本。`FinalTraceSummary.json` 及 `final-trace-debug-*` 保留最终 trace、zone/plot 导出。`delivery/Summary.json` 保存像素对照、持续运行、窗口结果及其命令，目录中有全部截图/CSV/log。`FinalCandidate.json` 标识初次交付的源码、文档与运行时文件；归档提交文件集另由 `ArchiveCommitCandidate.json` 记录，源码保持相同。重新构建后应先核对 hash。

```powershell
# baseline-* 是改动前保留的完整运行时目录；output 必须使用新目录。
python tools/CpuSubmissionBenchmark.py --viewer out/build/debug/bin/hyperion_viewer.exe `
  --baseline out/incremental-render-updates-20260909/baseline-debug/hyperion_viewer.exe `
  --output out/incremental-repeat-debug-small --counts --scene --scene-shadows on `
  --motion both --warmup 200 --samples 400 --trials 2 --timeout 90

# 大幅：--motion moving --camera-step 10
# 固定可见集：--scene-shadows off --fixed-visibility
# 真实 UI：--visible --ui
# Release：两处 debug 改为 release，并使用独立 output。

powershell -NoProfile -ExecutionPolicy Bypass -File tools/Build.ps1 -Preset debug
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Build.ps1 -Preset release
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Build.ps1 -Preset profile
ctest --test-dir out/build/debug --output-on-failure
ctest --test-dir out/build/release --output-on-failure
ctest --test-dir out/build/profile -R '^profiling_' --output-on-failure
python tools/CheckStyle.py
python tools/CheckStyle.py --naming --build-dir out/build/debug
python tools/CheckBoundaries.py
openspec validate --all --strict
```

| 冻结 Viewer | SHA-256 |
|---|---|
| debug / baseline | `46171eb00cf013452c02a429fdcc25dc52ad3c5b27a8e20fd712b45ba90aa37e` |
| debug / candidate | `001dec8b3330235169eec5b100410af9b2ac3e436251f83317394cf3a7c6cfea` |
| release / baseline | `f27dfd8647df1d892b62224140fd7c83ca56d431b34f988368c2d23bb7e6efc9` |
| release / candidate | `bfda1b77f9824e146652bf1921667169adc73b26c93d6b803b69edb7fe4308d8` |

构建/回归/检查日志分别为 `Stage9Build.log`、`ReleaseBuild.log`、`ProfileBuild.log`、`DebugCTest.log`、`ReleaseCTest.log`、`ReleaseSceneAcceptanceRetry.log`、`ProfileCTest.log`、`Style.log`、`Naming.log`、`Boundaries.log`。初次交付按要求未暂存或提交；随后用户于 2026-09-09 授权归档和 Git commit。本次提交包含已同步的主规范及 [归档记录](../openspec/changes/archive/2026-09-09-optimize-incremental-render-updates/implementation.md)，性能样本与源码保持不变。
