# 静态插件与应用宿主

插件是同一进程内的逻辑组织单位。编译时决定可用的静态库，启动时决定本次参与的插件；运行中不加载、卸载或重新选择插件。`Runtime/Application` 仅依赖 Core、Tasks、Plugins，拥有任务系统、Main 消息泵、帧时钟和退出状态；窗口、资产、GPU、GUI 和应用业务均由插件持有。

## 开发强制约束

本节由根目录 [AGENTS.md](../AGENTS.md) 要求遵守，适用于新增功能和现有功能改造。后续各节定义具体接口与行为；评审时必须检查以下规则。

1. **宿主保持最小。** 不得向 `Runtime/Application` 加入窗口、资产、渲染、GUI 或具体插件分支。Applications 负责选择插件目录与原生 provider，功能编排和资源生命周期归所属插件。可复用算法、数据结构和引擎接口仍放在 Runtime；不要求把每个类、面板或 render pass 都变成插件。
2. **可选性必须真实。** 缺失、禁用或启动失败的插件只影响自身及硬依赖分支，不得令无关功能崩溃；可选服务使用 `Find` 并处理缺失，硬依赖必须声明。明确要求的能力不可用时应给出受控失败，不得伪报成功。保存的数据字段不得隐式启用插件，显式禁用始终优先；不得自行引入运行中加载或重选。
3. **生命周期归框架。** 插件通过 `Start/Update/Quiesce/Stop` 管理功能，依赖顺序写入 descriptor；不得在应用主循环中手动逐个启动、更新或销毁具体插件。任务、订阅和注册必须随插件作用域清理，失败的部分初始化也须安全收尾；销毁状态前汇合任务，GPU 资源继续按 fence 退休。
4. **跨插件依赖面向契约。** 必需能力或有返回值的操作使用引擎自有类型化服务、领域接口；通知与多方贡献使用作用域事件。不得通过具体插件类型转换、私有头文件或进程全局 service locator 绕过依赖声明。组合入口可引用具体插件的注册接口。
5. **线程与扩展边界明确。** 生命周期、服务查找和事件分发在 Main；跨域传递拥有数据所有权的快照，不共享可变 Context。渲染贡献通过 `IRenderFeature` 阶段并声明 RenderGraph 资源读写，GUI 贡献使用面板事件；不要为新功能在宿主增加特判。
6. **变更必须可验证。** 生命周期或依赖变更须覆盖受影响的缺失、禁用、启动失败和关闭路径；宿主变更须保持空插件运行，构建开关变更须验证关闭后的编译和链接。使用已有 `plugin_runtime`、`plugin_applications`、相关功能回归及 `CheckBoundaries.py`；自动依赖检查不能替代上述架构评审。确需改变原则时，必须先在 OpenSpec 设计中明确理由，并在同一变更中同步本契约及回归覆盖。

## 组合与可选性

Viewer、Editor 的可执行入口位于 `Applications`，只选择 D3D12 provider 并调用各自的启动组合。业务位于 `Plugins/Viewer`、`Plugins/Editor`，不直接包含 native backend。`Plugins/ApplicationServices` 提供以下逻辑插件，不要求每个逻辑插件对应一个二进制文件。

| ID | 所有权 / 服务 | 依赖 |
|---|---|---|
| `assets` | 挂载文件系统、IO、资产服务及目录注册 | 宿主 Tasks |
| `window` | FWindow、Main 事件轮询 | 无 |
| `graphics` | Device、Swapchain、ShaderCompiler、RenderSession、渲染扩展注册表 | assets、window |
| `gui` | FGui、通用 FGuiRenderer | graphics |
| `contact-shadows` | 向图形服务注册 contact-shadow 工厂 | graphics |
| `triangle` / `model-viewer` / `scene-viewer` | IScenePlugin；后者另有 ISceneEditor | graphics，按需 assets |
| `debug-ui` | 诊断面板事件订阅 | gui |
| `viewer` | 输入、帧提交、截图、实验设置 | graphics；可选 scene、GUI、capture |
| `editor` | 文档、历史、浏览相机、面板、viewport | gui |
| `renderdoc` | 可选 FFrameCapture | 先于 window、graphics |

`FPluginDescriptor` 声明 ID、factory、硬依赖 `Dependencies`、`Provides/Requires/Optional` 服务和 `Before/After/Conflicts`。硬依赖按 ID 递归选入；类型化服务约束只在已选插件之间连边，不会猜测应该启用哪个实现。可选服务存在时先启动 provider，缺失时 `Find<T>()` 返回空。`Before/After` 只约束已选插件，不会触发加载；RenderDoc 用这一点在图形初始化前安装 hooks。

启动先规划，再构造实例。重复 ID、同一服务的多个已选 provider、冲突或依赖环是配置错误。`FPluginSelection` 默认使用 Continue：缺失、禁用或启动失败的分支会记录诊断，硬依赖消费者跳过，不相关分支继续。Strict 模式保留遇错回滚整个集合的行为；旧的 `Activate(span<id>)` 仍为 Strict。应用主体因真实启动错误无法运行时，可执行程序报告失败；显式禁用 graphics 等分支可以作为无图形配置正常退出。插件不是进程故障隔离边界，不承诺恢复任意 C++ 内存错误。

## 生命周期和线程

启动按拓扑顺序执行 `Start(FPluginContext&)`。宿主在 Main 泵消息后调用 `Update(FPluginUpdate)`，提供 frame、elapsed、delta；更新不以窗口可绘制为前提。Editor 最小化时仍推进场景加载和保存；Viewer 的场景接口接收 delta，相机推进在场景插件内部完成。

停止先断开所有事件，再逆序 `Quiesce()`，阻止生产新帧并汇合已提交帧；随后逐个逆序执行作用域 cleanup、`Stop()`、移除服务、析构实例。消费者清理期间其 provider 仍然存活。失败的 Start 也执行对应清理。`Defer()` 注册按逆序调用的 cleanup，某个 cleanup 抛错不会阻止其余清理，宿主最终报告第一个清理错误。`TrackPluginTask()` 可将任务 join 纳入该作用域；任务必须在捕获的插件状态销毁前完成。

生命周期、服务查找和事件分发属于创建宿主的 Main 线程。服务的具体方法仍须遵守自身线程契约。不得把 Context 或服务注册表作为 Render/RHI 的共享可变状态：Main 注入长寿命服务引用，跨域工作传递 owned snapshot。Viewer 仍通过 FFramePipeline 控制 Main/Render/RHI lead；GPU native handle 的保活仍由资源协调器和 fence 负责。Quiesce 或 CPU task 完成不等于 GPU 完成。

最终 GPU validation 检查归拥有 Device 的 `graphics` 插件：消费者和 RenderSession 关闭后，在 RHI 0 等待 GPU、释放 Swapchain、读取最终统计并销毁 Device，再回 Main 向 FApplicationControl 报告错误。等待 GPU 或读取统计失败时，也须在 RHI 0 按 Swapchain、Device 的顺序释放资源。Viewer/Editor 较早的统计仅覆盖各自停止时刻，不能替代这项最终检查。

## 服务与事件

`Context.Provide<T>(object)` 只允许发布声明过的类型，注册表不拥有 object。`Require<T>()` 用于硬依赖，`Find<T>()` 用于可选依赖，两者均要求 descriptor 声明。宿主提供的 Tasks、FApplicationControl 也通过这些接口注入。避免在模块内增加进程全局 service locator。

`Context.Subscribe<T>()` / `Publish<T>()` 是 Main 上的同步类型化事件；回调只在本次调用期间借用事件对象，不能跨线程保存引用。订阅随作用域断开；停止后不能重新订阅或发送事件。工作线程的完成通知先通过 Tasks 投递到 Main，再发布事件。

需要返回结果或表达必需能力时使用接口调用，例如 IScenePlugin 的 Input、Update、Ready、Status、Error，以及 ISceneEditor 的编辑和保存能力。通知或多个独立贡献者使用事件。`FDebugPanelEvent` 将诊断数据和操作结果连接到可选 DebugUI；`FGuiPanelEvent` 在 BeginFrame/Render 之间允许 Viewer、Editor 的扩展绘制额外面板。Editor 内置文档面板目前仍是同一个内聚插件，没有拆成每面板独立 DLL。

## 渲染扩展

`FRenderFeatureRegistry` 在启动时注册工厂，每个 viewport 调用 `Create()` 获得自己的 feature 实例，并封闭后续注册。注册插件须先于 viewport 消费者启动；作用域 cleanup 在消费者停止后移除工厂。`IRenderFeature` 不在 Main 逐帧查找插件；它在 Render 接收包含 graph、不可变 frame/view、设置和显式资源的同步上下文。

阶段依次为 AfterOpaque、BeforeLighting（Deferred）、BeforeTonemap、AfterTonemap。BeginFrame/EndFrame 组织每帧临时状态；无有效相机时 Reset 释放独占缓存。上下文公开 scene depth、color、GBuffer、output 及可选 DirectionalVisibility，资源携带 retention token。所有 pass 仍须通过 RenderGraph 声明真实读写，deferred preparation 只能捕获 owned 数据。

Contact-shadow feature 自行管理 HZB 请求、visibility mask、debug preview 和缓存退休；lighting 读取它发布的 DirectionalVisibility。未注册时不产生 contact/HZB 独占资源，普通光照继续工作。Forward、无方向光、禁止场景阴影的约束保持不变。独立 Renderer 使用者的三参数 pipeline 构造保留默认 feature 工厂以兼容现有调用；应用宿主显式传入本次选择的列表，空列表即不启用扩展。

## 配置与构建

`plugins` 决定实验插件；`disabled_plugins` 具有最终优先级，可阻止被依赖拉起的插件。Viewer profile 默认加入 viewer 和 contact-shadows，Editor profile 默认加入 editor 和 contact-shadows。保存的 `scene_source`、`model_source` 只是数据，不再修改插件选择；`--scene`、`--model` 作为显式便捷命令仍选择对应插件，但不能覆盖 disable。诊断 UI 中修改列表在重启后生效。

```powershell
# 无窗口、无资产挂载、无 GPU；可用于验证宿主
hyperion_viewer --kernel-only --frames 8
hyperion_editor --kernel-only --frames 8

# 运行时静态选择（重启生效）
hyperion_viewer --disable-plugin contact-shadows
hyperion_viewer --disable-plugin scene-viewer

# 不编译以下可选实验插件；应用仍可运行清屏和 Editor
cmake -S . -B out/build/vs2022 -DHYP_ENABLE_TRIANGLE=OFF -DHYP_ENABLE_MODEL_VIEWER=OFF -DHYP_ENABLE_SCENE_VIEWER=OFF -DHYP_ENABLE_DEBUG_UI=OFF
```

上述四个开关默认 ON，RenderDoc 仍由默认 OFF 的 HYP_ENABLE_RENDERDOC 控制。被排除的库不进入 Viewer 链接；配置请求未编译插件会给出缺失诊断。关闭任一实验库时 CTest 使用 PluginProfiles.cmake 中不依赖这些库的宿主/运行时验收集；完整图形、场景和 GUI 回归使用默认全部启用的构建。`plugin_runtime` 覆盖生命周期和服务作用域，`plugin_applications` 覆盖两个应用的空宿主、缺失、禁用和残留数据配置，Deferred 回归覆盖阶段顺序及无 contact feature 的光照路径。
