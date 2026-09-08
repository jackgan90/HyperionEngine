# Instance 批次性能验证

2026-09-08，在同一个 Release Viewer 中切换 instance batching，Showcase 静态相机由 194 个 scene draw 减为 5 个，三轮平均帧时间降低 **31.5%**；移动相机由 194–195 个减为 5 个，平均帧时间降低 **22.1%**。图像一致性由独立的静态对照和移动相机集成测试验证。

收益主要来自 CPU 命令录制和 RHI 准备。当前场景没有测得 GPU 时间改善，P95 也没有稳定改善；这些结果不代表更复杂场景或其他硬件的性能保证。接口与限制见 [InstanceBatching.md](InstanceBatching.md)。

## 测量条件

- Windows、NVIDIA GeForce RTX 5080、D3D12；两侧均启用 D3D12 debug layer。
- `experiments/Scene.json` / `assets/Scenes/Showcase.json`，1440 × 900，GUI 开启，隐藏窗口，VSync 关闭，78/78 模型就绪，validation error 和 failed item 均为 0。
- Release，Tracy 与 RenderDoc 均关闭；普通与 instance 路径使用同一个 exe，仅增加或移除 `--no-instance-batching`。
- 每次预热 240 帧、采样 600 帧；静态/移动各三轮，执行顺序为 off/on、on/off、off/on，所有进程串行。每种相机/开关组合共 1800 个采样帧。
- 测量 exe SHA-256：`0b9e5607972a81024110ec6f96baa3e72391f6dca63a4207309d32ea6b5577cc`。

原始 [Summary.json](../out/instance-performance/Release/Summary.json) 保存每次命令、配置和统计，同目录保存全部 CSV、运行日志及 CMakeCache。`out` 为本机证据目录，不纳入版本控制。这组测量对应上述 exe 快照。随后补充了异常路径打包统计，并在审计中修复可选 native 准备隔离、Viewer 回退诊断及 CLI 状态显示；合批兼容性、记录打包和复用算法保持一致，但本表不是审计后 exe 的重新测量。

## 同 exe 帧时间

单位为 ms。总体均值为三轮等样本均值；逐轮 P95 保留观测波动，未合并成单个百分位数。

| 相机 | 开关 | 总体均值 | 第 1 / 2 / 3 轮均值 | 第 1 / 2 / 3 轮 P95 | 实际 scene draw/帧 |
| --- | --- | ---: | --- | --- | --- |
| 静态 | off | 2.4867 | 2.3645 / 2.6140 / 2.4816 | 2.8760 / 3.9101 / 3.2549 | 194 |
| 静态 | on | 1.7029 | 1.7296 / 1.5809 / 1.7981 | 3.4641 / 2.7704 / 3.6771 | 5 |
| 移动 | off | 2.7650 | 2.8110 / 2.7382 / 2.7458 | 3.8311 / 3.2877 / 3.2869 | 194–195 |
| 移动 | on | 2.1540 | 2.1976 / 2.1376 / 2.1269 | 3.3207 / 3.4982 / 3.4708 | 5 |

两侧 visible item 完全一致：静态为 194，移动均值 194.85。开启后所有可见 item 由 5 个 instance draw 覆盖，无重复普通 draw；draw 数减少约 97.4%。

| 同一 Release CSV 指标，逐帧均值 | 静态 off | 静态 on | 移动 off | 移动 on |
| --- | ---: | ---: | ---: | ---: |
| Render 规划 ms | 0.01156 | 0.01185 | 0.01133 | 0.30777 |
| RHI 准备 ms | 0.06869 | 0.02016 | 0.20645 | 0.03509 |
| 复用 chunk/帧 | 0 | 5 | 0 | 4.90167 |
| 重建 chunk/帧 | 0 | 0 | 0 | 0.09833 |
| 新上传 instance 字节/帧 | 0 | 0 | 0 | 1050.4 |

静态采样窗口中实例数据没有重新上传。移动时可见成员变化只重建受影响的 chunk；共享 View 常量变化不要求重新上传 Object/Surface 记录。移动相机需要重新检查有效兼容性，规划成本增大，但端到端平均帧时间仍下降。字节统计为实例记录 payload，不包括常量 slice 对齐填充，也不包括共享 View/Scene 常量。

## CPU/GPU trace

使用另外的优化 Profile 构建开启 Tracy detail + GPU，每个 case 预热 240 帧、记录 300 帧。四次采集的 Metadata 均为 `passed`。这些带 instrumentation 的 scope 数据只用于解释成本来源，不与上面的 Release 帧时间混合。

| 累计 scope 时间 / 采样帧，ms | 静态 off | 静态 on | 移动 off | 移动 on |
| --- | ---: | ---: | ---: | ---: |
| CPU RecordCommands | 0.92968 | 0.29849 | 0.94455 | 0.31140 |
| CPU PrepareDraws | 0.09039 | 0.03131 | 0.27182 | 0.04987 |
| CPU PlanBatches | 0.01136 | 0.01156 | 0.01215 | 0.30525 |
| GPU GraphicsPass 总计 | 0.03097 | 0.03160 | 0.03005 | 0.03326 |

CPU 数值按 `Zones.csv` 的 `total_ns / 300 / 1e6` 计算，为 scope 累计墙钟时间，可能嵌套或跨线程，不能相加解释为帧关键路径。`DrawMaterial` 调用数静态从 58,200 减至 1,500，移动从 58,455 减至 1,500。

每次 `Gpu.csv` 包含 1,200 个 GraphicsPass timestamp（每帧 4 个），上表为所有 `GPU execution time` 求和后除以 300 帧。它包含清屏和 GUI，不是 scene-only 时间。GPU 总量略增；这个小场景中的实例化收益集中在 CPU 提交成本。

原始 trace、逐事件导出、GPU CSV 和运行日志：

- [静态 off](../out/instance-performance/ProfileStaticOff/Metadata.json) / [静态 on](../out/instance-performance/ProfileStaticOn/Metadata.json)
- [移动 off](../out/instance-performance/ProfileMovingOff/Metadata.json) / [移动 on](../out/instance-performance/ProfileMovingOn/Metadata.json)

## 复现

在仓库根目录运行；每次指定一个尚不存在的输出目录。构建选项会写入相应 preset 的 CMake cache。

```powershell
./tools/Build.ps1 -Preset release -NoTracy -NoRenderDoc
python tools/InstanceBenchmark.py --viewer out/build/release/bin/hyperion_viewer.exe --output out/instance-performance/ReleaseRepeat --warmup 240 --frames 600 --trials 3

./tools/Build.ps1 -Preset profile -NoRenderDoc
python tools/Profile.py --viewer out/build/profile/bin/hyperion_viewer.exe --out out/instance-performance/ProfileStaticOffRepeat --mode detail-gpu --static --no-instance-batching --warmup 240 --frames 300 --no-build
python tools/Profile.py --viewer out/build/profile/bin/hyperion_viewer.exe --out out/instance-performance/ProfileStaticOnRepeat --mode detail-gpu --static --warmup 240 --frames 300 --no-build
python tools/Profile.py --viewer out/build/profile/bin/hyperion_viewer.exe --out out/instance-performance/ProfileMovingOffRepeat --mode detail-gpu --no-instance-batching --warmup 240 --frames 300 --no-build
python tools/Profile.py --viewer out/build/profile/bin/hyperion_viewer.exe --out out/instance-performance/ProfileMovingOnRepeat --mode detail-gpu --warmup 240 --frames 300 --no-build
```

Tracy collector/exporter 需先按 [Profiling.md](Profiling.md) 构建。正确性检查使用 `instance_batching` 的自定义 shader 真 GPU 对照，以及 `scene_moving_camera` 的同相机、同可见集合和精确 PNG 对照；性能采样保留 GUI，因此不直接拿两侧包含动态统计文字的帧图做像素比较。
