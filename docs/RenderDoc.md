# RenderDoc 抓帧

当前支持 Windows x64 / D3D12、单设备和单窗口。Triangle 与 Model Viewer 共用 RDC 控件。原有 `Capture screenshot` / `--capture` 仍保存 PNG；RDC 保存可由 RenderDoc 回放的 GPU 命令、流水线和资源。

## 启用构建

```powershell
# Visual Studio：准备锁定的 API 头文件，生成并构建
.\tools\GenerateSolution.ps1 -RenderDoc -Build -Configuration Debug

# Ninja
.\tools\Build.ps1 -RenderDoc

# 显式关闭（两种构建 helper 都支持）
.\tools\GenerateSolution.ps1 -NoRenderDoc -Build -Configuration Debug
```

底层开关为 `HYP_ENABLE_RENDERDOC`，首次配置默认 OFF。VS helper 未指定开关时保留已有 CMake 缓存；Ninja 同样保留缓存。开启时编入 `hyperion_capture` 和 `hyperion_renderdoc`。关闭时不要求 RenderDoc 头文件、库或 DLL。

只下载固定提交的 `renderdoc_app.h`，校验值在 `dependencies.lock.json`，MIT 许可保留在头文件中。可以单独运行 `python tools/Bootstrap.py --only renderdoc`。不会编译 RenderDoc 源码、链接其 import library、复制其 DLL 到 Viewer 旁边或安装系统软件。

运行时需要安装 Windows x64 RenderDoc。默认查找 `%ProgramFiles%/RenderDoc/renderdoc.dll`；自定义位置使用 `--renderdoc-library` 或配置字段。已经通过 RenderDoc UI 启动时优先复用注入的运行库。要求 Application API 1.6.0；这不是 RenderDoc 产品版本号。本机验证运行库为产品 v1.37。

## Viewer 使用

```powershell
# 三角形：显示共用抓帧面板
.\out\build\vs2022\bin\Debug\hyperion_viewer.exe --renderdoc

# 模型：使用同一面板
.\out\build\vs2022\bin\Debug\hyperion_viewer.exe --config experiments/Model.json --renderdoc

# 捕获成功后自动启动 RenderDoc 打开文件
.\out\build\vs2022\bin\Debug\hyperion_viewer.exe --renderdoc --open-rdc

# 自定义安装目录和输出目录（路径可含空格及 Unicode）
.\out\build\vs2022\bin\Debug\hyperion_viewer.exe --renderdoc-library 'D:/Tools/RenderDoc/renderdoc.dll' --rdc-output 'out/我的 RDC'
```

面板操作：

- `RenderDoc plugin (restart)` 修改下次启动的插件列表，点击 `Save experiment` 保存后重启生效。`--renderdoc` 会强制本次启用，关闭时也要移除该启动参数。
- `Capture RDC` 请求一个完整渲染帧；正在等待/捕获或运行库不可用时禁用。
- `Open last capture` 打开最近成功抓取的文件。删除文件后会明确报错。
- `Open automatically after capture` 即时切换自动打开，默认关闭；保存实验后持久化。
- 面板显示运行状态、最近成功文件路径和独立的打开结果。

配置位于实验 JSON 的 `properties` 下：

```json
{
  "plugins": ["model-viewer", "debug-ui", "renderdoc"],
  "renderdoc_library": "",
  "renderdoc_output": "out/captures/renderdoc",
  "renderdoc_auto_open": false
}
```

相对输出目录基于工作目录；Visual Studio 调试工作目录和文档命令均为仓库根目录。默认实验不启用运行库。未编译插件却请求 `renderdoc` 会给出配置错误；已编译但 DLL 不可用时 Viewer 保持运行，面板说明原因。

## 引擎接口和线程契约

`Hyperion/Capture/FrameCapture.h` 定义 `FFrameCapture`、设置及状态；`Hyperion/RenderDoc/RenderDocPlugin.h` 提供生命周期插件。公共接口只有引擎与标准库类型。DLL、RenderDoc API 和 Windows 转换位于 Runtime/Capture 的 Private/Adapters，Renderer、RHI、DebugUI 不依赖具体捕获插件。

1. 调用 `Initialize()` / 启动插件必须早于首次 DXGI/D3D12 初始化。
2. `RequestCapture()` 接受至多一个待处理请求；`Status()` 返回同步后的副本。
3. 在 RHI 0 调用 `BeginFrame(Surface)`，等待它完成后再准备 GUI/模型 GPU 资源、录制和提交本帧。
4. 等待全部录制任务和 Present 完成，在 RHI 0 调用 `EndFrame()`。仅其返回 true 才表示本次抓取成功。
5. 图形异常时在同一协调线程调用 `Cancel()`；它只丢弃自身拥有的捕获。窗口最小化期间不开始捕获，待恢复绘制再执行；退出时取消待处理请求。
6. `OpenLastCapture()` 与捕获成功状态独立。自动打开只在本次 `EndFrame()` 成功后调用，不会因失败误开旧文件。

抓取文件使用实验名、进程/会话标识、请求序号作为前缀，再由 RenderDoc 附加文件后缀。服务读取本次新增记录，并确认路径匹配、文件存在且非空。`CompletedCaptures` 只统计本服务成功完成的请求；旧路径保留为最近成功记录，不代表后一次失败变成成功。

DLL 保留至进程退出，`Stop()` 不执行 `RemoveHooks()` 或 `FreeLibrary()`。运行中停用服务不能移除已经包装的图形对象；完全停用要重启。发现外部捕获已在进行时拒绝重叠请求，不修改 RenderDoc 全局热键。调用方仍应避免另一个工具在同一时刻发起捕获。

如果取消 API 返回失败且捕获仍在进行，服务保留该捕获的所有权，报告清理失败并禁用后续抓帧；`Cancel()` / `Shutdown()` 可以再次尝试清理。此时 `Initialize()` 不会重置仍活跃的状态。清理恢复后重启应用再继续抓帧。如果 API 返回失败但捕获实际已经停止，则允许正常重试抓帧。

RenderDoc 保存文件可能阻塞当前帧，模型越大耗时和文件越大。UI 自动打开采用 `LaunchReplayUI`，允许新建 RenderDoc 实例。返回 PID 只证明进程已启动，完整文件加载通过回放验收另行验证。D3D12 命令列表包含 RenderGraph Pass 名称的 BeginEvent/EndEvent，方便在 Event Browser 中定位。

## 自动验收

```powershell
# 第 8 和 16 帧抓取；帧号从 1 开始
.\out\build\vs2022\bin\Debug\hyperion_viewer.exe --frames 24 --hidden --capture-rdc 8 --capture-rdc 16

# 在真实 Viewer 中向 RDC 控件输入按下/释放事件，释放帧触发抓取
.\out\build\vs2022\bin\Debug\hyperion_viewer.exe --frames 24 --hidden --capture-rdc 8 --exercise-rdc-ui

.\tools\GenerateSolution.ps1 -RenderDoc -Test -Configuration Debug
.\tools\GenerateSolution.ps1 -RenderDoc -Test -Configuration Release
python tools/CheckStyle.py
python tools/CheckBoundaries.py
.\tools\Build.ps1 -RenderDoc
python tools/CheckStyle.py --naming --build-dir out/build/debug
```

`--capture-rdc N` 隐含启用插件，要求有限的 `--frames`，允许多次指定。UI 验收模式需要 debug-ui 和显示中的面板，并为释放预留下一帧。请求没有产生所需数量的 RDC 时进程返回失败。普通交互运行中的抓取失败仅反馈错误，保持 Viewer 运行。

新增验收包括：

- `capture_controls`：真实 Gui 控件的按下/释放、不可用/忙状态、操作分离、配置往返和高 DPI。
- `capture_unavailable` / `capture_incompatible`：缺 DLL、无 API 入口和不可用服务行为。
- `capture_failure_recovery`：在专用测试进程中注入取消失败，验证 Cancel/End/Shutdown 的所有权保留、清理重试，以及返回失败但实际已停止的恢复。该测试不创建 GPU 设备，注入代码不编入生产库。
- `capture_runtime`：真实 DLL 的设备创建前初始化、独占捕获、外部捕获保护、取消、重试、输出失败、Unicode 路径和已删除文件。
- `renderdoc_acceptance`：两个 Viewer 的控件触发、Triangle 连续抓取、每份 RDC 的 GPU 回放和 XML 检查。按命令列表的 Pass marker 分别验证场景与 GUI 的非零 draw 已提交，并检查 Present 和 pipeline；还覆盖运行关闭/缺 DLL/输出失败。

真实 RenderDoc 验收需要桌面会话和 D3D12 GPU。缺少运行库时相关测试显式 SKIP；不能据此声称抓帧已通过。Python 验收默认查找 `C:/Program Files/RenderDoc`，可用 `HYP_RENDERDOC_RUNTIME` 指定目录。C++ 服务验收使用默认安装位置。日志与 RDC 保存在构建目录的 `renderdoc-acceptance` 和 `capture-service-tests` 下，属于 `out` 产物。

## 2026-09-06 验证记录

环境：Windows x64、NVIDIA GeForce RTX 5080、RenderDoc v1.37 / Application API 1.6.0。

| 检查 | 结果 | 日志 |
|---|---|---|
| VS 2022 Debug，RenderDoc ON | 28/28，50.89 秒 | `out/RenderDocDebugTest.log` |
| VS 2022 Release，RenderDoc ON | 28/28，49.54 秒 | `out/RenderDocReleaseTest.log` |
| VS 2022 Debug，RenderDoc OFF | 24/24，34.31 秒 | `out/RenderDocDisabledTest.log` |
| OFF 构建显式请求插件 | 按预期失败并提示构建开关 | `out/RenderDocDisabledRequest.log` |
| Ninja Debug 与 clang-tidy | 构建通过；50 个编译单元命名通过 | `out/RenderDocNinjaBuild.log`、`out/RenderDocNaming.log` |
| 依赖边界、格式与 OpenSpec 严格校验 | 通过 | 可按上述命令复验 |

两个实验均由真实 Viewer 控件的按下/释放事件生成 RDC，并用安装的 `renderdoccmd replay --loops 2` 完成 GPU 回放。XML 验收分别检测到 Triangle 6 个、Model 9 个 indexed draw，包含场景、GUI、pipeline、marker 和 Present。全部 GPU 验收报告零 D3D12 validation errors。

最终面板的 GPU 回读截图为 `out/RenderDocTrianglePanel.png` 和 `out/RenderDocModelPanel.png`，已检查按钮、自动打开复选框、成功状态和路径布局。桌面鼠标自动化的应用授权超时，没有将它计为通过；上述控件验收使用引擎真实 GUI 输入管线。

自动打开另行验证：含中文/空格路径正确传给安装目录的 qrenderdoc；正常用户环境中再次执行后，进程 44600 的窗口标题为 `Triangle-78272-178910399411000-1_capture.rdc - RenderDoc v1.37`，与本次新捕获匹配。证据为 `out/RenderDocAutoOpen.log`、`out/RenderDocAutoOpenProcess.json`、`out/RenderDocUserOpen.log`、`out/RenderDocUserOpenProcess.json`、`out/RenderDocUserOpenWindow.json`。

验收后已将日常 VS 工程恢复为 RenderDoc ON 并重新构建 Debug，原始实验配置保持未启用状态，用户通过 `--renderdoc` 或面板保存配置选择启用。

后续独立审计确认并局部修复了内容验收误报和清理失败后的所有权问题，增加持久化失败注入测试。核实过程、未采纳的推断和新一轮验证见 [RenderDoc 独立审计记录](RenderDocReview.md)。

## 官方参考

- [Application API](https://github.com/baldurk/renderdoc/blob/v1.37/docs/in_application_api.rst)
- [固定版本 API 头文件与 MIT 许可](https://github.com/baldurk/renderdoc/blob/cd94206b0fd995bfb3e5ed95c1e68d4f5d38ea7e/renderdoc/api/app/renderdoc_app.h)
- [GPU marker 与资源名称](https://github.com/baldurk/renderdoc/blob/v1.37/docs/how/how_annotate_capture.rst)
