# CPU 异步帧管线

Main、Render 和 RHI 可以处理不同的 engine tick，同时分别限制 Main 对 Render、Render 对 RHI 的最大领先帧数。RHI 0 仍等待所有同帧录制线程，再按帧顺序提交。GPU fence、交换链缓冲数量和资源退休规则保持独立。

## 配置

实验配置 `properties` 支持：

```json
{
  "main_render_lead": 2,
  "render_rhi_lead": 1,
  "rhi_threads": 2
}
```

也可以通过 Viewer 命令行覆盖：

```powershell
.\out\build\debug\bin\hyperion_viewer.exe --main-render-lead 2 --render-rhi-lead 1
```

两项领先配置均为启动配置，范围为整数 **0～16**，默认均为 **1**；负数、小数、无效字符串和越界值会在启动时拒绝。修改持久化配置需要重新启动才能生效。本次仍要求至少一条 RHI 线程。

限额是上限，不要求线程保持固定距离。GPU 帧槽复用、资源准备、同帧并行录制汇合、Present、截图和排空仍可能发生等待。

## 帧推进语义

帧 ID 从 1 开始，与每次 engine tick 对应。Viewer 的既有 benchmark `frame` 列从 0 开始，因此正常绘制时该列加 1 等于 CPU 帧 ID。

设当前提交帧为 N：

| 边界 | 进入下一帧之前的条件 |
| --- | --- |
| Main → Render | Render 已完成至 `max(0, N - main_render_lead)` |
| Render → RHI | RHI 已完成至 `max(0, N - render_rhi_lead)` |

例如 `main_render_lead=1` 时，Main 可以处理 N，Render 同时处理 N−1；Main 进入 N+1 之前必须完成 Render(N−1)。零配置会等待刚派发的当前帧。

- `a=0,b=0`：两级都同步，可显式选择此配置恢复同步 CPU 帧关系。
- `a>0,b=0`：Main 可以提前，Render 每帧等待 RHI。
- `a=0,b>0`：Main 每帧等待 Render，但 Render 可以提前于 RHI。
- `a>0,b>0`：两个边界分别施加限制，慢 RHI 最终会反压至 Main。

**Render 完成**包含本帧构图、交付 RHI 和本次 Render 推进所需的限流等待。**RHI 完成**包含所有同帧录制者、提交／Present 调用和帧结果写入，不代表 GPU 已完成。

RHI 线程的任务数不是帧进度。管线观察 RHI 0 的整帧完成，因此最慢的录制线程会约束 Render；本帧没有分配 Pass 的 RHI 线程不会阻塞进度。不同帧不会在 RHI 内部交错录制。

## API 和数据所有权

`FFramePipeline` 位于 Renderer，通过既有 `FTaskSystem` 执行两段工作：

1. Main 调用 `Submit`，移交拥有输入的 Render callable。
2. Render callable 返回拥有图及相关数据的 RHI callable。
3. 管线将 RHI callable 派发到 RHI 0，并在相应帧边界施加限额。

`FFrameTicket::Ready()` 表示两段工作已终止。即使 Ready 返回 true，读取结果前仍应在 Main 调用 `Wait()` 观察潜在异常。等待要求 TaskSystem 仍然存活。`Drain()` 汇合全部已接收工作后报告异常；成功 Drain 后可以继续提交下一帧，用于 profiling 边界等用途。

生产者的局部变量不可被异步任务引用捕获。按值捕获的裸指针仅用于稳定的服务对象，并要求这些对象存活到 Drain 完成。当前 Viewer 在帧包中复制 Settings、View、阴影设置、GUI 数据和原生 Surface 值，材质与场景数据通过不可变快照持有。

`ExecuteGraphOnRhi` 在 RHI 0 执行一个拥有数据的图。原有 `ExecuteGraph` 同步包装仍保留。图中的延迟 Prepare/Resolve 回调同样需要拥有其输入，单纯复制一个引用捕获的 callable 不会延长被引用对象的生命周期。

`FRenderSession::GetViewPreparation()` 与 `FForwardRenderPipeline::GetFrame()` 返回对应构建帧的准备结果句柄，必须在下一次构建前取得。后续构建可以替换 Session/Pipeline 的当前状态，旧句柄仍独立存在；RHI 准备结果通过同步发布，未准备完成时访问会报错。原有同步 `CompleteViews()`、`Complete()` 和 `Statistics()` 兼容接口保留。

共享状态分工：

| 状态 | 所有权／同步 |
| --- | --- |
| 输入、GUI、Viewer 设置、显示统计、文件输出请求 | Main |
| 场景收集、材质求值、batch planning、阴影布置 | Render |
| 原生资源准备、图执行、提交与 Device 统计采样 | RHI 0 |
| 帧请求与输出 | 请求冻结后移交；输出完成后由 Main 消费 |
| 视图族准备统计 | 每帧独立存储与发布锁 |
| 资源发布及缓存协调器 | 沿用 Publication、Mutex、ScopeMutex 等现有同步 |

Provider 注册在首帧前完成，首次冻结后不再由 Main 重写冻结标志；求值缓存仍由 Render 独占。场景发布任务和帧构建从 Main 按顺序进入 Render 队列，没有给后续帧添加可使其越过先前场景更新的异步依赖。

## 生命周期与错误

- 最小化或零尺寸的 tick 通过 `Skip()` 推进两级 CPU 帧身份，不调用 BeginFrame/Present。恢复后的 Resize 仍由 RHI 0 按序执行，并沿用原生 fence 等待。
- 正常退出先排空 CPU 帧并收集结果，再验证输出、销毁插件／Session 和释放原生对象。异常退出也先汇合在途帧，再清理依赖。
- Render、RHI 或录制线程失败会停止后续帧接收。Drain 会继续等待其他已接收任务，不依赖“前驱成功才运行”的后继清理任务。
- Main 等待保留 TaskSystem 的 Main 队列泵送。被泵送的回调不能重入同一管线的 Submit/Drain；接口会显式拒绝这种重入。RHI 工作不能同步等待 Render。
- 稳定运行中的帧包数量随 `a+b+1` 有界增长。GPU 引用仍依据 fence 生命周期保留，不能因为 CPU ticket 完成而提前复用 GPU 存储。

## 统计、截图和 RenderDoc

GUI 显示 Main 已提交、Render/RHI 已完成及当前显示结果的 CPU 帧号；这些是进度计数，不是线程当前正在执行的帧号。显示最近完成结果允许自然滞后。

截图随原始帧保存，验证使用该帧的设置与就绪状态。RenderDoc 请求到目标帧进入 RHI 0 时才提交，日志记录 `RenderDoc target CPU frame`，避免更早排队帧消费请求。自动打开在该帧结束后执行，防止打开后续帧的文件。GUI 点击触发的目标帧仍遵循控件实际触发的 tick。

Benchmark 每行使用同一 CPU 帧的视图、draw、Device 统计，按实际 GPU submission identity 匹配 GPU 时间。开始采样之前跳过的 tick 不会破坏对应关系；采样区间内没有绘制的 tick 会明确报错。采样开始时场景必须就绪。

保留既有 CSV 列，并新增：

- `cpu_latency_ms`：从本次 Main tick 开始到本帧 RHI 工作完成前记录结果的 CPU 延迟。
- `main_render_lead`、`render_rhi_lead`：本次启动的领先配置。

`frame_ms` 是 Main tick 的处理和限流时间，与 CPU 帧延迟、GPU 时间不同；不要将三者相加。profiling 的两个管线 scope 携带 CPU FrameId；切换预设采样窗口时先 Drain，避免旧 CPU 帧跨过切换点。

## 验证入口

```powershell
.\tools\Build.ps1 -Preset debug -Test
.\tools\Build.ps1 -Preset release -Test
ctest --test-dir out/build/debug -R 'cpu_frame_' --output-on-failure
```

- `cpu_frame_pipeline`：闸门控制的零／混合／最大窗口、慢 RHI 3、无工作线程、有序完成、失败、Main 泵送、重入拒绝和尾帧排空。
- `cpu_frame_ownership`：两个图都已构建而第一帧尚未准备，在场景移除后仍能执行第一帧的 8 draws，第二帧为 0；检查对应统计、不同清屏像素与原生验证。
- `cpu_frame_viewer`：同步／异步像素一致、配置与 CLI、最小化恢复和 Resize、尾帧截图、跳帧后 benchmark 身份、移动相机 CSM 图像和 draw 覆盖。CSM 测试副本将地面下移 0.01，避免原场景共面接触处的绘制顺序影响逐像素比较。
- `cpu_frame_capture`：启用 RenderDoc 的构建中，连续指定帧的异步请求各产生一个正确标记的 RDC；没有运行库时按既有约定跳过。

- `profiling_trace_acceptance`：Tracy 构建中以 2/3 限额采集真实 trace，检查 Main 与 Render/RHI 的对应帧 ID、采样窗口和完整／增量准备计数。

具体构建配置、测试结果与证据见本次 [实施记录](../openspec/changes/archive/2026-09-10-add-bounded-cpu-frame-pipeline/implementation.md)。
