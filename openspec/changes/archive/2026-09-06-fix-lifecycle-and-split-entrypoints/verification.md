# 验证记录

本页保留独立审计前的初始实现记录与改动规模。随后独立审计确认并最小修复了一处 Present 失败取消等待遗漏；最终 Debug/Release 为 31/31、RenderDoc OFF 为 26/26，详见 [独立审计与修复记录](independent-review.md)。

2026-09-06，Windows / VS 2022 Community / MSVC 19.38；Ninja 编译数据库使用本机 MSVC 19.50 Build Tools。

## 范围与回归

- 窗口：两个窗口按两种顺序销毁，保留窗口仍能查询尺寸、缩放和轮询；全部销毁后可以重新创建。
- 帧恢复：真实 D3D12 后端连续三次执行失败图后成功渲染并读回已知颜色；取消可重复调用，旧录制结果在新帧被拒绝，GPU validation errors 为 0。
- 录制时序：公共 RHI mock 覆盖一个录制任务抛错而另一任务仍在运行的情况，取消时后者必须完成；覆盖 EndFrame 抛错后的下一帧成功。
- 资产重试：缺少外部依赖时加载失败，补齐文件后重新请求成功；重试共享一个生产者、旧请求保留错误、取消不影响其他消费者、其他成功缓存保持有效。
- Viewer/importer：使用现有导入、材质像素、窗口、模型、配置保存失败收尾、RenderDoc 抓帧及交互验收检查行为保持一致。

设备丢失或 GPU 无法完成工作时仍允许取消操作报错。本轮真实 D3D12 注入的是录制错误；结束帧异常由 mock 覆盖，没有人为制造驱动/GPU 故障。

## 已完成检查

| 检查 | 结果 | 日志 |
| --- | --- | --- |
| Ninja 构建与三项修复的重点回归 | 构建成功；3/3（含 fixture） | `out/ArchitectureReview/NinjaBuild.log` |
| VS Debug 完整构建与 CTest | 30/30；57.02 秒 | `out/ArchitectureReview/FixesVsDebug.log` |
| VS Release 完整构建与 CTest | 30/30；53.56 秒 | `out/ArchitectureReview/FixesVsRelease.log` |
| RenderDoc OFF Debug 构建与 CTest | 25/25；36.12 秒；未编译插件提示正确 | `out/ArchitectureReview/FixesDisabledBuild.log`、`FixesDisabledTest.log` |
| 格式、文件名/include 大小写 | 106 个源文件通过 | `tools/CheckStyle.py` |
| 语义命名/局部声明 | 60 个 C++ 编译单元通过 | `out/ArchitectureReview/FixesNaming.log` |
| 模块边界 | 102 个 C++ 源/头文件、23 个模块通过 | `tools/CheckBoundaries.py` |
| OpenSpec strict | 21/21 | `openspec validate --all --strict` |
| Diff whitespace | 通过 | `git diff --check` |

RenderDoc ON 的两个配置均实际运行了 runtime 和抓帧验收，没有跳过。OFF 在独立的 `out/build/renderdoc-off` 中验证，常用 VS/Ninja 配置保留 ON。初轮边界检查发现拆分后的 Viewer 缺少 Tasks、Config 直接依赖，补齐后上述完整检查全部通过。

## 函数长度与改动规模

按完整定义的物理行统计，包含签名、注释、空行、内部 lambda。Viewer 的 `main` 从 399 行缩短为 19 行，应用主流程 `FViewerApplication::Run` 为 11 行，最长函数为 60 行。原 importer 的 289 行导入函数分解为独立阶段，`FGltfImport::Run` 为 15 行，最长函数为 47 行。新的编码原则已写入 `AGENTS.md` 和 `docs/CodingStyle.md`。

以下统计以 HEAD 为基线，包含新增文件；“改动行”指新增加删除，包含空行和注释，不等同于净新增逻辑。文档和 OpenSpec 文件不计入代码统计。

| 类别 | 新增 | 删除 | 改动行 | 净新增 |
| --- | ---: | ---: | ---: | ---: |
| 三项修复的生产代码 | 121 | 38 | 159 | 83 |
| 回归测试与测试 CMake 接入 | 274 | 3 | 277 | 271 |
| 三项修复合计 | 395 | 41 | 436 | 354 |
| Viewer/importer 拆分及对应 CMake | 1533 | 1126 | 2659 | 407 |

三项修复落在先前估计的 300–500 改动行范围内。拆分主要移动既有逻辑，因此新增/删除合计显著高于净新增。辅助统计保存在 `out/ArchitectureReview/ChangeSize.json` 与 `FunctionLengths.txt`。

## 延后范围

通用离屏资源图、材质绑定泛化、资产缓存预算/热重载、任务系统设计、反射统一、流水线缓存、多窗口事件路由及其他超长函数均留待对应模块后续迭代。没有更改 CLI 选项、序列化字段、插件 ID 或既有 CMake target 名称。
