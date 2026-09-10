# 实施与验证记录

日期：2026-09-10。

## 交付范围

已实现 Renderer 所有的 `FFramePipeline`，由既有 Main、Render、RHI 0 队列执行有序 CPU 帧。启动参数 `main_render_lead` / `render_rhi_lead` 与 CLI `--main-render-lead` / `--render-rhi-lead` 分别控制两个边界，默认均为 1，支持整数 0～16。RHI 至少一条；RHI 0 汇合同帧录制线程后提交，GPU fence、帧槽和原生资源退休规则沿用现有实现。

Main 提交 N 后等待 Render 至 N−a，Render 提交 N 后等待 RHI 至 N−b，负阈值按零处理。Render 完成包含该次推进所需的限流等待；RHI 完成包含同帧录制汇合、提交／Present 和结果发布。CPU ticket 完成不代表 GPU 完成。

没有引入跨帧 RHI 交错录制、乱序提交、运行时修改限额或专用无锁数据队列。实施阶段结束时 OpenSpec change 保持活动状态，未归档、未暂存或提交；后续按用户要求于 2026-09-10 同步主规范并归档。

## 所有权与生命周期复核

| 交接或状态 | 实现与保证 |
| --- | --- |
| Main → Render | Viewer 帧包拥有 Settings、View、GUI、阴影设置与 Surface 值，持有不可变材质上下文；生产者局部数据不被引用捕获 |
| Render → RHI | 图与当前 `FForwardFrame` token 按值移动，结果包通过 `shared_ptr` 持有 |
| RHI 0 → 录制 peers | `FFrameRecording` 拥有命令与结果数组；各线程写独立分区，成功和失败路径都先汇合已派发 peers |
| 稳定服务指针 | Tasks、Device、Swapchain、插件和 Session 由应用持有；所有退出路径先排空 CPU 帧，再释放消费者 |
| 视图族统计 | 每族独立 `FPreparedViewFamily` 与 publication mutex；后续构图不能覆盖旧帧统计；未完成读取显式报错 |
| 材质 Provider | 首帧前冻结一次；去除 Main 后续每帧重写 Render 正在读取的冻结标志；求值缓存仍归 Render |
| 场景与资源 | 场景发布与帧构建按 Main 派发顺序进入 Render；保留资源 Publication/Mutex/ScopeMutex、不可变快照和写时复制缓存边界 |
| Viewer 输出 | Main 按完成顺序更新 UI、发起图片写入并收集 benchmark；验证使用原始帧 Settings 与就绪状态 |
| 运行参数诊断 | 活动限额在启动时独立保存；GUI 编辑待重启配置不会改写当前显示、CSV 和运行日志中的有效限额 |

首次失败以粘性异常保存，阻止新帧接收；Drain 仍逐一汇合所有已接收任务后报告错误。Main 的等待保留任务泵送，同一管线的重入 Submit/Drain 会被拒绝。正常、异常和部分初始化退出均先处理在途工作；GPU 清理继续经过已有原生 fence 路径。

最小化或零尺寸 tick 使用有序空 CPU 帧，不绘制；恢复和 Resize 在 RHI 0 顺序执行。RenderDoc 请求跟随目标 CPU 帧进入 RHI 0，连续请求不会被之前排队的帧提前消费。profiling 开关与预设窗口边界先排空 CPU 帧。

## 验证覆盖

- `cpu_frame_pipeline` 用闸门阻塞 Render 或 RHI peer，检查 0/0、1/0、0/1、1/1、3/1、1/3、16/16 的推进边界；延迟 RHI 3，同时让 RHI 1/2 无工作；检查有序结果、Render/peer 异常、禁止后续接收、反复 Drain、析构尾帧、Main 泵送和重入拒绝。
- `cpu_frame_ownership` 在第一帧无法执行时先构建两个原生图，并在中间移除场景绑定。第一帧仍有 8 draws、第二帧为 0，旧 token 统计保持 8；不同清屏颜色、截图和 D3D12 validation 同时验证。
- `cpu_frame_viewer` 检查同步与混合限额的 Triangle PNG 完全一致、配置保存和 CLI 拒绝非法值、4 RHI 的窗口最小化／恢复／缩放、1 RHI 空场景、尾帧截图，以及跳帧 warmup 后实际 GPU submission identity 的匹配。
- 运动镜头 CSM 验收使用 220 tick、160 tick warmup、60 行数据，比较 0/0 与 2/3：对应行的可见物体、主视图与阴影 draw 数一致，最终 PNG 完全一致，且场景就绪、无失败 items。测试副本的地面下移 0.01，以排除共面接触处的绘制顺序歧义，源场景不修改。
- `cpu_frame_capture` 在 2/3 限额下请求第 40、41 帧；检查目标 CPU 帧日志和两个不同的非空 RDC 文件。现有 RenderDoc 验收另外 replay Triangle、Model、CSM 捕获，覆盖 GUI draws 与抓帧失败路径。

## 构建与检查证据

本机使用 Ninja / MSVC 与 D3D12。Debug、Release 为 Tracy OFF、RenderDoc ON；profile 为 Release 优化、Tracy ON、RenderDoc OFF。三个配置均完成构建。

| 验证 | 结果 | 日志（仓库下） |
| --- | --- | --- |
| Debug 全套 CTest | 首轮 52/53；唯一失败为缺少 RenderDoc 运行库时的既有错误前缀不匹配。恢复原前缀后，两个抓帧验收均通过 | `out/CpuFrameFullDebug.log`、`out/CpuFrameDebugCaptureRetest.log` |
| Release 全套 CTest | 53/53，130.89 秒 | `out/CpuFrameFullRelease.log` |
| 最终 Debug/Release/profile 增量构建 | 通过；包含原子布尔显式类型和活动限额诊断修正 | `out/CpuFrameFinalBuild-debug.log`、`out/CpuFrameFinalBuild-release.log`、`out/CpuFrameFinalBuild-profile.log` |
| 完整语义命名 | 210 编译单元通过；最后修改的 4 编译单元增量检查通过 | `out/CpuFrameNamingFinal.log`、`out/CpuFrameNamingDelta.log` |
| Debug 最终相关回归 | 6/6；更新共面 fixture 后另复测 Viewer 1/1 | `out/CpuFrameFinalTests-debug.log`、`out/CpuFrameSceneRetest-debug.log` |
| Release 最终相关回归 | 6/6；更新共面 fixture 后另复测 Viewer 1/1 | `out/CpuFrameFinalTests-release.log`、`out/CpuFrameSceneRetest-release.log` |
| profile 相关回归 | 首轮 5/7；下述两类验收假设修正后，Viewer 1/1、真实 Tracy trace 1/1 均通过，最终覆盖的 7 项全部通过 | `out/CpuFrameFinalTests-profile.log`、`out/CpuFrameProfileRetest.log`、`out/CpuFrameTraceFinal.log` |
| 最终格式／路径 | 335 源文件通过 | `out/CpuFrameStyleFinal.log` |
| 模块边界 | 326 源文件、25 模块通过 | `out/CpuFrameBoundariesFinal.log` |
| OpenSpec | 全部 40 项 strict 通过 | `out/CpuFrameOpenSpecFinal.log` |
| Git 差异 | `git diff --check` 通过；索引无暂存改动 | 最终工作区检查 |

Debug 的全套与针对性复测是分开执行的；没有将这些结果描述为同一轮完整 53/53。最后两次 C++ 修改仅为原子布尔显式类型及活动配置诊断，之后三个配置均重建并通过相关回归。后续 Python 验收修正也已单独复测。

附加 profiling 配置发现并定位了两类验收假设：

1. 原 Showcase 场景同步／异步之间有 3 个边界像素差异。同步模式下仅将地面实例声明提前，恰好复现异步图像；将测试副本地面下移 0.01 后两模式完全一致。这是共面深度竞争对绘制顺序敏感，未降低 PNG 比较精度，也未更改引擎渲染策略。对照证据在 `out/CpuFrameRepeat.log`、`out/CpuFrameCoplanar.log` 和 `out/CpuFrameRepeat/`。
2. 旧 trace 验收强制要求 `PrepareSceneSnapshot`，并将所有 packet 复用等同于没有常量更新。现有 `PrepareRetainedView` / `CollectPrepared` 路径能跳过旧快照函数；`RefreshViewPasses` 复用命令并更新绑定时仍发送常量统计。验收改为检查实际路径，并通过 CSV 的 `local_packet_reuses` 严格计入增量更新。增加 2/3 异步模式下两个流水线 scope 各 120 个、ID 为 121～240 的真实 trace 检查。

## 默认值调整

按后续确认，两级默认限额均从 0 改为 1；应用配置与 Renderer API 保持一致。已有显式 0 配置继续同步等待。上述初轮全套验证发生在默认值调整前；调整后重新完成 Debug 构建与 6/6 定向回归（配置、帧所有权、限流、默认 Triangle、Viewer 和 RenderDoc），22.54 秒，格式与本 change strict 检查通过。默认启动的 80 tick 原生日志确认 `leads=1,1`，截图与显式同步模式一致。证据为 `out/CpuFrameDefaultOneBuild.log`、`out/CpuFrameDefaultOneTests.log`、`out/CpuFrameDefaultOneStyle.log`。

## 证据边界

闸门测试证明两级允许重叠且能反压，原生验收证明受测场景、参数和退出路径下的像素与结果归属。本轮未宣称 FPS 提升，也未执行 Windows C++ ThreadSanitizer；源代码所有权复核与测试不能等同于所有调度交错均已穷尽。较大领先量会增加 CPU 帧包保留和输入延迟，实际吞吐仍受 GPU、Present、资源准备与同帧录制等待约束。

使用与 API 约定见 [CPU 异步帧管线](../../../../docs/CpuFramePipeline.md)。
