# 实施与验证证据

2026-09-08，基础 revision `06b678b`。实现了 Core 宏与控制器、Viewer 控制、选择性常驻热点、材质计数、兼容的 Tracy 工具链、TaskSystem 执行段及 D3D12 pass 查询。使用方式见 [docs/Profiling.md](../../../../docs/Profiling.md)。以下保留实施和审计阶段的验证证据。

## 验证

| 构建 | 结果 | 日志 |
| --- | --- | --- |
| VS 2022 Debug，Tracy OFF，RenderDoc ON | 45/45，145.03 秒 | `out/ProfileVsDebugRetry.log` |
| VS 2022 Release，Tracy OFF，RenderDoc ON | 45/45，102.14 秒 | `out/ProfileVsRelease.log` |
| Ninja profile，Release 优化，Tracy ON，RenderDoc OFF | 40/40，111.83 秒 | `out/ProfileFinalTests.log` |
| 格式/路径、语义命名、模块边界 | 242 个源文件，149 个 C++ 单元；237 个模块源码/25 个模块 | `out/ProfileFormat.log`、`out/ProfileNaming.log` |

Debug 第一次链接遇到 `MaterialBindingContext.obj`、`SessionMaterialEvaluation.obj` 的 LNK1163。错误对象保存在 `out/Profiling/LinkFailureEvidence`；仅重新生成这两个对象后，完整构建和测试通过，没有修改渲染逻辑或优化级别。命名检查的数据库合并了 profile 的启用路径与 Ninja Debug 的 RenderDoc 专属单元，覆盖全部自有 C++ 源码。`CheckBoundaries.py` 和 `git diff --check` 通过。

普通 Debug/Release 的 `offline_startup` 均通过真实进程 TCP/UDP 观察。两种配置也通过原有 RenderDoc capture/replay、失败 Present 恢复、材质/场景/资源、GUI 和运动相机验收。Profiling 构建已生成配套 PDB，Release 在 `/DEBUG:FULL` 后显式保留 `/OPT:REF /OPT:ICF`；Viewer exe 5,879,296 字节，PDB 72,339,456 字节。

真实 trace 验收位于 `out/Profiling/Acceptance`：

- 两次连接均检查多个源位置、同线程区间嵌套、数值 task ID、异常、运行中关闭 mask 后的结束事件和 plots；collector 没有报告 instrumentation failure。
- 单 Worker 嵌套等待及异常传播继续工作。一个 Worker 经 IO 主动挂起约 6 秒，两次采集中各有一个短执行段，挂起期间发生断开/重连。
- 原生测试启用 GPU 后执行失败 Present、重复取消、恢复帧和已提交资源回收。额外保存取消的 list，验证查询池最多两个、槽压力下跳过采集、完成后释放查询引用及后续恢复；未提交 list 没有发布 GPU span。
- 运动 Viewer 的 basic 与 detail-gpu 各采 120 帧，ApplicationFrame 注释精确为 120–239，均为 194–195 scene draws。Basic 不包含 Detail 或 GPU；Detail/GPU 含 480 个 GPU pass，与 RecordPass 数一致，各材质/缓存 plot 每帧一组。
- GUI 测试检查未编译状态、四个独立开关、反馈状态和高 DPI 的按下/释放交互。CLI 拒绝未知类别、尾随逗号、负数和超出运行帧范围的窗口。helper 的非零客户端退出及超时路径也经过实际子进程测试。

## 固定场景开销

同一台 NVIDIA GeForce RTX 5080，MSVC 19.50.35725，`/O2 /Ob2 /DNDEBUG`，D3D12 debug layer 开启、VSync 关闭、RenderDoc 未加载。场景来自 `experiments/Scene.json`，1440×900、78 个模型；临时配置只关闭 GUI，避免编入/未编入 Profiling 时控件数量不同。GUI 开启的真实场景已由上面的采集验收单独覆盖。

每组 240 帧预热、1500 帧测量，交错顺序运行三轮；测量期间没有同时运行构建或 GPU 测试。所有组都保持 194–195 draws。下表 mean/P95 为三次结果各自的中位数，范围是三次 mean 的最小/最大值。

| 模式 | mean ms | P95 ms | mean 范围 ms | 相对编译关闭 |
| --- | ---: | ---: | ---: | ---: |
| 编译关闭 | 3.1275 | 4.1288 | 3.1066–3.4098 | 基准 |
| 编入，运行时关闭 | 3.2581 | 4.1605 | 3.2089–3.3023 | +4.17% |
| Basic，连接并采集 | 3.2526 | 4.2889 | 3.2242–3.3171 | +4.00% |
| Detail + GPU，连接并采集 | 3.3465 | 4.3087 | 3.3342–3.4349 | +7.00% |

这些是本场景的实际进程成本，包含后台服务、事件传输与已存在的分配事件，不只是 scope 分支成本。三轮存在波动和区间重叠，不能把 Basic 略低于 runtime-off 解读为采集提升了性能，也不能推广为固定开销保证。

原始帧 CSV、trace、各类导出、二进制路径、CMake 编译配置和每轮状态保存在 `out/Profiling/FinalOverhead`；汇总为 `Summary.json` 与 `Results.json`，复测脚本为 `out/Profiling/MeasureOverhead.py`。前期测量位于 `out/Profiling/Overhead`，未用于上表。最终测量二进制 SHA-256：

- 普通 Release：`c374a91520138d3e2c558e60d900997eeda3bc8cfa95bbf3861c70c10880fc48`
- profile：`b5d2c009c97c84dab411076f02a1d7618c2be6993ee3458a70098f4ba4ed43df`

## Scope 微基准与限制

每个样本执行 200,000 次不可内联 probe；每次程序取七个样本的中位数，每模式运行三次。下表报告三次「带 scope 减 plain probe」结果的中位数，保留 checksum，避免工作被优化移除。

| 状态 | 额外 ns / 调用 |
| --- | ---: |
| 编译关闭 | 0.000，落在测量噪声内 |
| 编入、mask=0，无 collector | 0.173 |
| mask 开启、无连接 | 2.198 |
| mask 开启、连接采集 | 21.118 |

每次 connected 微基准的 collector 均收到 1,400,000 个 zone，且无 instrumentation failure。编译关闭的宏参数被预处理移除；runtime-off 是廉价分支，不宣称绝对零成本。

`FinalOverhead/Sampling` 实际请求了系统采样；当前进程未提权，状态为 `Unavailable`，仍成功完成 120 帧普通 CPU scope 采集。因此没有报告 ETW 调用栈采样的性能或覆盖率。`Requested` 状态在 GUI 契约中验证，实际有权限的 ETW 数据仍需在对应运行环境确认。GPU 数据的精度受硬件频率与校准误差约束；本轮没有诱发真实 device removal 或 OOM。

工具链验证还修正了一个退出顺序问题：Tracy 可先完成保存、Viewer 随后才结束线程回收。helper 现在允许这种正常顺序，仍限制客户端总时长，并在导出后检查整个请求帧窗口，拒绝不完整 trace。

## 独立审计与范围内修复

用户请求 `quality-audit` 后，由不继承实现会话的 reviewer 对基线 `06b678b3f9425e7acff206839229e1fd5c6d4fb2` 上的全部 69 个改动文件独立审核；审核期间 SHA-256 未变化。报告位于 `out/Profiling/Audit/Review.md`。唯一确认项 `PROFILE-01 / P2` 是 Assets 类别仅包含 shader 编译，遗漏实际资产加载阶段。主 agent 独立跟读 IO、AssetService、glTF 和图像调用链后确认。

最小修复增加五个完整操作入口 scope：`ReadAssetBytes`、`ImportGltf`、`ParseGltf`、`ConvertGltfMeshes`、`DecodeImage`，并为 IO 声明直接 Core 依赖。没有在顶点、参数或像素循环中增加标记。现有 trace acceptance 增加 assets-only 与 frame-only 两次启动加载采集；前者验证五个阶段及真实源文件，后者验证禁用类别无对应事件，两者均验证加载完成后的 120 帧和 draw 数。

修复后 profile 与普通 Release 构建通过；针对性 CTest 各 8/8（profile 40.74 秒，Release 11.47 秒），包括 Tasks、IO、资产导入、图像、profiling/GUI 契约，以及 profile 的完整真实 trace 验收和 Release 的 offline_startup。日志位于 `out/Profiling/Audit/ProfileTests.log`、`ReleaseTests.log`；本轮 trace 位于 `out/Profiling/Acceptance/20260908-105804`。格式、模块边界、三个修改 C++ 单元的语义命名、Python 编译与 diff whitespace 检查通过。

Reviewer 另独立重跑 CPU 两次重连，并在同一 Viewer 进程完成 GPU 两次重连（3,677 / 4,116 spans，无 instrumentation failure），证据位于 `out/Profiling/Audit/IndependentReconnect`、`GpuReconnect`。本次小修复没有重跑前文全量 VS 套件或完整开销矩阵；前文的二进制哈希和性能测量对应修复前版本，新增 scope 作用于启动加载阶段。

原 reviewer 针对修复独立重新采集 assets-only/frame-only 启动加载，确认五个新增阶段、类别排除及线程内嵌套均正确，两组各 120 个稳定帧、194 draws。复审末 73/73 文件哈希一致，`PROFILE-01` 关闭，无其他未解决 finding；复审报告和原始数据位于 `out/Profiling/Audit/ReReview.md`、`IndependentAssetReReview`。随后按用户要求归档为 `2026-09-08-add-runtime-performance-profiling`，并同步 Core 与 runtime-performance-profiling 主规格。
