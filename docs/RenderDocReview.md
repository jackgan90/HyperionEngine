# RenderDoc 独立审计与修复

日期：2026-09-06。范围为可选 RenderDoc 插件、引擎抓帧接口、Triangle / Model Viewer 控件、自动打开与构建验收。

reviewer 以 `fork_turns: none` 启动，仅收到仓库位置、功能要求和只读审计任务，没有继承主对话。它独立检查源码、规范、API 契约和捕获 XML；主 agent 对发现逐项复核、复现并实施局部修复。reviewer 随后复审修复及新增测试，未发现新的确认问题。没有需要大规模重构的发现。

## 确认的问题

| 优先级 | 发现与主 agent 复核 | 最小修复 |
| --- | --- | --- |
| P2 | 原内容验收仅要求总 draw 数至少 2、任意 marker、Present 和 pipeline。reviewer 在内存中移除场景命令后仍能通过；主 agent 另用仅启用 debug-ui 的配置生成真实 RDC，确认其 5 个 GUI draw 足以误通过。正常原始捕获确实包含场景，此项是验收缺口。 | 对每份 RDC 按命令列表追踪 marker 范围，只计入已提交且索引数、实例数均非零的 draw，分别要求 Triangle / Static model 与 Debug UI 存在。原来只检查最后一份的行为也已修正。 |
| P2 | reviewer 将取消失败后无条件清除 OwnsCapture 列为未证实风险。主 agent 编译实际 FrameCapture.cpp，注入“第一次 Discard 返回 0 且仍在捕获，第二次成功”：旧实现第二次 Cancel 不再调用 API，捕获仍活跃；同一探针在修复后调用两次并成功停止。 | 取消失败且仍在捕获时保留所有权，禁用新请求并显示清理失败；Cancel / Shutdown 仍能重试。Initialize 不覆盖尚未清理的所有权。返回失败但捕获已停止时正常恢复。 |

第二项是依据 [RenderDoc API 的错误返回契约](https://github.com/baldurk/renderdoc/blob/v1.37/renderdoc/api/app/renderdoc_app.h) 进行的受控失败验证，不能表述为本机正常 GPU 捕获曾自然发生该故障。生产代码只改了局部清理与状态处理，没有修改公共 API、渲染线程结构或插件架构。

## 复核边界

- 含点号的文件名前缀曾被怀疑会被截断；直接调用本机 v1.37 API 未复现，因此未修改前缀处理。
- 未额外根据 UI 中可能滞后的 Ready 文本判断模型是否已捕获；以 RDC 内已提交的 Static model 非零 draw 为证据。
- 内容检查证明相应绘制命令存在并已提交，不替代模型材质、像素正确性测试。实际回放及原有模型图像验收继续执行。
- 没有宣称覆盖所有驱动故障、外部工具同时发起捕获的线程交错或新的图形后端。

## 回归与验证

新增 CTest `capture_failure_recovery` 覆盖 Cancel、EndFrame、Shutdown 清理失败但仍活跃，以及 API 返回失败但实际已停止四条路径；检查所有权保留、禁止新请求、初始化保护和再次清理。API 函数表注入只发生在专用测试进程中，并通过 RAII 恢复；测试不创建 GPU 对象。为遵守第三方接口边界，源码位于 Runtime/Capture/Private/Adapters，仅由 BUILD_TESTING 下的独立测试目标编译，不进入生产库。

原始真实 Triangle / Model XML 通过新验证，分别包含 1 / 4 个场景 draw 和各 5 个 UI draw。真实 UI-only 文件被拒绝；主 agent 和 reviewer 分别验证了移除场景 draw、marker、命令提交及零实例数等内存反例被拒绝。每次成功抓取的 RDC 均独立回放和检查。

| 检查 | 最终结果 | 本地证据 |
| --- | --- | --- |
| 修复前基线 | 抓帧相关 5/5 | `out/RenderDocReviewBaseline.log` |
| 内容反例复核 | 正例通过，缺场景的反例拒绝 | `out/RenderDocReviewContentChecks.log`、`out/RenderDocReviewUiOnly/UiOnly.xml` |
| 取消失败探针 | 旧实现失败，修复后通过 | `out/RenderDocReviewDiscardBefore.log`、`out/RenderDocReviewDiscardAfter.log` |
| VS 2022 Debug / RenderDoc ON | 29/29，55.33 秒 | `out/RenderDocReviewDebug.log` |
| VS 2022 Release / RenderDoc ON | 29/29，45.37 秒 | `out/RenderDocReviewRelease.log` |
| VS 2022 Debug / RenderDoc OFF | 24/24，36.64 秒 | `out/RenderDocReviewDisabled.log` |
| Ninja 测试目标构建、格式及语义命名 | 通过，51 个编译单元 | `out/RenderDocReviewNinja.log`、`out/RenderDocReviewStyle.log` |
| 模块边界、OpenSpec strict | 89 个实现文件 / 23 个模块通过；规格 20/20 | CTest `dependency_boundaries`、`out/RenderDocReviewOpenSpec.log` |

验收后已恢复日常 VS 工程为 RenderDoc ON 并重新构建 Debug，日志为 `out/RenderDocReviewRestore.log`。

源码：[捕获清理](../Source/Runtime/Capture/Private/Adapters/FrameCapture.cpp)、[失败回归](../Source/Runtime/Capture/Private/Adapters/FrameCaptureFailureTests.cpp)、[RDC 内容验收](../Source/Tests/Integration/RenderDocAcceptance.py)。
