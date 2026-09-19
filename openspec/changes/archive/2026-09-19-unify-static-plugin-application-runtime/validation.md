# 验证记录

本变更保持静态链接和启动时选择，没有运行中加载/卸载。实现与审计阶段未暂存或提交代码；用户随后明确授权 OpenSpec 归档和 Git 提交。架构、服务依赖、扩展方式和配置迁移见 [PluginSystem.md](../../../../docs/PluginSystem.md)。

独立质量审计、三个运行时 P2 修复及后续 graphics 最终检查和等待异常修复见 [审计记录](audit.md)；以下完整回归和裁剪构建属于该审计之前的验证，审计后的实际检查单独记录。

## Graphics 停机检查后续验证

- 最终检查归 graphics owner，在 Swapchain 清理后、Device 析构前读取 validation，再在 Main 报告失败；等待异常也在 RHI 0 按 Swapchain、Device 顺序清理。
- 真实 D3D12 晚到错误注入与假后端等待异常两项测试均验证修复前失败、修复后通过，且覆盖重复 Stop。独立 reviewer 复审通过并单独运行等待异常用例。
- Visual Studio Debug 四个相关目标构建成功；最终相关回归 **7/7 通过**，包括两个新用例、RHI 契约、D3D12 帧故障恢复、插件应用以及 Viewer/Editor 验收。
- 格式、路径、4 个 C++ 翻译单元语义命名、依赖边界、OpenSpec strict 和 diff 检查通过。本项未重跑完整回归、裁剪构建或 Release；具体证据和验证边界见审计记录最后一节。

## 构建与回归

- Visual Studio Debug 全部目标构建成功。
- 完整 CTest 共运行 81 项，初次 80 项通过。唯一失败是 RenderDoc 验收仍要求旧的 `Debug UI/0` 标记；共享 GuiRenderer 使用会话准备路径后标记为 `GUI/0`。实际 RDC XML 确认提交了 5 个 GUI 绘制，场景和 tonemap 提交也存在。验收更新为识别两个有效路径，仍要求真实提交且非空的 GUI draw。
- 最终相关复测 **6/6 通过**：plugin_runtime、plugin_applications、d3d12_gui、viewer_acceptance、cpu_frame_capture、renderdoc_acceptance。包括真实 RenderDoc 捕获、XML 提交检查和 GPU replay。没有剩余失败项。
- 完整回归其余项目涵盖 Editor 交互、场景、Forward/Deferred、两种深度约定、CSM、HZB/contact shadows、异步资产、CPU 帧所有权、排队帧和故障恢复。
- 关闭 HYP_ENABLE_TRIANGLE、HYP_ENABLE_MODEL_VIEWER、HYP_ENABLE_SCENE_VIEWER、HYP_ENABLE_DEBUG_UI、HYP_ENABLE_RENDERDOC 的 Ninja Debug 配置成功编译 Viewer、Editor 和独立测试，裁剪配置 **8/8 通过**。额外断言确认这些插件及 Runtime/Capture 的源码不存在于编译数据库，对应库不存在于 Ninja 链接图。

## 新增及调整的覆盖

- 空宿主、不创建窗口/设备；插件失败分支与无关分支隔离；显式禁用依赖；可选排序不引入插件；依赖环和重复服务拒绝。
- 服务声明校验、线程约束、失败发布回滚、事件退订、逆序清理、任务汇合和清理异常上报。
- Viewer/Editor 禁用 graphics；残留 scene_source 不启用插件；缺失插件允许退化；请求输出而对应能力不可用时报告失败；未编译抓帧服务也不能把未生成的 RDC 当作成功。
- 渲染阶段顺序、注册封闭、独立 viewport feature 实例，以及不注册 contact feature 时零 HZB/contact 资源；现有 GPU 回归继续覆盖开启、关闭、debug、resize 和两种深度约定。

## 静态检查与工作区

- CheckStyle：612 个自有源码文件的路径与格式通过。
- CheckStyle 语义命名：本次变更的 51 个 C++ 翻译单元通过，包括布尔变量规则。
- CheckBoundaries：579 个源码文件、35 个模块通过。
- `git diff --check` 通过；OpenSpec strict validation 通过。
- AGENTS.md 强制要求功能接入前阅读并遵守 PluginSystem.md；该文档集中规定最小宿主、可选性、生命周期、通信、线程和验收边界，SourceLayout.md 与文档索引提供必读入口。后续改变原则须在 OpenSpec 设计中说明，并同步契约和回归覆盖。
- 临时验收数据保存在被忽略的 `out/`。Ninja 配置恢复为原来的完整插件配置；默认运行产物也恢复完整构建。
- 实现与审计阶段保留 proposal/design/specs/tasks/validation 供审阅；归档与 Git 提交在用户后续明确授权后执行。

## 本地生成证据

以下日志为本次生成产物，不要求后续检出保留它们：

- `out/PluginFullBuild.log`、`out/PluginFullTests.log`
- `out/PluginFinalBuild.log`、`out/PluginFinalTests.log`
- `out/PluginMinimalConfigure.log`、`out/PluginMinimalBuild.log`、`out/PluginMinimalTests.log`
- `out/PluginNaming.log`、`out/PluginRestoreConfigure.log`、`out/PluginRestoredBuild.log`

采用最终相关复测记录判断旧 RenderDoc 标记失败的修复结果；不把首次全量运行的原始日志改写为全通过。
