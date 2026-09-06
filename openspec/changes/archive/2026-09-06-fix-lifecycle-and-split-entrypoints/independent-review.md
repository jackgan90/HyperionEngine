# 独立审计与修复记录

2026-09-06。独立 reviewer：Kuhn，agent ID `01a07707-9da5-7af1-9b5c-d7c06bd8cc65`，以 `fork_context=false` 启动，未继承实现对话。初始审计以 HEAD `a32fbcd791a2e748c9e09b190a88b7db10587b8c` 为基线，覆盖当时完整 change 的 40 个文件。

**最终结论：一项 P2 经独立复核确认并最小修复，reviewer 复审关闭；未发现其他必须修复的回归或明显安全/性能问题。** 未进行超出该 change 的架构重构。

## F1：Present 失败遗漏取消等待

EndFrame 已提交命令，Present 返回失败但 Signal 成功时，原状态顺序先清除 Active，再检查 HRESULT。ExecuteGraph 捕获异常后的 CancelFrame 因此直接返回，未等待尚未完成的 GPU 工作。这是本 change 新增取消契约的遗漏。

原始审计明确指出：Retained 和 fence 仍持有资源，未复现提前释放、崩溃或设备损坏。父代理复核了 EndFrame、CancelFrame、DeviceState::Signal/Idle/Wait 和新增规格，接受“取消等待遗漏”的结论，没有将它升级为未经证实的内存安全回归。

父代理检查故障注入代码后独立复跑：使用真实 D3D12 队列和延迟 500ms 的 fence gate，在 Present 调用点注入 E_FAIL；旧顺序约 1ms 返回，gate 未释放且 completed fence 2 < issued fence 3。仅调整赋值顺序的副本约 501ms 返回，completed/issued fence 均为 4。两版均能在清理后读回 64×64 图像，validation errors 为 0。

证据：`out/IndependentReview/ParentSubmissionBefore.log`、`ParentSubmissionFixed.log`。原始 reviewer 报告及复审分别位于 [Findings.md](../../../../out/IndependentReview/Reviewer/Findings.md) 和 [ReReview.md](../../../../out/IndependentReview/Reviewer/ReReview.md)。

## 最小修复与常驻回归

- 核心行为修复只将 `Active = false` 移至 `Check(Hr, "Present swapchain")` 成功之后。提交统计位置保持一致；失败时由现有 CancelFrame 等待 GPU 完成后清除状态。
- 新增后端私有 `D3D12FrameFailureTests.cpp`，真实队列暂缓工作、Present 单次返回 E_FAIL，断言原错误返回时 fence 已完成，随后验证重复取消、下一帧颜色读回及零 GPU validation error。
- 新 CTest 项为 `d3d12_frame_failure_recovery`，加入 hyperion_check，设置 gpu/desktop、RUN_SERIAL、TIMEOUT 60。
- `HYP_TEST_D3D12_PRESENT` 只编入专用测试目标。reviewer 核查编译命令及对象符号，父代理也核对编译数据库：生产库没有测试宏或 PresentForTesting 引用，正常路径继续直接调用 DXGI，没有新增等待或运行时分派。

先接入新回归、保留旧赋值顺序，测试明确失败于 `CheckCondition(Drained)`（第 108 行）；随后移动赋值，完全相同的测试通过。证据：`out/IndependentReview/RegressionBefore.log`、`RegressionAfter.log` 及对应 RegressionBuildBefore/After.log。reviewer 静态复审、只读符号检查和日志复核后确认 F1 关闭；最终完整构建与运行由父代理执行。

## 其他独立审计证据

- 分别将 Window、AssetService、RenderGraph 实现替换回 HEAD，三个新回归各自失败，当前实现各自通过。父代理核对了探针源码和错误日志，确认失败来自原有三个问题。
- HEAD/current importer 的 20 个 glTF/GLB fixture 输出逐字节一致，包括成功模型的完整序列化和失败错误。父代理自行比较两侧文件集合和字节内容，确认 20/20 一致。
- HEAD/current Viewer 均通过窗口、UI、配置往返、后端/插件错误、glTF/GLB 显示、缺失依赖和保存失败收尾验收。独立编译的 RenderDoc OFF 版本正常渲染并拒绝未编译插件。
- reviewer 实际完成 RenderDoc 两个 Triangle 与一个 Model 抓帧的回放及 XML draw 核对。审计初次输入副本遗漏模型导致的失败在补齐原始输入后消失，未记为仓库问题。
- 初始审计期间 40 个文件的 SHA256 保持一致；父代理在修改前独立确认。接口清单核对显示 19 个 CLI 选项未改变，既有 CMake target 未删除或改名。证据保存在 `out/IndependentReview/StartManifest.json`、`ContractInventory.json`、`ParentEvidenceCheck.json`。

## 最终验证

| 检查 | 结果 | 日志 |
| --- | --- | --- |
| 新增故障回归（修复前 / 后） | Drained 断言失败 / 1/1 通过 | RegressionBefore.log / RegressionAfter.log |
| VS Debug 完整构建与 CTest | 31/31，57.37 秒 | FinalVsDebug.log |
| VS Release 完整构建与 CTest | 31/31，55.71 秒 | FinalVsRelease.log |
| RenderDoc OFF Debug 完整构建与 CTest | 26/26，27.96 秒；未编译插件提示正确 | FinalDisabledBuild.log / FinalDisabledTest.log |
| 格式、文件名和 include 大小写 | 107 个源文件通过 | FinalNaming.log |
| C++ 命名与局部声明 | 61 个编译单元通过 | FinalNaming.log |
| 模块边界 | 103 个源/头文件、23 个模块通过 | tools/CheckBoundaries.py |
| OpenSpec strict / diff whitespace | 通过 | openspec validate --all --strict / git diff --check |

上述日志位于 `out/IndependentReview/`。新测试最长函数 38 行，调整后的 EndFrame 为 75 行；已有 Viewer/importer 拆分不再修改。常用 VS/Ninja 构建仍保留 RenderDoc ON，OFF 使用独立目录。

## 验证边界与范围控制

未人为触发真实设备移除、驱动失效、GPU 超时、所有 Signal/Map/分配失败，不能据此保证所有硬件故障可恢复。新测试的 gate 固定延迟 500ms，极端调度停顿可能让工作在观察前自然完成；本次修复前实际失败于 Drained 的负例证实测试确实建立并检出了该故障，reviewer 将此调度敏感性列为非阻塞测试边界。

原审计探针保留的是审计时快照；日常回归使用常驻 CTest 项。任务系统、通用资源图、缓存预算/热重载、反射统一和其他已延后架构问题继续保持本 change 的非目标范围。
