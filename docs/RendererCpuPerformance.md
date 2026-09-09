# Renderer CPU 提交优化

本轮对应 OpenSpec `optimize-renderer-cpu-submission`，基线为 `144c1e5`。实现顺序为测量基准、材质与批次刷新、D3D12 录制、数据所有权与任务调度。原始分析见本机 [Analysis.md](../out/cpu-analysis-20260909/Analysis.md)。

## 实现

- 材质求值按实际依赖预备参数索引。仅依赖 Global/Frame/Scene/View/Pass 的兼容语义使用同一视图内共享的不可变值覆盖层；Object/Material/Draw、混合依赖、覆盖和默认值仍按各自契约处理。覆盖层是平坦存储，不串联历史帧。
- 内置 instance 策略在局部值、资源、几何、状态及组内共享值仍兼容时复用成员计划；instance 参数变化仍重建载荷，自定义策略仍完整重新求值。全局缓存过期扫描移到 family 边界，逐视图只遍历自己的 chunk 范围。
- D3D12 按完成 fence 的 frame slot/context 复用原生命令列表。逻辑录制仍独立；外部保留旧录制时创建另一个原生列表。每次录制的 PSO、topology、VB/IB view、stencil、blend 和 scissor 缓存从空状态开始。
- 保留设备/类型/范围/发布版本/上传完成/附件验证。常量槽重复检查改用经过 root signature 预算验证的位掩码；已发布常量区间使用有序查找；纹理上传完成用一次 fence 快照和 binding set 的最大上传 fence 验证。
- `CompileAndConsume()` 转移图的命令向量，`RecordOwned()` 让 fence 保留同一份不可变命令。原有 `Compile() const` 和借用式 `Record()` 继续复制输入。调用 `RecordOwned()` 后不得通过其他别名修改命令。
- 不透明和 overlay 项不再计算无用的透明深度排序键；关闭剔除时不计算局部剔除矩阵。静态 primitive 直接在输出容器构造条目。设备统计并入 EndFrame 的 RHI 任务，GUI 无绘制时仅在首次隐藏时清理旧数据。

未改变 Debug 编译优化、验证开关、CSM 数量/分辨率/更新频率、材质 ABI 或 shader 行为。Present 转换保留为独立 pass；资源转换和并行录制的既有同步顺序继续生效。

## 可重复测量

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Build.ps1 -Preset debug
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Build.ps1 -Preset release
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Build.ps1 -Preset profile

# 基线目录应在修改代码前保留完整运行时依赖；每次 output 必须为新目录。
python tools/CpuSubmissionBenchmark.py --viewer out/build/debug/bin/hyperion_viewer.exe `
  --baseline out/cpu-submission-20260909/baseline-debug/hyperion_viewer.exe `
  --output out/cpu-submission-20260909/final-debug --scene --warmup 200 --samples 400 --trials 2

# 同一命令换为 Release；默认扫描 0/1/100/300/600/1200 次普通绘制及静态/移动相机。
# --visible --ui 可运行真实可见窗口。图像采集与正常计时应分开。

# 预制 RHI packets：计时只包围 Record(0)，包含验证/保留/录制，排除 BeginFrame、Present 和等待。
out/build/release/bin/submission_benchmark.exe out/NativeRecord.csv 200 400

# 大幅相机运动：10 像素步长，对比默认 0.1；该运行包含可见集合和 CSM 重新规划。
out/build/release/bin/hyperion_viewer.exe --config experiments/Scene.json --hidden --no-ui `
  --no-vsync --frames 4200 --benchmark-warmup 200 --benchmark out/CameraStress.csv --benchmark-camera-step 10
```

基准拒绝未就绪/空场景、错误 draw 数、失败项、CPU/GPU 帧对应缺失以及稳定相机负载中持续创建 PSO/descriptor 的结果。原始 CSV、命令、二进制 SHA-256 和分布位于各次运行的 `Summary.json`。基准默认在两个相同采样数量的轮次中交换 baseline/candidate 次序，所有 Viewer 和 GPU 测试串行运行。

`PrepareSceneSnapshot`、`SharedMaterialRefresh`、`ReuseBatchPlan`、`BuildFreshBatchPlan`、`RetireBatchFamily`、`PrepareRecordingStorage`、`CreateNativeCommandList`、`ResetNativeCommandList` 和 `RecordNativeDraws` 使用既有 `HYP_PERF` 门控。Viewer CSV 增加材质耗时、计划复用、命令列表创建/重置及原生状态绑定计数。并行 RHI scope 之和是工作量，不能当作整帧关键路径耗时。

## 测量条件与证据

2026-09-09，Windows、Ryzen 9 9950X3D、RTX 5080、D3D12 debug layer 开启；1440×900，4 worker、2 RHI，VSync/GUI 关闭，隐藏窗口。Debug 保留 `/Od /Ob0 /RTC1`；Release 保留 `/O2 /Ob2`。性能表使用 Tracy 关闭的 Debug/Release，Profile 仅用于阶段归因。Debug 编译了 RenderDoc 支持，但没有启用捕获。

普通绘制场景使用同一小三角模型的重复实例，关闭剔除和 instance batching，像素/几何量很小，因此主要衡量 CPU 提交。默认 Showcase 为 79 个就绪模型；CSM 保留四个 cascade。该机器上的低几何量结果不代表其他硬件或 GPU 密集场景。

本机证据根目录为 [out/cpu-submission-20260909](../out/cpu-submission-20260909)。`out` 不纳入版本控制。后续重新编译需根据记录的二进制 hash 判断结果是否仍对应当前实现。

## 最终 A/B 结果

每个配置/负载/版本合并两轮各 400 帧，共 800 个正式样本；总计 48,000 个 A/B 样本。均值按等样本合并，P95/P99 由原始样本排序计算，不剔除离群帧。逐轮结果及 hash 保存在 [Debug Summary](../out/cpu-submission-20260909/final-debug/Summary.json) 和 [Release Summary](../out/cpu-submission-20260909/final-release/Summary.json)；汇总为 [FinalAggregate.json](../out/cpu-submission-20260909/FinalAggregate.json)。

### 默认 Showcase / CSM

| 配置 | 相机 | 原版整帧均值 / P95 / P99 (ms) | 新版整帧均值 / P95 / P99 (ms) | 均值减少 |
| --- | --- | ---: | ---: | ---: |
| Debug | 静态 | 18.616 / 20.057 / 21.024 | 15.553 / 17.210 / 18.183 | 16.5% |
| Debug | 移动 | 41.535 / 48.335 / 57.104 | 25.565 / 33.276 / 36.949 | 38.5% |
| Release | 静态 | 2.534 / 4.513 / 5.067 | 2.416 / 4.345 / 4.858 | 4.6% |
| Release | 移动 | 4.405 / 5.568 / 6.059 | 2.772 / 4.130 / 4.784 | 37.1% |

CSM 静态负载为主视图 195 items / 6 draws、四个阴影视图合计 643 items / 24 draws。所有源项均有提交或明确回退，失败项与 D3D12 validation error 为 0。每组候选的预热后命令列表创建数保持恒定。低 draw 数和 Release 静态 CSM 的整帧结果受呈现/调度波动影响明显，不能将单次差值视为稳定收益。

### 普通 draw 数扫描

| 配置 | draw 数 | 相机 | 原版均值 / P95 / P99 (ms) | 新版均值 / P95 / P99 (ms) | 均值减少 |
| --- | ---: | --- | ---: | ---: | ---: |
| Debug | 0 | 静态 | 1.009 / 1.265 / 1.453 | 0.936 / 1.192 / 1.364 | 7.3% |
| Debug | 1 | 静态 | 1.209 / 1.558 / 2.380 | 1.107 / 1.360 / 1.543 | 8.4% |
| Debug | 1 | 移动 | 1.773 / 2.120 / 2.272 | 1.710 / 2.001 / 2.165 | 3.6% |
| Debug | 100 | 静态 | 4.714 / 5.657 / 6.516 | 3.648 / 4.212 / 4.505 | 22.6% |
| Debug | 100 | 移动 | 9.489 / 11.976 / 13.837 | 6.725 / 8.658 / 9.760 | 29.1% |
| Debug | 300 | 静态 | 12.854 / 16.092 / 17.396 | 10.649 / 14.490 / 15.433 | 17.2% |
| Debug | 300 | 移动 | 27.870 / 35.024 / 36.760 | 17.358 / 22.201 / 23.481 | 37.7% |
| Debug | 600 | 静态 | 26.865 / 34.420 / 37.599 | 20.667 / 27.489 / 29.224 | 23.1% |
| Debug | 600 | 移动 | 52.595 / 64.221 / 68.239 | 31.982 / 34.282 / 36.665 | 39.2% |
| Debug | 1200 | 静态 | 52.216 / 54.670 / 56.743 | 42.757 / 44.866 / 46.019 | 18.1% |
| Debug | 1200 | 移动 | 98.030 / 101.587 / 103.730 | 62.990 / 65.651 / 67.772 | 35.7% |
| Release | 0 | 静态 | 0.452 / 0.992 / 2.068 | 0.384 / 0.832 / 2.905 | 15.0% |
| Release | 1 | 静态 | 0.888 / 3.655 / 4.252 | 0.845 / 3.688 / 4.607 | 4.8% |
| Release | 1 | 移动 | 1.562 / 4.244 / 5.215 | 1.612 / 4.487 / 8.503 | -3.2% |
| Release | 100 | 静态 | 1.806 / 4.170 / 4.784 | 1.726 / 4.228 / 4.775 | 4.5% |
| Release | 100 | 移动 | 1.508 / 3.368 / 4.673 | 0.942 / 1.230 / 1.604 | 37.5% |
| Release | 300 | 静态 | 1.842 / 2.214 / 2.541 | 1.424 / 1.751 / 1.999 | 22.7% |
| Release | 300 | 移动 | 3.113 / 3.956 / 4.370 | 2.055 / 2.645 / 2.894 | 34.0% |
| Release | 600 | 静态 | 3.570 / 4.234 / 4.669 | 2.571 / 3.178 / 3.510 | 28.0% |
| Release | 600 | 移动 | 7.154 / 8.986 / 10.207 | 4.483 / 5.680 / 9.279 | 37.3% |
| Release | 1200 | 静态 | 8.642 / 10.803 / 11.634 | 7.525 / 9.418 / 12.334 | 12.9% |
| Release | 1200 | 移动 | 18.499 / 21.235 / 22.276 | 8.781 / 11.378 / 12.696 | 52.5% |

### 独立原生录制

单位 ms；该基准保留借用式 Record 的输入快照语义，复用相同 pipeline/geometry/bindings。它包含完整验证、引用保留和命令录制，不是单个 DrawIndexedInstanced 的裸 API 时间。每个单元格为原版 → 新版均值。

| draw 数 | Debug | Release |
| ---: | ---: | ---: |
| 0 | 0.0549 → 0.0484 | 0.0394 → 0.0477 |
| 1 | 0.0813 → 0.0752 | 0.0565 → 0.0601 |
| 100 | 0.4440 → 0.2640 | 0.2478 → 0.1704 |
| 300 | 1.1105 → 0.6418 | 0.6150 → 0.4112 |
| 600 | 2.0868 → 1.1959 | 1.1334 → 0.7596 |
| 1200 | 4.1439 → 2.2279 | 2.2167 → 1.4069 |

600 次录制的 Debug 均值减少 42.7%，Release 减少 33.0%。全扫描线性拟合的边际成本，Debug 为 3.383 → 1.809 μs/draw，Release 为 1.802 → 1.132 μs/draw；该拟合仍包含每项验证和保留成本。新实现每次基准仅创建 4 个原生列表，随后重置 7196 次。原始 [native-final/Summary.json](../out/cpu-submission-20260909/native-final/Summary.json) 和 [NativeFinalAggregate.json](../out/cpu-submission-20260909/NativeFinalAggregate.json) 保存完整分布与二进制 hash。

### 阶段归因

以下为同参数 Profile/Tracy 的 200 帧采样，单位为每帧 ms。每行都是自身的 inclusive scope，不应把嵌套项相加；并行 RHI 项是各线程工作量之和。正式整帧收益以上方 Tracy 关闭的数据为准。

| 移动负载 | scope | 原版 | 新版 |
| --- | --- | ---: | ---: |
| 600 普通 draw | ForwardPipeline | 4.900 | 2.549 |
| 600 普通 draw | PrepareMaterials | 3.150 | 1.143 |
| 600 普通 draw | PlanBatches | 0.031 | 0.027 |
| 600 普通 draw | PrepareDraws | 1.115 | 0.911 |
| 600 普通 draw | CompileRenderGraph | 0.077 | 0.003 |
| 600 普通 draw | ValidateDraws | 0.174 | 0.096 |
| 600 普通 draw | RecordCommands | 1.827 | 1.280 |
| Showcase CSM | ForwardPipeline | 3.993 | 2.155 |
| Showcase CSM | PrepareMaterials | 1.884 | 1.120 |
| Showcase CSM | PlanBatches | 1.178 | 0.237 |
| Showcase CSM | PrepareDraws | 0.220 | 0.199 |
| Showcase CSM | CompileRenderGraph | 0.011 | 0.005 |
| Showcase CSM | ValidateDraws | 0.018 | 0.011 |
| Showcase CSM | RecordCommands | 0.648 | 0.601 |

五视图 CSM 的材质求值工作减少约 40.6%，批次规划减少约 79.8%；普通 600 draw 的图编译由约 0.077 ms 降至 0.0025 ms。候选 CSM 的场景快照准备约 0.171 ms/帧、收集约 0.141 ms/帧；这些成本仍存在。独立 Present transition 保留：它承载最后的颜色/深度状态转换，当前数据没有证明应为小规模调度收益引入新的转换时序契约。全部 scope/操作计数见 [FinalTraceSummary.json](../out/cpu-submission-20260909/FinalTraceSummary.json)。

### 仍未达到的工程目标

- Debug 600 静态普通 draw 的 P95 为 27.489 ms，尚未达到 16.67 ms。优化后每项准备和数据管理的 Debug 成本仍然显著，不能据此声称稳定 60 FPS。
- Release 移动场景开启 CSM 的额外 pipeline 准备从 2.223 ms 降到 1.249 ms，仍高于 1 ms 目标。这里比较同一相机模式的 CSM 开/关，未用 CSM 静态/移动差值替代。
- 0/1 draw 和部分静态整帧结果波动较大；完整扫描保留收益较小或负收益的行。没有更改 Debug 优化或降低阴影工作量来掩盖差距。

## 图像与持续运行

四组 Forward/CSM × 静态/移动的最终对照均逐像素相同，未使用容差。最初移动 Forward 对照出现 1 个像素差异；同一个原版的重复运行也出现相同位置的差异，随后原版/新版及两侧关闭合批的图像均完全一致。首次失败和自对照保留于 [PixelRepeatDiagnosis.json](../out/cpu-submission-20260909/delivery/PixelRepeatDiagnosis.json)，没有删除失败证据。最终四组对照及命令见 [delivery-verified/Summary.json](../out/cpu-submission-20260909/delivery-verified/Summary.json)。

连续运动每组预热 200 帧、采样 4000 帧，共 100 个往返周期；默认 0.1 像素步长与 10 像素步长各测 Debug/Release，总计 16,000 个采样帧。

| 配置 / 每帧步长 | 整帧均值 | P95 | P99 |
| --- | ---: | ---: | ---: |
| debug-step0.1 | 24.231 | 28.760 | 30.876 |
| debug-step10 | 35.021 | 37.624 | 40.510 |
| release-step0.1 | 3.022 | 4.676 | 5.838 |
| release-step10 | 3.905 | 4.987 | 5.576 |

末尾 1000 帧的 PSO 创建累计为 4、descriptor 分配累计为 136、原生命令列表创建累计为 12，均无增长。小幅运动 GPU 分配区间宽度为 64 KiB，大幅运动为 448 KiB；这是 GPU 分配/缓存稳定性证据，不代表对全部 CPU/第三方分配进行了统计。所有帧都有匹配的 GPU 提交，模型就绪、失败项与验证状态均正常。

实际可见窗口另测 1000 帧（800 个采样帧），GUI 开启，输出为 1440×900，并按进程确认窗口可见。Win32 helper 的客户端坐标读数为 960×600，截图为实际渲染像素尺寸。该组使用不同 GUI 条件，不与隐藏窗口无 GUI 基准混合。

| 可见窗口 / GUI | 整帧均值 | P95 | P99 |
| --- | ---: | ---: | ---: |
| [visible-ui-debug](../out/cpu-submission-20260909/delivery-verified/visible-ui-debug.png) | 33.702 | 39.458 | 43.176 |
| [visible-ui-release](../out/cpu-submission-20260909/delivery-verified/visible-ui-release.png) | 4.020 | 5.256 | 5.928 |

已检查两张 UI 截图：79/79 模型就绪、0 failed、D3D12 validation enabled / errors 0、四个 2048 cascade 每帧渲染，场景与阴影面板可见。

## 正确性检查

Debug 的 48 项、Release 的 43 项 CTest 均通过；Profile 的 42 项常规测试和修正为逐视图计数后的真实 Tracy 验收通过。覆盖任意材质作用域/混合依赖、旧帧值、instance 分组与自定义策略、交替原生状态的像素、图数据所有权、命令列表复用、失败 Present/cancel、窗口/GUI/RenderDoc 和 CSM。

新增状态缓存测试在同一 PSO 下交替 VB/IB/scissor、重复相同绘制，并跨 frame slot 检查颜色与实际绑定次数。持有旧录制的测试验证其元数据和原生列表不被后续帧修改。完整命名/格式、模块边界及 `git diff --check` 通过。

测量和图像/持续运行详情见上述结果及本机 evidence；OpenSpec change 保持开发完成状态，归档和提交单独执行。

两个 CPU proposal 的后续独立审计修复与验证见 [RendererCpuAudit.md](RendererCpuAudit.md)。上述历史性能数据不替代修复版本的复测。
