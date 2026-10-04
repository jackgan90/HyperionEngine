# GBuffer 契约验证记录

## 范围

基线 `13c997a6a0226a8dcbd78594eb259eb0c353ea29`。Renderer 单一角色描述表与旧四槽位/semantic/格式对应一致；布局、分配及 fullscreen/debug/local/contact 消费者复用。shader 编码、MRT 顺序、缓存与退休不变。单通道格式从后续字节计算报错提前到布局校验拒绝。

## 构建与回归

- Debug、Release 全量构建通过：`out/InteractionContractsDebugBuild.log`、`out/InteractionContractsReleaseBuild.log`。
- 两配置各 4 项直接回归通过：交互策略、GBuffer 布局、对象放置、资产文档。日志为 `out/InteractionContractsFocusedTests.log` 与 `out/InteractionContractsReleaseFocusedTests.log`。
- Debug 集成回归 46/46 通过；连同直接回归，去重覆盖 49 项。`out/InteractionContractsDebugTests.log`。
- Release 集成首轮 45/46 通过；唯一 editor_placement 在放置后的 Ctrl+V 报 Windows OpenClipboard 访问失败。源码中错误来自 Platform/Adapters/Clipboard.cpp 的 OpenClipboard 失败分支。未改代码，单独复跑 1/1 通过，最终去重覆盖同样 49 项。`out/InteractionContractsReleaseTests.log`、`out/InteractionContractsReleasePlacementRetest.log`。未认定具体占用进程。
- 覆盖真实 Editor 导航/拾取/Gizmo、快捷键/多选/剪贴板/层级/放置、资产窗口、关闭和内容切换；覆盖 Deferred、contact/HZB、深度约定、资源与实例批次；覆盖 shader、GUI、共享资产/场景 automation、discovery/attachment/parity、插件缺失/禁用及最终 GPU 关闭校验。
- 两配置实际 hyperion_check 生成规则均包含新增测试 exe；结果汇总与文件哈希位于 `out/InteractionContractsResults.json`、`out/InteractionContractsManifest.json`。

## 规范与独立审查

CheckStyle 通过 1138 个源文件的路径/格式检查；修改范围语义命名通过 28 个 translation units；CheckBoundaries 通过 1099 源文件、39 模块。git diff --check 通过。OpenSpec 全量 strict 校验 113/113 通过。

quality-audit 使用不继承上下文的独立 reviewer，覆盖本轮全部 unstaged/新增源码、文档和四个 OpenSpec；结论为 0 项可确认 findings。Reviewer 独立运行四项 Debug 直接回归均通过；真实 Editor/GPU 和 Release 由主 agent 完成。审核后源码与产品文档哈希未变化。

人工复核新增生产文件均低于 500 行，新增函数按职责拆分；唯一触及的既有超限生产文件 AssetWorkspace.cpp 为 603 行（原 605），本轮仅替换基础支持判断，不扩大为 workspace 生命周期拆分。语义身份、共享事实归属、分入口策略和资源所有权已单独复核。

## 交付状态

实现、文档与验证完成后，用户于 2026-10-04 授权 OpenSpec 归档并创建本地 Git 提交。本记录随 change 归档，主规范同步后按已审查范围冻结文件、核对暂存内容与提交树；不推送。
