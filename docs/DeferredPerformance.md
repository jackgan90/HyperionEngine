# Deferred 性能与验收报告

测量日期：2026-09-11。本次实现默认启用传统 raster Deferred，并保留相同线性 HDR / Reinhard / sRGB 输出的 Forward 作为有效 A/B 参照。

在本机的单方向光工作负载中，Deferred 紧凑布局增加了 GPU 通道总时间：16 个匹配场景组合的两轮合并均值增量为 **0.0105–0.0354 ms**。这些场景没有获得 GPU 加速；额外 GBuffer 带宽、全屏光照和 CPU pass 准备构成了实际成本。结果不代表未来多光源场景。

## 条件与可重复性

- Windows，AMD Ryzen 9 9950X3D，NVIDIA GeForce RTX 5080，D3D12；Release，D3D12 debug layer 保持 enabled，Tracy OFF，RenderDoc 编译开关 ON、测量时未启用捕获。
- 同一审计前测量可执行文件，SHA-256：4e1030663cfab329eeda87c37b328f4ff20c6a9b5c27ef191a4b82d93f013d50。
- Scene 使用 79 个模型的 Showcase 场景，主视图 190–210 个可见 item（随宽高比/镜头变化）；Model 使用 Showcase.gltf，主视图 4 个可见 item。相机输入、光照、实例化与输出参数按组匹配。ModelViewer 已补齐投影对应的相机元数据，CSM 开启组确实执行阴影绘制。
- 2 场景 × 2 分辨率 × 2 相机状态 × 2 CSM 状态 × 3 管线/布局 × 2 轮，共 96 次串行运行、48,000 个采样帧。每次预热 300 帧、采样 500 帧；第二轮反转三种模式的顺序。
- 隐藏窗口、无 GUI、VSync OFF、曝光 1、实例化 ON，Main→Render / Render→RHI lead 为 1/1。CSM 开启时使用四级 2048² 深度图。图像读回、编译和其他本任务 GPU 测试未与计时并行运行。未锁定硬件频率，短区间结果存在调度和频率噪声。
- 每组验证精确的 CPU 帧 300..799 与 GPU 提交 301..800 对应，拒绝丢失/重复 timing、未就绪/空场景、failed item、PSO/descriptor 持续增长；所有组验证层错误为 0。还逐帧核对每组三种管线的可见项、绘制、实例化和阴影数量，32 个正式对比组与 4 个历史条件对比组全部一致。

    python tools/MeasureDeferred.py --output out/DeferredPerformance20260911 --samples 500 --warmup 300 --repeats 2

表格从每组两轮原始 CSV 合并 1,000 帧计算；P95 是合并样本分位数，没有平均两轮分位数。完整 CPU、各 pass GPU 和计数器聚合保存在 [DeferredPerformanceData.json](DeferredPerformanceData.json)，原始命令、逐帧 CSV 和日志位于本机 [Summary.json](../out/DeferredPerformance20260911/Summary.json)。

## GPU 通道总时间

单位 ms，均值。Total 是本次记录的 GPU pass 区间之和，包含空兼容/透明 pass 与转换等间隔；它不是 GPU 整帧墙钟时间、Present 延迟或 FPS。Compact 与 High 的打包语义相同，分别为 24 / 32 bytes/pixel GBuffer。

| 场景 | 分辨率 | 镜头 | CSM | HDR Forward | Deferred Compact | Deferred High | Compact 增量 |
|---|---|---|---|---:|---:|---:|---:|
| Scene | 1280×720 | 静止 | 关 | 0.0410 | 0.0563 | 0.0620 | +0.0153 |
| Scene | 1280×720 | 静止 | 开 | 0.1088 | 0.1207 | 0.1197 | +0.0119 |
| Scene | 1280×720 | 移动 | 关 | 0.0387 | 0.0562 | 0.0598 | +0.0175 |
| Scene | 1280×720 | 移动 | 开 | 0.1041 | 0.1196 | 0.1260 | +0.0155 |
| Scene | 1920×1080 | 静止 | 关 | 0.0522 | 0.0876 | 0.0935 | +0.0354 |
| Scene | 1920×1080 | 静止 | 开 | 0.1298 | 0.1640 | 0.1691 | +0.0343 |
| Scene | 1920×1080 | 移动 | 关 | 0.0538 | 0.0862 | 0.0933 | +0.0324 |
| Scene | 1920×1080 | 移动 | 开 | 0.1338 | 0.1603 | 0.1703 | +0.0265 |
| Model | 1280×720 | 静止 | 关 | 0.0201 | 0.0306 | 0.0328 | +0.0105 |
| Model | 1280×720 | 静止 | 开 | 0.0442 | 0.0593 | 0.0563 | +0.0152 |
| Model | 1280×720 | 移动 | 关 | 0.0194 | 0.0305 | 0.0310 | +0.0111 |
| Model | 1280×720 | 移动 | 开 | 0.0444 | 0.0565 | 0.0571 | +0.0121 |
| Model | 1920×1080 | 静止 | 关 | 0.0278 | 0.0444 | 0.0473 | +0.0166 |
| Model | 1920×1080 | 静止 | 开 | 0.0560 | 0.0732 | 0.0756 | +0.0172 |
| Model | 1920×1080 | 移动 | 关 | 0.0283 | 0.0444 | 0.0489 | +0.0161 |
| Model | 1920×1080 | 移动 | 开 | 0.0539 | 0.0759 | 0.0770 | +0.0221 |

## CPU 管线准备

单位 ms，**median / P95**。包括 Render 管线声明、RHI 几何准备和全屏材质绑定准备。Main tick 的 frame_ms、含排队的 cpu_latency_ms 与细分 material/plan/prepare/fullscreen 指标均保留在 JSON/CSV；不能把这些指标相加当作墙钟帧时间。

| 场景 | 分辨率 | 镜头 | CSM | HDR Forward | Deferred Compact | Deferred High |
|---|---|---|---|---:|---:|---:|
| Scene | 1280×720 | 静止 | 关 | 0.1175 / 0.3017 | 0.1357 / 0.2308 | 0.1406 / 0.2517 |
| Scene | 1280×720 | 静止 | 开 | 0.1790 / 0.2741 | 0.2650 / 0.4031 | 0.2695 / 0.4094 |
| Scene | 1280×720 | 移动 | 关 | 0.3260 / 0.5597 | 0.3454 / 0.5611 | 0.3424 / 0.5864 |
| Scene | 1280×720 | 移动 | 开 | 0.7447 / 1.1299 | 0.7866 / 1.1421 | 0.8095 / 1.1876 |
| Scene | 1920×1080 | 静止 | 关 | 0.1228 / 0.2553 | 0.1467 / 0.2776 | 0.1609 / 0.3229 |
| Scene | 1920×1080 | 静止 | 开 | 0.1682 / 0.2593 | 0.2795 / 0.4373 | 0.2649 / 0.3554 |
| Scene | 1920×1080 | 移动 | 关 | 0.3238 / 0.5545 | 0.3471 / 0.5819 | 0.3613 / 0.6626 |
| Scene | 1920×1080 | 移动 | 开 | 0.7450 / 1.0975 | 0.7683 / 1.1562 | 0.7762 / 1.1748 |
| Model | 1280×720 | 静止 | 关 | 0.0567 / 0.0865 | 0.1065 / 0.1705 | 0.1023 / 0.1547 |
| Model | 1280×720 | 静止 | 开 | 0.0778 / 0.1209 | 0.1402 / 0.1933 | 0.1388 / 0.2046 |
| Model | 1280×720 | 移动 | 关 | 0.1458 / 0.2236 | 0.1701 / 0.2559 | 0.1685 / 0.2503 |
| Model | 1280×720 | 移动 | 开 | 0.2263 / 0.3466 | 0.2677 / 0.4100 | 0.2660 / 0.3989 |
| Model | 1920×1080 | 静止 | 关 | 0.0550 / 0.0854 | 0.1062 / 0.1574 | 0.1030 / 0.1630 |
| Model | 1920×1080 | 静止 | 开 | 0.0796 / 0.1266 | 0.1428 / 0.2111 | 0.1422 / 0.2052 |
| Model | 1920×1080 | 移动 | 关 | 0.1403 / 0.2237 | 0.1668 / 0.2653 | 0.1725 / 0.2715 |
| Model | 1920×1080 | 移动 | 开 | 0.2224 / 0.3507 | 0.2697 / 0.4364 | 0.2702 / 0.4200 |

## 1080p 移动镜头、CSM 开启的通道分解

单位 ms，均值；Forward 列为 Forward/HDR 几何，Base 列为 Deferred BasePass。

| 场景 | 管线 | Shadow | Forward | Base | Lighting | Tonemap | Total |
|---|---|---:|---:|---:|---:|---:|---:|
| Scene | HDR Forward | 0.0534 | 0.0620 | 0.0000 | 0.0000 | 0.0128 | 0.1338 |
| Scene | Deferred compact | 0.0558 | 0.0000 | 0.0426 | 0.0429 | 0.0122 | 0.1603 |
| Scene | Deferred high | 0.0556 | 0.0000 | 0.0496 | 0.0431 | 0.0124 | 0.1703 |
| Model | HDR Forward | 0.0204 | 0.0168 | 0.0000 | 0.0000 | 0.0118 | 0.0539 |
| Model | Deferred compact | 0.0228 | 0.0000 | 0.0132 | 0.0214 | 0.0121 | 0.0759 |
| Model | Deferred high | 0.0215 | 0.0000 | 0.0160 | 0.0214 | 0.0120 | 0.0770 |

## 内存与提交计数

SceneColor RGBA16F + SceneDepth D32 为 12 bytes/pixel；Compact 总计 36、High 总计 44 bytes/pixel。以下实际分配来自 Scene 移动/CSM 开启组，包含四级阴影、帧资源、模型、上传等，不等于 GBuffer 载荷；区间是两轮所有采样帧的 min–max。

| 分辨率 | 管线 | 主场景目标载荷 MiB | 设备实际分配 MiB |
|---|---|---:|---:|
| 1280×720 | HDR Forward | 10.55 | 80.35–80.47 |
| 1280×720 | Deferred compact | 31.64 | 102.85–103.10 |
| 1280×720 | Deferred high | 38.67 | 110.35–110.60 |
| 1920×1080 | HDR Forward | 23.73 | 99.10–99.22 |
| 1920×1080 | Deferred compact | 71.19 | 149.72–149.97 |
| 1920×1080 | Deferred high | 87.01 | 166.60–166.85 |

所有 96 次运行的 warmed PSO 和 descriptor 累计数均保持不变；单次采样内设备分配最大波动为 256 KiB。另有原生测试覆盖尺寸、布局、管线和 CSM 分辨率的交替替换、同时排队的两代图、取消/失败及恢复；这证明所测窗口内复用与退休有界，不构成无限时长内存证明。

下表为 1080p 移动/CSM 开启。主视图 item/draw 不含 fullscreen；绑定、常量写入和原生命令列表 reset 是设备累计计数在采样区间的每帧增量均值。PSO 为进程累计创建数量，采样期不增长。

| 场景 | 管线 | 主 items / draws | Shadow draws | Fullscreen draws | PSO | Pipeline binds/帧 | Geometry binds/帧 | Dynamic binds/帧 | Constants B/帧 | Lists reset/帧 |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Scene | HDR Forward | 205–206 / 6 | 24 | 1 | 5 | 11.0 | 68.0 | 18.0 | 750.1 | 9.0 |
| Scene | Deferred compact | 205–206 / 6 | 24 | 2 | 6 | 12.0 | 71.0 | 21.0 | 894.1 | 11.0 |
| Scene | Deferred high | 205–206 / 6 | 24 | 2 | 6 | 12.0 | 71.0 | 21.0 | 894.1 | 11.0 |
| Model | HDR Forward | 4 / 4 | 16 | 1 | 3 | 6.0 | 48.0 | 18.0 | 696.0 | 9.0 |
| Model | Deferred compact | 4 / 4 | 16 | 2 | 4 | 7.0 | 51.0 | 21.0 | 840.0 | 11.0 |
| Model | Deferred high | 4 / 4 | 16 | 2 | 4 | 7.0 | 51.0 | 21.0 | 840.0 | 11.0 |

逐帧主视图/阴影实例上传量和 instanced item/draw、descriptor、原生命令列表创建数量也保存在聚合 JSON 与原始 CSV。没有添加 profiling-only GPU waits/flushes，计时读取现有完成 submission 的 timestamp。

## 保存的旧 Forward 参考

改动前保存了旧 Release 可执行文件及数据，SHA-256 为 000D01E961FB16A480894ED61FD228EA6038AFE97730E14DD4F7B530617F807A；保存时工作区 HEAD 为 2a1ad62，没有重新构建该旧二进制来确认其精确源码版本。旧采样发生在本次 shader 修改之前，原始数据位于 [旧 Summary.json](../out/DeferredBaseline/Measurements/Summary.json)。

补采使用相同 Scene、1440×900、CSM 2048、预热 200 / 采样 300、lead 0/0、静止/移动镜头；[补采 Summary.json](../out/DeferredLegacyMatched20260911/Summary.json) 记录 12 组审计前管线数据。旧版为直接 backbuffer 输出，新版包含 HDR 与最终 tone pass，输出契约、采样时段和二进制来源存在区别，此表只用于观察迁移成本；上面的同二进制 HDR A/B 是主要结论。

    python tools/MeasureDeferred.py --output out/DeferredLegacyMatched20260911 --samples 300 --warmup 200 --repeats 1 --sizes 1440x900 --scenes Scene --main-render-lead 0 --render-rhi-lead 0

单位 ms，CPU 为 pipeline prepare median / P95，GPU 为均值。旧 GPU 仅有 Shadow + Forward 可比较区间，新 GPU 采用 Shadow + Forward/Base + Lighting + Tonemap 的和，未把新额外空 pass/转换时间计入此历史表。

| 镜头 | CSM | 管线 | CPU median / P95 | GPU 主要阶段合计 |
|---|---|---|---:|---:|
| 静止 | 关 | 旧 Forward | 0.0285 / 0.0433 | 0.0344 |
| 静止 | 关 | HDR Forward | 0.0700 / 0.1064 | 0.0374 |
| 静止 | 关 | Deferred compact | 0.1079 / 0.1607 | 0.0590 |
| 静止 | 开 | 旧 Forward | 0.0677 / 0.0989 | 0.1063 |
| 静止 | 开 | HDR Forward | 0.1150 / 0.1666 | 0.1069 |
| 静止 | 开 | Deferred compact | 0.1612 / 0.2359 | 0.1341 |
| 移动 | 关 | 旧 Forward | 0.2303 / 0.3406 | 0.0522 |
| 移动 | 关 | HDR Forward | 0.3052 / 0.4875 | 0.0378 |
| 移动 | 关 | Deferred compact | 0.3214 / 0.4842 | 0.0586 |
| 移动 | 开 | 旧 Forward | 0.6931 / 0.9428 | 0.1186 |
| 移动 | 开 | HDR Forward | 0.7123 / 1.0498 | 0.1167 |
| 移动 | 开 | Deferred compact | 0.7313 / 1.0130 | 0.1235 |

## 正确性与交付证据

- Debug、Release 全套 CTest 各 54/54 通过，包含真实 D3D12、窗口/GUI、场景/模型、CSM、帧生命周期和 RenderDoc 捕获及 GPU 回放。最后的相机元数据补充后，两配置的相关 Viewer/捕获测试各 4/4 再通过；此前的子视口和恢复补充也通过针对性测试。日志：[Debug](../out/DeferredCheckFinalDebug.log)、[Release](../out/DeferredFinalRelease.log)、[最后 Debug](../out/DeferredModelCameraDebugTests.log)、[最后 Release](../out/DeferredModelCameraReleaseTests.log)。
- 新增 deferred_rendering：混合 RGBA8/RGBA16F MRT、清除后采样、HDR >1、Reinhard/sRGB 单次编码、透明线性混合、Unlit 排他路由、mask/UV1/normal map/double-sided/mirror、实例化与普通路径、Compact/High/RGBA32F 自定义布局、子视口、CSM 与移动镜头；同时检查错误数量/格式/尺寸/别名/读写冲突和资源生命周期。
- 图像数值夹具中 HDR 4.0 经 tone/sRGB 得约 0.906，透明在 HDR 混合后得约 0.842，均验证最终像素。Forward/Deferred 对照夹具平均误差限制 <0.002；本次运行最大差值约 1/255（浮点记录为 0.00392158），见 [数值证据](../out/DeferredFinalImageEvidence.log)。法线/深度量化与重建不承诺逐位一致。高精度 GBuffer + GUI 实机截图见 [诊断截图](../out/DeferredHighGBufferUi.png)，验证层错误 0。
- 着色器测试覆盖 DXIL/SPIR-V/MSL 的 BasePass/Forward、实例变体及全屏 lighting/debug/tonemap；BasePass 恰有四个颜色输出，不含阴影/光照资源依赖。Reflection v5 修复系统输出大小写导致的跨格式重复计数。
- 格式、355 个 owned 源文件路径、219 个 C++ 翻译单元语义命名及最后两处修改复查通过；依赖边界检查通过，OpenSpec strict validation 与 git diff --check 通过。

API、颜色、GBuffer、兼容材质和未来 stencil 扩展边界详见 [DeferredRendering.md](DeferredRendering.md)。当前只实现单一 DefaultLit deferred lighting；透明与 Unlit 分流到共享 HDR Forward。未实现多 shading-model/material-ID 分发、compute lighting、tile/cluster 光源剔除或多光源性能优化。

## 审计修复后补测（2026-09-11）

上方 96 次矩阵与历史比较保留为审计前测量，未将旧数据改写为修复后的结果。本节使用四项审计修复后的 Release 构建补测 Scene 1080p，覆盖静止/移动 × CSM 关/开 × 三种管线/布局 × 两轮，共 **24 次运行、12,000 个采样帧**。每次预热 300、采样 500，第二轮反转模式顺序，其余条件与主矩阵相同。

可执行文件 SHA-256：80052391cd1c7495f54f52a4dedfca195cacb0f38c025053eb78dc663703f7b3。源码/着色器身份见 [审计报告](DeferredAudit.md) 的修复候选快照。原始命令、CSV 和日志：[补测 Summary.json](../out/DeferredAudit/Performance/Summary.json)。GPU 测试和编译未与本补测并行。

下表单位 ms，单元格为 **GPU pass 总时间 mean / CPU pipeline prepare median**；每格合并两轮 1,000 帧。

| 镜头 | CSM | HDR Forward | Deferred Compact | Deferred High |
|---|---|---:|---:|---:|
| 静止 | 关 | 0.0521 / 0.1184 | 0.0893 / 0.1400 | 0.0944 / 0.1410 |
| 静止 | 开 | 0.1337 / 0.2060 | 0.1596 / 0.3103 | 0.1688 / 0.2995 |
| 移动 | 关 | 0.0503 / 0.3487 | 0.0870 / 0.3707 | 0.0929 / 0.3887 |
| 移动 | 开 | 0.1346 / 0.7936 | 0.1562 / 0.8062 | 0.1715 / 0.8705 |

所有逐帧工作量匹配、提交对应、就绪状态和资源稳定性断言均通过；验证层错误为 0，采样期 PSO/descriptor 累计数不增长，单次采样内实际设备分配最大波动 256 KiB。补测仍体现该单方向光场景的 Deferred 额外成本；硬件频率未锁定，不将不同时间段的小幅差值解释为修复导致的性能变化。

该候选 Debug/Release 全套 CTest 各 54/54 通过，包含新增审计回归与 RenderDoc 捕获/回放。完整修复、独立复验、验证日志与边界见 [DeferredAudit.md](DeferredAudit.md)。
