# RenderDoc capture

Build with `./tools/Build.ps1 -RenderDoc`, install the RenderDoc runtime, then enable **Edit > Editor preference > Enable RenderDoc capture** and restart. The viewport camera button requests a complete frame and opens exactly the resulting capture. `renderdoc.status/capture/open/set_preference` shares the same service. Capture/replay failures remain explicit and do not open an old result. PNG screenshots use `render.screenshot` or `--capture` separately.

Support is optional and default off. The header dependency is pinned in `dependencies.lock.json`; the build does not install RenderDoc or copy its DLL. Explicit `--disable-plugin renderdoc` wins over preferences. Uncompiled or unavailable capture leaves unrelated editing available. Preference files and startup behavior are described in [Editor](Editor.md).

**Show RenderDoc HUD** in the same preference dialog defaults to off and persists independently of capture. With the runtime loaded, toggling applies immediately without restart and affects all RenderDoc windows in the process. Capture, replay and capture hotkeys remain available with the HUD hidden. Old preference files default to hidden. `renderdoc.hud.get/set` uses the same Editor operation; `preference` is the saved value and `enabled` is the actual runtime state (null when unavailable). Setting a preference does not load or select a plugin. Save failure preserves the previous preference and HUD state.

## 引擎接口和线程契约

`Hyperion/Capture/FrameCapture.h` 定义 `FFrameCapture`、设置及状态；`Hyperion/RenderDoc/RenderDocPlugin.h` 提供生命周期插件。公共接口只有引擎与标准库类型。DLL、RenderDoc API 和 Windows 转换位于 Runtime/Capture 的 Private/Adapters，Renderer、RHI、DebugUI 不依赖具体捕获插件。

1. 调用 `Initialize()` / 启动插件必须早于首次 DXGI/D3D12 初始化。
2. `RequestCapture()` 接受至多一个待处理请求；`Status()` 返回同步后的副本。
3. 在 RHI 0 调用 `BeginFrame(Surface)`，等待它完成后再准备 GUI/模型 GPU 资源、录制和提交本帧。
4. 等待全部录制任务和 Present 完成，在 RHI 0 调用 `EndFrame()`。仅其返回 true 才表示本次抓取成功。
5. 图形异常时在同一协调线程调用 `Cancel()`；它只丢弃自身拥有的捕获。窗口最小化期间不开始捕获，待恢复绘制再执行；退出时取消待处理请求。
6. `OpenLastCapture()` 与捕获成功状态独立。自动打开只在本次 `EndFrame()` 成功后调用，不会因失败误开旧文件。
7. `OverlayEnabled()` / `SetOverlayEnabled()` 同步查询和修改进程级 HUD 总开关，保留帧率等其他 overlay bits；未初始化、初始化失败或停止后返回 null / false。可选 `FFrameCaptureSettings::OverlayEnabled` 在初始化时应用；未指定时保留已有 mask。Editor 显式传入保存的偏好。

抓取文件使用实验名、进程/会话标识、请求序号作为前缀，再由 RenderDoc 附加文件后缀。服务读取本次新增记录，并确认路径匹配、文件存在且非空。`CompletedCaptures` 只统计本服务成功完成的请求；旧路径保留为最近成功记录，不代表后一次失败变成成功。

DLL 保留至进程退出，`Stop()` 不执行 `RemoveHooks()` 或 `FreeLibrary()`。运行中停用服务不能移除已经包装的图形对象；完全停用要重启。发现外部捕获已在进行时拒绝重叠请求，不修改 RenderDoc 全局热键。调用方仍应避免另一个工具在同一时刻发起捕获。

如果取消 API 返回失败且捕获仍在进行，服务保留该捕获的所有权，报告清理失败并禁用后续抓帧；`Cancel()` / `Shutdown()` 可以再次尝试清理。此时 `Initialize()` 不会重置仍活跃的状态。清理恢复后重启应用再继续抓帧。如果 API 返回失败但捕获实际已经停止，则允许正常重试抓帧。

RenderDoc 保存文件可能阻塞当前帧，模型越大耗时和文件越大。UI 自动打开采用 `LaunchReplayUI`，允许新建 RenderDoc 实例。返回 PID 只证明进程已启动，完整文件加载通过回放验收另行验证。D3D12 命令列表包含 RenderGraph Pass 名称的 BeginEvent/EndEvent，方便在 Event Browser 中定位。

## Validation

`editor_capture_ui` drives actual preference/capture controls, restarts, tests explicit disablement, and checks capture replay/XML when a runtime is installed. `capture_unavailable`, `capture_incompatible`, `capture_runtime` and `capture_failure_recovery` retain the shared optional-service, runtime and cleanup coverage. Unavailable runtime cases use the documented skip result instead of claiming a GPU capture passed.
