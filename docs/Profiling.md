# 性能分析

默认 `HYP_ENABLE_TRACY=OFF`，不启动 Tracy，也不执行 scope 的时钟读取或全局累计计数。应用帧时钟、场景诊断和 tagged 内存统计独立保留。启用构建后，运行时默认 category mask 为零；GUI 与 CLI 的设置只影响本次运行，不写入 experiment 配置。

## 构建与采集

```powershell
# Release 优化与完整 PDB，独立输出目录
.\tools\Build.ps1 -Preset profile
.\tools\BuildProfilingTools.ps1

# 120 帧预热，随后 600 帧运动场景；保存 trace、CSV、日志、revision 和构建信息
python tools/Profile.py --out out/Profiling/MovingBasic --no-build
python tools/Profile.py --out out/Profiling/MovingDetailGpu --mode detail-gpu --no-build

# 同一 profiling exe，运行时关闭，用于开销对照
python tools/Profile.py --out out/Profiling/RuntimeOff --mode off --no-build

# 交互控制；先用匹配版本的 Tracy Profiler 连接本机，再在 GUI 中开关采集
out/build/profile/bin/hyperion_viewer.exe --config experiments/Scene.json --no-vsync
```

省略 `--no-build` 时，helper 先构建 Viewer 和所需采集工具。`--mode` 支持 `off/basic/detail/gpu/detail-gpu/sampling`；默认小幅连续相机运动，`--static` 改为静止，`--visible` 显示窗口，`--warmup`、`--frames`、`--timeout` 控制运行边界。`--viewer <exe> --no-build` 可对照普通 Release 构建。输出目录必须不存在，避免覆盖旧证据。场景未在预热结束前加载完成时明确失败；应增加预热，不能把空场景帧用于比较。

工具使用 `dependencies.lock.json` 锁定的 Tracy v0.14.1 源码，同时构建 `tracy-capture` 与 `tracy-csvexport`，不需要构建 GUI。仅工具构建通过 Tracy 自带 CPM 下载其指定的 Capstone、zstd、PPQSort、JSON 等依赖到 `out/profiling-deps`；普通引擎构建不需要它们。首次构建需要网络和 Git。交互查看应使用同版本 Tracy GUI。

Profiling 的 Release 链接在 `/DEBUG:FULL` 后显式保留 `/OPT:REF /OPT:ICF`，因为 [MSVC 的 `/DEBUG` 会改变这些默认值](https://learn.microsoft.com/en-us/cpp/build/reference/opt-optimizations?view=msvc-170)。PDB 不等于关闭优化；内联与相同函数折叠仍可能影响采样栈中显示的函数名。

helper 为其子进程选择独立 loopback 端口，通过子进程环境变量 `TRACY_PORT` 配对客户端和 collector；不修改系统环境或防火墙。失败和超时会终止并等待自己启动的进程。Tracy 官方 capture 在部分错误时仍返回零，因此 helper 也检查 instrumentation failure、完成消息和 trace 文件。

输出包括 `Capture.tracy`、`Frames.csv`、`Zones.csv`、`Events.csv`、`EventsAndPlots.csv`、`Gpu.csv`、`Metadata.json` 与日志。`EventsAndPlots.csv` 使用官方 exporter 的格式：`src_file` 为空的行是 plot，`value` 为数值；zone 的 value 可能形如 `123 [0x7b]`。CPU、GPU 时间单位为纳秒，Viewer 帧 CSV 为毫秒。Metadata 和复制的 CMake 配置记录实际二进制路径、编译配置、源码 revision、工作区状态、采集模式及负载统计。

## 使用 API

```cpp
#include "Hyperion/Core/Profiling.h"

void UpdateMaterial()
{
	HYP_PERF_FUNCTION();
	HYP_PERF_SCOPE_C(Material, PrepareInputs);
	// 默认 scope 类别为 Frame；细粒度热点显式归入 Detail。
	HYP_PERF_SCOPE_NAMED(Hyperion::EProfileCategory::Detail, "EvaluateParameters", Scope);
	HYP_PERF_VALUE(Scope, ParameterCount);
	HYP_PERF_PLOT(Material, UpdatedMaterials, UpdatedCount);
}
```

`HYP_PERF_SCOPE(Name)` 接受标识符，自动字符串化。`HYP_PERF_FUNCTION()` 使用当前函数名；`_SCOPE_NAMED` 的名称必须是静态字符串。每个宏站点保留调用方文件、函数、行号以及固定 backend 存储；重复执行不分配动态 source location。不要拼接 frame ID 或对象地址作为名字，使用 value 注释表达它们。作用域在异常退出时也结束；禁止复制、移动 scope 对象或让它跨越引擎 TaskSystem 之外的协程/线程切换。

编译关闭时所有宏参数均不求值。运行时关闭的 scope 只走内联状态检查，不读取时钟、注册站点或增加全局计数；禁用 category 的 plot/value 表达式不求值。已开始的 scope 即使中途关闭 mask 仍会按原连接结束。`FProfileScope` 和 `ProfileStats` 的旧无条件累计计时接口已移除，迁移时将构造改为 scope 宏；如需业务计时，使用独立应用时钟。

类别为 `frame,render,material,rhi,tasks,assets,detail,gpu`。`ProfileBasicMask` 包含前六项。CLI `--profile` 开启基本 CPU scope，`--profile-detail`、`--profile-gpu`、`--profile-sampling` 增加相应能力并开启基本类别；`--profile-categories material,rhi` 可仅选类别，多个开关取并集。`--profile-start N --profile-frames M` 使用零起始帧编号，窗口必须位于有限 `--frames` 内；M=0 表示不设采集帧数上限。`--profile-wait` 最多等 collector 15 秒。未编入支持时，显式请求会报错。

## 常驻观测点与语义

- Frame：输入、GUI、场景更新、ApplicationFrame 和帧标记。
- Render/Material：BuildViews、空间刷新/收集、PrepareMaterials、RenderGraph 编译/执行。
- RHI：GUI 资源准备、PrepareDraws、ValidateDraws、RecordCommands、队列提交、FenceWait、PresentWait、即时上传。RecordPass 从校验前开始。
- Detail：完整材质求值与刷新尝试、绑定上下文、常量查找/打包/上传、资源/PSO 缓存、索引范围和资源绑定。没有逐参数或逐索引元素埋点。
- Tasks：静态线程名、TaskDispatch/TaskExecute/TaskWait 的数值 task ID，以及仅在启用时计时的 TaskQueueMilliseconds。队列延迟从依赖已满足、进入执行器队列时开始。
- Assets：IO 任务内的 ReadAssetBytes、ImportGltf、ParseGltf、ConvertGltfMeshes、DecodeImage，以及 ShaderCompilation。标记覆盖完整操作，不进入顶点或像素循环；导入等待外部 IO 时同样按 Worker 执行段拆分。查看加载热点应从启动时开启 Assets，预热结束后的采集通常已经错过初次加载。

材质 reuse/refresh/full 的计数在一次 view 准备中使用局部变量累计；provider 与常量/绑定/PSO 缓存使用已有累计计数的差值。`SceneDraws` 表示该 Viewer 帧实际场景 draw。当前 Viewer 一帧一个 view，因此这些 plot 每帧一组；多 view 调用者会每 view 发布一组，应按时间和 view 准备 scope 解读。Full 计数包含进入完整求值后失败的尝试，Refresh scope 包含未能使用快速刷新路径的尝试。

材质优化新增 `ConstantFullLookups/ConstantPreparedReuses/ConstantEvictions` 和 `ProviderEvictions` 增量计数，以及 `ConstantCachedBlocks/ConstantCachedBytes/ConstantPreparedBlocks/ConstantPageBytes`、`ProviderCachedEntries/ProviderCachedValueBytes` 存量。候选常量字节为对齐后的 slice extent；provider 字节为 owned value-tree 估算，均不能代替进程 Private Bytes。`ConstantPageBytes` 包括外部旧帧与活跃 draw 保留的完整页面。原生 `GraphicsRootBinds/GraphicsHeapBinds/GraphicsConstantBinds/GraphicsTableBinds` 每次 list 录制发布实际命令数；同一帧有多个 pass/list 时需要汇总，不能拿单个 plot 样本当作每帧数量。详细对照见 [材质性能优化](MaterialPerformance.md)。

CPU scope 是包含子 scope 的墙钟区间；不同线程的总时间不能直接相加当作帧耗时。Worker 在 oneTBB 主动挂起前关闭所有引擎 scope，恢复后重新打开 execution segment 并保留注释；6 秒挂起会显示为前后两个短段。段内仍可能发生 OS 抢占或阻塞，不是纯 on-CPU 时间。Render/RHI 专属线程的同步等待保留为墙钟等待区间。frame CSV 和 GUI 曲线包含 Present 等待，也不等同于 GPU 时间。

GPU 使用 Direct3D12 队列时间戳与 clock calibration，单独显示静态 `GraphicsPass` 轨道，通过 CPU 录制时间关联各 pass。当前最多两个 frame 槽，每槽一个 32-query heap 和 readback buffer；每个 recording context 两个 timestamp。查询包跟随已提交 list 的现有 fence 回收，正常帧推进、Idle 和失败 Present drain 都能收集；不会为 profiling 新增 fence、wait 或 flush。未提交的取消录制不发布 GPU span，旧连接的数据不会进入新连接。槽仍被外部保存的旧 list 占用、查询分配失败或硬件不支持时跳过 telemetry，渲染继续。精度受 GPU 时钟周期与校准误差约束。

## 系统采样、内存与空闲成本

Tracy 系统采样采用手动 start/stop，与 GPU/Detail 分开。GUI 的 `Request CPU sampling` 或 `--profile-sampling` 只请求采样；Windows ETW 需要相应权限。本实现不自动提权，未满足权限时显示 `Unavailable`。`Requested` 表示 vendor 接受请求，不保证 ETW 已实际产出采样；需在 Tracy 中验证调用栈与符号。关闭全部 category 会停止采样。可选 VSync 系统采集被关闭，避免锁定版本的采样重启路径复用已销毁的 VSync worker。

`TRACY_ON_DEMAND` 不等于没有后台成本。编入 Tracy 后即使 mask=0，仍有服务线程和监听；新配置默认 `TRACY_ONLY_LOCALHOST=ON`、`TRACY_NO_BROADCAST=ON`，现有显式缓存选择保持不变。要恢复普通无监听程序，应重新配置 `HYP_ENABLE_TRACY=OFF` 并重编译。

已有 `Hyperion::Allocate/Deallocate` 的 tagged 分配/free 事件保持为连通时的完整配对流，不按 CPU category 独立开关，以免破坏内存视图。这意味着连着 collector、但 scope mask=0 时仍可见引擎内存事件。它只覆盖 routed 分配，不代表 STL、ImGui、驱动和全部第三方分配。显式采集中的排队、内存事件和导出都具有成本。

## 验证

```powershell
# 默认 build 的常规契约测试；编入 Tracy 且已构建工具时包括真实 trace 验收
.\tools\Build.ps1 -Preset profile -Target hyperion_check

# 独立验收：断线重连、单 Worker 嵌套/异常/挂起、GPU 生命周期、CLI、运动场景与计数
python Source/Tests/Integration/ProfilingAcceptance.py out/build/profile/bin/hyperion_viewer.exe . out/build/profile/bin/profiling_tests.exe out/build/profile/bin/d3d12_frame_failure_tests.exe

# scope 微基准；有 collector 时测 connected，没有时测 disconnected
out/build/profile/bin/profiling_tests.exe --microbenchmark
```

测量结果和完整回归记录见本次 [实施证据](../openspec/changes/archive/2026-09-08-add-runtime-performance-profiling/implementation.md)。基准采用优化构建并对照实际 draw 数；短运行的单次差异不能作为开销保证。

Renderer 的普通 draw 数扫描、独立 RHI 录制基准与逐视图阶段归因见 [RendererCpuPerformance.md](RendererCpuPerformance.md)。

静止准备复用、共享 scope 更新、每帧同步收拢及按 task identity 拆分的 Wait 分析见 [RetainedRenderFrames.md](RetainedRenderFrames.md)。GPU timing capture 按 submission 的 capture epoch 归属，较晚完成的旧提交不会混入新 capture；正常 GPU 统计仍处理全部完成结果。
