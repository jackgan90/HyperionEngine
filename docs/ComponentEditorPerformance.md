# Component 与编辑器迭代性能对比

2026-09-16，Windows / D3D12 / Sponza。此文记录本次未提交工作树的测量结果，不是跨硬件性能保证。

下方原始主表对应组件化与自动展开阶段，测量时 Transform 仍显示矩阵。后续 TRS 显示与整模型引用修正分别记录于 [验收记录](../openspec/changes/archive/2026-09-17-component-scene-editor/verification.md)；本页末尾追加最终整模型版本的测量，不回写历史样本。旧版程序另保存在 `out/TransformInspector/Baseline`。

组件化及 primitive 对象拆分没有增加场景绘制数量或重复创建整套 GPU 资源。初版出现的 Debug Viewer 移动相机回退已定位并修正：高频组件查询改用带边界检查的索引遍历，避免反复注册 MSVC Debug 迭代器。编译优化、CRT、STL 调试检查和 D3D12 debug layer 均未降低。最终数据如下。

## 条件与复现

- CPU：AMD Ryzen 9 9950X3D，16 核 / 32 逻辑处理器；GPU：NVIDIA GeForce RTX 5080；Windows 11 10.0.26200。
- 基线引擎：`eb40123683ef18ef8e3e3bed4cfa3ff63292a1d4`；基线内容：`05f5382c2cc59d0fc90728457727e4fbce331e23`。冻结旧程序后才开始语义修改。基线 Editor 添加与新版相同口径的可选测量；补丁随原始证据保留。
- 最终源码指纹：`b7a2d748fd227b14c5e14c810b3c225f5c0537b891a418757b8e860ceadd5873`。各程序 SHA-256、场景哈希与版本见 [数据文件](ComponentEditorPerformanceData.json)。
- Deferred、reversed-Z、四级 CSM、聚簇局部光、曝光 2.5；VSync 关闭、窗口隐藏、UI 开启；两版 D3D12 验证均开启。
- Editor 视口 1181×602，资源就绪后预热 120 帧；Viewer 输出 1440×728，预热 6000 帧。每格 3 个独立进程，各采样 300 个已就绪且非空帧。移动输入路径固定；测试与编译结束后串行采样。
- 表中均值合并 900 帧；P95/P99 使用这 900 帧的 nearest-rank 分位数，不是逐轮分位数的平均。逐轮均值和所有 CSV 哈希在数据文件中。
- Editor `frame_ms` 是含 Render/RHI 等待的宿主帧墙钟时间；Viewer 是 CPU 流水线吞吐口径。只在同一宿主内比较。`preparation_ms` 包含在渲染等待中，不能再次相加；GPU pass 合计不包含整个宿主、GUI 或 Present。

```powershell
python tools/ComponentEditorBenchmark.py --debug out/build/debug/bin --release out/build/release/bin --output out/ComponentEditor/Candidate/Measurements
python tools/ComponentEditorBenchmark.py --debug out/build/debug/bin --release out/build/release/bin --hosts editor --editor-outliner collapsed --output out/ComponentEditor/Candidate/Collapsed
```

基线复现需使用冻结程序及旧场景根；当前 Sponza 已升级，旧程序不能直接读取新 schema。原始测量命令逐次保存在 Results.json。最终跑序优先验证 Debug Viewer 移动回退，其余参数和每格重复数不变。

## 主要结果

单位 ms；正变化表示耗时增加。

| 构建 / 宿主 / 相机 | 基线均值 | 新版均值 | 变化 | P95 基线 → 新版 | P99 基线 → 新版 |
|---|---:|---:|---:|---:|---:|
| Release / Editor / 静止 | 4.900 | 4.422 | -9.8% | 5.823 → 5.226 | 6.380 → 5.812 |
| Release / Editor / 移动 | 7.082 | 6.698 | -5.4% | 8.320 → 7.789 | 8.876 → 8.655 |
| Release / Viewer / 静止 | 5.099 | 5.040 | -1.2% | 5.969 → 5.840 | 6.750 → 6.287 |
| Release / Viewer / 移动 | 6.328 | 6.253 | -1.2% | 7.389 → 7.235 | 8.145 → 7.913 |
| Debug / Editor / 静止 | 12.077 | 12.213 | +1.1% | 13.569 → 13.449 | 14.487 → 14.222 |
| Debug / Editor / 移动 | 27.557 | 28.270 | +2.6% | 30.571 → 30.994 | 32.087 → 32.592 |
| Debug / Viewer / 静止 | 14.830 | 15.206 | +2.5% | 16.470 → 16.358 | 18.304 → 17.612 |
| Debug / Viewer / 移动 | 24.628 | 24.953 | +1.3% | 27.280 → 27.710 | 29.435 → 29.836 |

不要把小幅差异全部归因于代码：这里没有锁定 CPU/GPU 频率，也不是交错随机跑序。下方的相邻时段基线复测和关闭 UI 配对用于检查这种限制；单次诊断不能替代上表的三轮数据。

Release Editor 的总帧时间下降主要出现在 Render wait，GUI 本身耗时增加；不能将这个结果解释为组件 Inspector 比旧只读面板更便宜。Debug Editor 的 GUI 增量约 0.7 ms，收起 Outliner 可减少约 0.45 ms。当前观察到的 Debug 总帧均值增量为 1.1%–2.6%，Release Viewer 变化约 -1.2%。

## Editor 成本分解与 Outliner

Scene 是两次场景 Tick 及相机处理；GUI 是界面构建；Render wait 含场景、GUI 合成及等待。数值为均值 ms。

| 构建 / 相机 | Scene 基线 → 新版 | GUI 基线 → 新版 | Render wait 基线 → 新版 | 新版收起 Outliner：帧 / GUI |
|---|---:|---:|---:|---:|
| Release / static | 0.004 → 0.018 | 0.120 → 0.177 | 4.776 → 4.227 | 4.359 / 0.147 |
| Release / moving | 0.028 → 0.026 | 0.134 → 0.203 | 6.919 → 6.469 | 6.612 / 0.163 |
| Debug / static | 0.045 → 0.101 | 0.592 → 1.284 | 11.436 → 10.824 | 11.803 / 0.827 |
| Debug / moving | 0.217 → 0.125 | 0.611 → 1.289 | 26.724 → 26.851 | 28.058 / 0.853 |

对象从 7 个增至 111 个；组件 Inspector 和层级条目是新增的实际功能负载。收起对照只改变 Outliner 的默认展开状态，Inspector 仍显示同一网格组件，Render diagnostics 默认收起。视口导航使用独立相机，不写入场景相机或撤销历史。

## 回退定位与相邻控制

初版 Debug Viewer 移动的两次完整采样为 41.683 / 41.841 ms，而绘制、缓存复用和上传工作量与基线相同。高频 `Slot<T>` 的 vector 迭代器在 MSVC Debug 下使用进程级 `_LOCK_DEBUG`；结合本机 STL 实现及此次单项调整前后的重复实测，判断 Main 查询引发的迭代器注册与锁竞争是主要回退来源。未单独采集锁等待调用栈，因此没有给出锁等待占比。索引遍历保留 vector 的 Debug 边界检查，只移除该查询路径的迭代器注册，Editor 的串行帧也减少了查询成本。

| 相邻时段重新运行的冻结基线 | 均值 ms |
|---|---:|
| Debug Viewer 移动 | 23.890 |
| release-editor-static-1 | 4.803 |
| release-editor-moving-1 | 7.145 |
| debug-editor-static-1 | 12.043 |
| release-viewer-static-1 | 5.072 |

关闭 Viewer UI 的诊断每侧一个独立进程、6000 帧预热、300 帧采样；同时保存最终帧进行像素比较。

| 关闭 UI 的配对 | 基线均值 | 新版均值 |
|---|---:|---:|
| Release / static | 4.017 | 3.970 |
| Release / moving | 5.041 | 5.065 |
| Debug / moving | 22.531 | 22.853 |

## 工作量、内存、加载与保存

主视图保持 79 个可见项 / 79 次绘制，CSM 保持 404 次绘制，descriptor_allocations 计数保持 273；没有把逻辑对象拆分成重复的几何和纹理上传。以下为 Viewer 三轮均值；RHI 分配字节是设备分配器报告值，不是整个进程的显存峰值。

| 构建 / 相机 | GPU pass 合计 ms：基线 → 新版 | RHI 分配 MiB：基线 → 新版 |
|---|---:|---:|
| Release / static | 0.281 → 0.282 | 502.90 → 502.90 |
| Release / moving | 0.290 → 0.291 | 502.95 → 502.95 |
| Debug / static | 0.307 → 0.300 | 502.90 → 502.90 |
| Debug / moving | 0.307 → 0.307 | 502.95 → 502.95 |

下表是静止模式三次进程峰值的中位数（MiB）。工作集包含整个进程；private bytes 每 50 ms 采样，含设备、驱动、加载缓存与分配器预留，不能直接当作组件大小或泄漏统计。完整范围见 JSON。

| 构建 / 宿主 | 峰值工作集：基线 → 新版 | private 峰值：基线 → 新版 |
|---|---:|---:|
| Release / Editor | 568.9 → 571.4 | 1194.9 → 1198.6 |
| Release / Viewer | 586.2 → 590.2 | 1213.5 → 1254.7 |
| Debug / Editor | 645.8 → 649.7 | 1277.5 → 1274.3 |
| Debug / Viewer | 675.8 → 680.1 | 1317.1 → 1307.3 |

Editor 启动到首个就绪帧包括设备、catalog、资源准备与上传，不是纯场景解码。每构建汇总静止和移动共六次进程的启动值。保存是新建目标的 Save As，包含 Main 快照/验证、异步写入及完成轮询；每构建三次，并实际重载验证。旧 Editor 没有保存功能，因此保存基线为 N/A。

| 构建 | 启动到就绪均值 ms：基线 → 新版 | 新版 Save As 均值 [最小, 最大] ms |
|---|---:|---:|
| Release | 1814.2 → 1792.2 | 21.1 [20.6, 21.5] |
| Debug | 5598.4 → 5731.5 | 248.3 [246.8, 250.9] |

Sponza 场景文件由 38,164 增至 205,098 字节；模型由 12,449,676 增至 12,452,055 字节，增量主要是对象/组件与子资源身份信息。新模型作为一个不可变版本发布，旧版本继续保留，因此仓库磁盘会额外保留约 12.45 MB；这不表示运行时上传两份模型。场景/模型资产 ID 保持不变，可重建配方与 catalog 已更新，重复导入为 0 写入。

## 正确性与证据

- Release 全量 73/73 通过；Debug 全量及针对初次失败项的复测覆盖 74/74；索引查询调整后，两种配置各 9/9 相关测试通过。格式、命名、边界与 OpenSpec 检查通过。详见 [验收记录](../openspec/changes/archive/2026-09-17-component-scene-editor/verification.md)。
- Release 静止、Release 移动、Debug 移动三组冻结基线/新版 Viewer 最终帧的 RGB 像素完全一致，最大通道差为 0，详见数据文件 `parity`。编辑器真实点击覆盖独立 primitive 可见性、Apply、Undo/Redo、保存期间的新修改、重载和保存失败保护。最终干净文档截图记录 111 个对象、79 次主视图绘制及 0 个 GPU 验证错误。
- 可移植汇总：[ComponentEditorPerformanceData.json](ComponentEditorPerformanceData.json)。它保留逐轮均值、池化分位数、内存范围、程序/场景身份及原始文件 SHA-256。
- 本机原始证据：`out/ComponentEditor/Baseline/Measurements`、`Candidate/Measurements`、`Candidate/Collapsed`、`Candidate/Save`、`Candidate/Isolation`、`Candidate/BaselineSanity`；打包为 `out/ComponentEditor/PerformanceEvidence.zip`。`out/` 不纳入版本控制，新检出不保证存在。
- 初版程序与调查数据保留在 `Candidate/Initial`；其中收起 Outliner / 保存采样与构建重叠，已排除最终结论。最终所有测量均在构建、检查和 GPU 回归停止后运行。

此结果只覆盖本机 D3D12 Sponza、当前 UI 与确定性相机路径。它验证了 103 个独立网格对象仍共享渲染资源、最明显的 Debug 回退已经消除；更大对象数量、编辑操作频率和其他后端需要分别测量。

## 2026-09-17：最终整模型引用版本

这一阶段包含 TRS Inspector 和整模型引用修正。Sponza 恢复为 7 个场景对象、1 个模型组件；103 个 primitive 保留在共享模型资产中。此前的 111 对象数据继续作为历史记录。

源码指纹 `60eb40547a308370e44bed9430010dbb306a38d5dd7e94572b4e5b6568ed3001`；四个程序和场景的 SHA-256、逐轮均值、900 帧池化分位数及内存范围见 [最终数据](WholeModelReferencePerformanceData.json)。主矩阵仍为每格三轮、每轮 300 个就绪帧，Editor 预热 120 个就绪帧，Viewer 预热 6,000 帧；尺寸、隐藏窗口、UI、相机路径及验证设置沿用原报告。编译和回归测试结束后才开始测量。

单位 ms。历史基线与本次数据来自不同测量时段，百分比不能单独解释为代码变化的成本。

| 构建 / 宿主 / 相机 | 历史基线均值 | 最终均值 | 变化 | 最终 P95 | 最终 P99 |
|---|---:|---:|---:|---:|---:|
| release-editor-static | 4.900 | 5.908 | +20.6% | 7.362 | 9.693 |
| release-editor-moving | 7.082 | 8.205 | +15.9% | 10.413 | 11.656 |
| release-viewer-static | 5.099 | 6.227 | +22.1% | 7.686 | 8.536 |
| release-viewer-moving | 6.328 | 7.311 | +15.5% | 9.282 | 11.005 |
| debug-editor-static | 12.077 | 14.384 | +19.1% | 17.842 | 20.469 |
| debug-editor-moving | 27.557 | 36.445 | +32.3% | 43.562 | 47.634 |
| debug-viewer-static | 14.830 | 17.460 | +17.7% | 21.438 | 23.249 |
| debug-viewer-moving | 24.628 | 31.096 | +26.3% | 36.409 | 40.228 |

为检查时段漂移，追加同一时段的静止 Editor 对照：原始组件化前程序（original）、刚修正前的 TRS + 展开程序（expanded）、最终整模型程序（compact）。三轮轮换执行顺序，主视图均为 79 个可见项 / 79 次绘制，Outliner/Inspector 保留各版本实际功能。以下不是 Viewer 或移动相机的对照结论。

| 构建 / 阶段 | 帧均值 | GUI 均值 | Scene 均值 | Render wait 均值 |
|---|---:|---:|---:|---:|
| release / original | 5.901 | 0.138 | 0.005 | 5.758 |
| release / expanded | 5.428 | 0.217 | 0.023 | 5.187 |
| release / compact | 5.949 | 0.171 | 0.007 | 5.770 |
| debug / original | 15.355 | 0.746 | 0.056 | 14.549 |
| debug / expanded | 15.322 | 1.589 | 0.123 | 13.606 |
| debug / compact | 15.359 | 0.931 | 0.065 | 14.358 |

同期静止 Editor 中，最终版与原始版总帧时间接近：Release 约 +0.8%，Debug 约 +0.03%。相对于修正前的展开版，GUI 耗时下降约 21% / 41%，Scene 耗时也减少；但 Release 总帧时间仍比展开版高约 9.6%，差异主要在 Render wait。结构简化没有直接转化为总帧时间下降。冻结旧程序在此次时段也明显慢于历史数据，说明跨时段差异确实存在；本次没有同一时段的 Viewer/移动相机配对，不能据此排除这些情况的回退，也不能将主表的全部差异归因于代码。


以下内存值为每构建/宿主六次进程的峰值均值，MiB；包含加载和运行全过程，不是场景对象本身的占用。

| 构建 / 宿主 | 峰值工作集 | private 峰值 |
|---|---:|---:|
| release / editor | 572.6 | 1219.5 |
| release / viewer | 589.8 | 1275.0 |
| debug / editor | 647.4 | 1430.8 |
| debug / viewer | 679.1 | 1395.1 |

Editor 启动到就绪包含设备、catalog、资产准备与上传，并非纯解码耗时：release 2683.0 ms；debug 7999.6 ms。

此次隔离验收的一次 Release Save As 为 27.47 ms，并实际重载；它是功能验收样本，不能与历史三轮保存均值作性能结论。原始 Editor 无保存功能，仍为 N/A。

场景文件从展开版 205,098 字节降为 39,472 字节；原始组件化前是 38,164 字节。模型文件与本次修正前相同，全部 123 个已有不可变依赖文件和资产库索引逐字节未变。根资产 ID 保持不变，场景配方与 catalog 已更新，重复导入为零写入。

Debug / Release 各 11 项相关回归通过。迁移前后相同相机的 Viewer 截图逐像素一致；干净 Editor 预览为 7 对象、79 主视图绘制、零验证错误。真实输入验证覆盖 TRS、可见性、撤销重做、异步保存和重载。

原始证据位于 `out/WholeModelReference/Measurements`、`Controls`，并打包为 `PerformanceEvidence.zip`；这些本机产物不纳入版本控制。复测命令为原报告的 `ComponentEditorBenchmark.py`，输出目录改为 `out/WholeModelReference/Measurements`；相邻对照使用同目录下 `MeasureControls.py`。
