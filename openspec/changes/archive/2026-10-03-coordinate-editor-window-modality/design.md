## Context

主窗口内的 ImGui modal 无法跨 HWND 保证遮挡与输入。旧临时修复修改 SDL parent 并使主窗口成为资产窗口的 owned modal；第二个附加窗口无法组合，最小化主窗口也不会隐藏原先的 owner。应用组合与共享文档服务不需要改变。

## Goals / Non-Goals

**Goals:** 一个主窗口、多个直接附加窗口；统一模态状态；作用域注册；保留正常 ownership、显隐、最小化/最大化、enabled 状态；动态增删与失败隔离；主线程生命周期。

**Non-Goals:** 任意嵌套或跨组 modal stack；全局 topmost；新的文档操作、automation schema 或插件生命周期；修改 SDL 依赖源码。

## Decisions

1. `FWindowGroup` 提供 `Register`、`SetModalActive`、`Synchronize`、`IsInputBlocked`；`FWindowRegistration` 是 move-only RAII。调用与清理均在窗口所属线程；重复/跨组注册、非直接 owner、已注册窗口变更 owner 被拒绝。主窗口不允许同时属于另一个组。注册应在窗口创建及 SetOwner 后、暴露输入前完成，注销在原生窗口销毁前完成。弱引用 token 在组销毁后安全失效；FWindow 销毁钩子移除已失效原生资源的风险。
2. Platform 私有适配器捕获 auxiliary HWND 的 owner/enabled/visibility；进入时仅临时清除 auxiliary 原生 owner、禁止输入、置于 Main 后方。SDL parent 保持原样，主窗口 owner 从不改变。退出恢复快照及正常 owner 的层级。拒绝在注册期通过其他通道直接修改 native owner/enabled；这些字段归组独占协调。无需另建 engine ownership 树或重写 SDL registry。
3. Synchronize 在输入接纳前、主窗口 GUI 操作后和每轮 Update 末尾运行，包括主窗口不绘制时。仅在实际 native 状态不同才写入；不重复抢前台。进入 modal 时请求 Main 激活；附加窗口 Raise 在受阻时重定向 Main。保持 domain busy 检查与 GUI input reset。
4. 主窗口最小化时隐藏被临时解除 native ownership 的附加窗口；恢复时只恢复原先可见窗口。隐藏/显示使用保持 placement 的原生 SetWindowPos，并检查 SDL 和 HWND 的 min/max 状态。取消 modal 时若 Main 仍最小化，允许延后恢复 native ownership/输入到 Main 恢复，防止提前露出附加窗口；被延后窗口仍由组保护。不得将本来隐藏/最小化/禁用窗口强行恢复。
5. 注册失败只撤销新窗口；批量进入失败回滚已经应用的成员；注销只恢复自身；退出与析构清理所有成员。若注销或组析构时 Main 仍最小化，owner/enabled 立即恢复，未完成的显示恢复移交 Platform 私有待办，由 FWindow::Poll 在 Main 恢复后执行；窗口销毁取消该待办，重新进入 modal 接管待恢复的可见性。原生 API 错误抛出，析构无法抛出时在低频清理边界报告，不伪造成功。不支持的平台显式报告窗口组不可用。
6. Editor 持有组并将现有模态谓词推广为附加窗口阻塞状态；资产宿主持有注册 token，移除反转 owner 的代码。后续工具窗口使用相同注册，无需增加 modal 分支。无需新增 automation adapter，因为这是已有关闭/保存/打开操作的呈现修复；GUI 和 agent 继续调用原有共享服务。

## Risks / Trade-offs

- Windows 原生 owner 与 SDL parent 暂时不同 → 只在 Platform 私有实现内部操作，封锁注册期 SetOwner，销毁前清理；测试重复切换、动态销毁与全部退出。
- Windows 显隐消息影响 SDL flags → 使用保持 placement 的 API 并同时验证两层状态。
- 输入已入队或 GUI 保有拖拽状态 → 保留宿主已有 input reset 与共享服务 busy 检查。
- 预估生产代码新增约 350–500 行，测试约 220–350 行；若 SDLWindow 触及 500 行限制，仅按输入翻译的完整逻辑边界搬移，不改语义。

## Migration Plan

完成组/适配器、接入 Editor、替换原生测试与补充真实 Editor 验收，同步插件文档。运行格式/命名/模块边界、相关构建与 CTest，原 reviewer 独立复审。变更保持 active，不归档、不提交；无持久化迁移。

## Open Questions

无阻塞问题；实际 Windows min/max/visibility 及失败回滚由实现期测试验证。

## Review follow-up

注销/组析构后的延迟显隐不再依赖组存活；`FWindow::Poll` 执行待办，窗口销毁撤销待办。注销后成功 `SetOwner` 时将待办转交新 owner；解除 owner 时下一次 Poll 恢复可见性，旧 Main 不再控制该窗口。以上边界由独立复现和正式回归覆盖，结果记录在 tasks.md。
