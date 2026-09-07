# 验证记录

日期：2026-09-07。用户已审核 plan 并授权完成全部任务；本记录对应实现后的工作区，不包含 Git 提交或 OpenSpec 归档。

## 环境与最终结果

Windows x64，NVIDIA GeForce RTX 5080，D3D12 debug layer 启用。Visual Studio 2022 Community / MSVC 19.38；Ninja 使用本机 Build Tools / MSVC 19.50。两个构建目录均为 RenderDoc ON、Tracy OFF。实际安装的 RenderDoc 可加载，捕获、XML 内容检查与 GPU replay 均执行成功，最终完整套件无跳过项。

| 验证 | 实际结果 | 日志 |
| --- | --- | --- |
| VS Debug 最终构建及完整 CTest | 35/35，110.41 s | `out/RenderPrimitivesVsDebugFinal.log` |
| VS Release 最终构建及完整 CTest | 35/35，93.33 s | `out/RenderPrimitivesVsReleaseFinal.log` |
| Ninja Debug 完整迁移回归 | 35/35，84.26 s | `out/RenderPrimitivesNinjaTests.log` |
| Ninja 最终源码全量构建 | 成功 | `out/RenderPrimitivesNinjaBuild.log` |
| Ninja 收尾调整后的相关回归 | 7/7，21.57 s（含 fixture） | `out/RenderPrimitivesNinjaFinalTests.log` |
| 语义命名、局部声明、双向 bool 前缀检查 | 74 个编译单元通过 | `out/RenderPrimitivesNaming.log` |
| 路径、include 大小写、源码格式 | 127 个源文件通过 | `out/RenderPrimitivesStyle.log` |
| 模块边界 | 123 个被检查源文件、23 个模块通过 | `out/RenderPrimitivesBoundaries.log` |
| OpenSpec 本 change strict | 通过 | `out/RenderPrimitivesOpenSpec.log` |
| OpenSpec 全部 strict | 21/21 | `out/RenderPrimitivesOpenSpecAll.log` |
| tracked/untracked 文件空白检查 | 通过 | `out/RenderPrimitivesDiffCheck.log` |

Ninja 最终相关回归包含 `render_primitives`、`render_resources`、`scene_rendering`、`model_rendering`、`d3d12_frame_failure_recovery`、`model_viewer_acceptance` 及 fixture。最终 VS 两种配置都在最后一次 ModelViewer Worker 调整后重建并执行完整套件；没有沿用旧套件数量。

执行入口：`tools/GenerateSolution.ps1 -Test -Configuration Debug/Release`、`tools/Build.ps1`、`tools/CheckStyle.py`、`tools/CheckStyle.py --naming --build-dir out/build/debug`、`tools/CheckBoundaries.py`。最终 VS 日志没有自有代码编译警告或错误。

## 需求验收

| 范围 | 可执行证据 |
| --- | --- |
| Render 独占 proxy、消息自有性 | `render_primitives` 暂停 Render 后修改 Main 输入，验证原快照；检测 wrong-domain 构造/收集；验证零/多 item 派生类及恰好一次 Render 析构 |
| 注册、版本和批量状态 | `CreateBatch` 一次注册完整模型；`render_primitives` 验证完整更新、非法 batch 无部分修改、旧 revision、generation 复用和跨 scene 句柄 |
| 无帧控制与关闭 | 独立 Render 控制流处理失败后的 Remove、重复 Remove、无任何 GPU 帧的关闭、绑定晚于 scene 释放；关闭后 binding 不再向 Tasks 派发 |
| 共享资源和实例隔离 | `render_resources` 验证并发请求合并、失败后重试合并、不同 version/layout/颜色配置/device 分离；`scene_rendering` 验证两个模型资源身份相同且几何只上传一次 |
| 异步就绪、取消和迟到结果 | 上传 gate 下取消一位消费者，另一位仍就绪；旧 preparation 晚于新版本完成而不覆盖当前状态；pending 生产者在 shutdown 被 join |
| 失败清理 | preparation 标准/非标准异常、pipeline 创建异常、GPU completion 查询异常均有终态；查询失败不提前放掉权威资源，单 Worker 可继续推进控制工作 |
| RHI 0 最后释放 | 替身 buffer/texture/pipeline 析构检查执行域；上传中移除、从未提交资源、GPU frame 引用存活及无后续帧的退休均被验证；普通退休不增加 WaitIdle 次数 |
| 真实 GPU fence 退休 | `d3d12_frame_failure_recovery` 使用真实 queue/fence gate：Remove 回执后 proxy 已销毁、buffer 仍存活；释放 gate 后无需下一帧，weak buffer 失效、资源计数归零 |
| 深度与透明 | `scene_rendering` 在先画近模型、再画远模型时验证像素；两个模型的透明 section 深度交错，检查全场景 back-to-front 合成的预期 sRGB 像素 |
| 剔除、多视图和聚合 | 替身场景验证隐藏/视锥外过滤、边界相交、镜像非均匀变换和多视图重复收集；真实场景绘制 40 个 primitive 仍只有聚合 scene pass；无 bounds 的真实程序化 primitive 保守保留 |
| 模型与应用迁移 | 旧 FModelRenderer 删除；Runtime 场景测试不链接 ModelViewer/Triangle；两插件经标准 session 注册；相机输入/取景状态在 Main，包围盒计算在 Worker，跨域仅传 owned 数据 |
| 原有材质与资产 | 保留 glTF/GLB/hasset、alpha/mask、深度、sRGB/UV1/mip、镜像/双面、相机操作、延迟 IO、加载失败与既有常量范围负例 |
| 原有应用与 backend | clear、triangle、GUI、resize/minimize/restore、截图、默认无网络监听、独立 device/payload ownership、Present 失败/重复取消/下一帧恢复全部通过 |
| RenderDoc | 真实 Triangle/Model 捕获、scene 与 GUI draw 提交标记、回放、缺失/不兼容 runtime、错误输出路径及未启用路径通过 |

## 收尾修正

- 首轮完整迁移测试指出 Viewer 缺少直接 Renderer CMake 依赖、RenderDoc 验收仍查找旧 `Triangle`/`Static model` marker；已补依赖并改为标准 `Scene 0` marker，保留“已提交 scene draw，不能用 GUI 替代”的检查。
- 资源故障测试中的 gate 改为 `Tasks.Wait` 可恢复等待，避免测试自身用 `future.wait()` 占住唯一 Worker；作用域释放保证测试失败时也能排空队列。
- 语义检查识别到 `atomic_bool` 别名的检测差异，测试改用仓库既有 `std::atomic<bool>` 拼写；新增测试类模板加入 `.clang-tidy` 的精确 T 前缀例外，没有放宽普通类型规则。
- 包围盒计算保留在 Worker，Main 只读取已发布结果；Worker 捕获请求副本和有明确寿命的 Tasks，不捕获 ModelViewer 可变 PImpl。
- 一次直接调用 MSBuild 的局部复编译遇到宿主 `PATH`/`Path` 重复环境键，按仓库脚本现有逻辑仅规范化子进程环境后通过；没有改动系统环境。

## 范围说明

共享几何仍使用普通 indexed draws。本阶段不实现 GPU instancing、instance buffer 分配器、动画/蒙皮、LOD 或离屏资源图。透明排序以中心深度为粒度。替换资源版本在 pending 期间可以暂时不绘制，完整就绪后才提交。通用资源请求者负责把所有影响表示的配置纳入 version/configuration；不同内容不能复用相同键。跨队列循环等待由协议禁止，未增加通用死锁检测器。

开发契约见 [RenderPrimitives.md](../../../../docs/RenderPrimitives.md)，实现选择见 [design.md](design.md)。

## 2026-09-07 独立质量审计补充

使用仓库 quality-audit 流程审计全部 64 项未提交文件，独立 reviewer 初审发现两项 P2，主审独立复核并完成局部修复：非法 section 的批量更新/延迟就绪诊断，以及直接 RHI 正常帧的完成提交回收。原 reviewer 复审均关闭，无新增确认缺陷。

修复后 Ninja Debug、VS Debug、VS Release 全部目标构建成功，各自相关 CTest **8/8**，耗时分别 24.23 / 33.86 / 30.59 s；74 编译单元命名、127 文件格式、123 文件/23 模块边界、OpenSpec strict 和空白检查通过。本轮是定向回归，没有重跑上述完整 35 项套件或 RenderDoc 捕获/回放。详细版本、复现和验证限制见 [独立审计记录](independent-review.md)，原始日志在 `out/RenderPrimitiveAudit/`。
