# Deferred 开发质量审计与修复

日期：2026-09-11。范围为 `add-deferred-render-pipeline` 的全部未提交实现及直接调用链，基线 `2a1ad620759a8eb23555b49f2c84af4b7cdd0676`。本轮按 quality-audit 流程使用不继承实现会话的独立 reviewer；主 agent 独立复核每项发现、实施范围内修复，原 reviewer 再复审。

**确认 4 项 P2，全部修复并经独立复验关闭。初审未报告 P0/P1；针对性复审未发现新的确认缺陷。没有未关闭的确认问题。**

## 审查身份与范围

- 初审包含 staged、unstaged、新增文件共 115 项；[InitialSnapshot.json](../out/DeferredAudit/InitialSnapshot.json)，SHA-256 `a14aff5f37cfa1b0fc7cbf1d95abf2d6de13753d341a5941fe4fef3bf0555dc7`。
- 修复候选共 116 项；[RepairSnapshot.json](../out/DeferredAudit/RepairSnapshot.json)，SHA-256 `6b357e9a63382dde37b95a44e88a9fb0b104afe877b9383f3916e25d34e53a8f`。相对初审仅改 11 项：4 个运行时代码文件、1 个 shader、测试 CMake、3 个测试文件、2 个说明文档。
- reviewer 静态复审前和独立实验后均校验 116 项，哈希差异 0。此后只补充本报告与性能说明，没有改源码；最终文件身份见 [FinalSnapshot.json](../out/DeferredAudit/FinalSnapshot.json)。
- 覆盖 BasePass→raster Lighting→HDR 输出、材质/透明/Unlit 路由、GBuffer/shared shader、CSM、fullscreen 缓存、MRT/Graph/D3D12 录制及资源退休、Viewer/config、性能工具与验收文档。
- 原始报告：[初审](../out/DeferredAudit/InitialReview.md)、[针对性复审](../out/DeferredAudit/RepairReview.md)。生成日志与探针保存在本机 out 中。

## 发现、复核与修复

| ID | 已确认触发与影响 | 修复 | 验证 |
|---|---|---|---|
| DAR-001 / P2 | Viewport 深度范围设为 .2..8 时，Lighting 直接把 window depth 当 NDC depth；世界位置错误，原生探针阴影消失 | 冻结 MinDepth 与范围倒数；主像素/邻居重建共同先还原 NDC；Deferred 在资源/图变更前拒绝不可逆范围 | 独立探针修复前非默认范围 MaximumShadow=0；修复后与默认范围均为 .290196，Forward/Deferred 图像 mean/max=0。新增偏移子视口及零范围拒绝用例 |
| DAR-002 / P2 | MRT slot1+ 使用 bSrgb 时，附件为 sRGB 而 PSO/retained target 签名仍为线性；错误 PSO 被接受，触发 D3D12 验证错误 | 后续槽位使用附件有效格式，保留 slot0 兼容表达 | 独立探针附件/签名均为 sRGB，录制错误 0→0；回归实际提交正确 PSO、复用 SharedDraws，并拒绝错误 PSO/变更附件后沿用旧 PSO |
| DAR-003 / P2 | 全屏参数逐项更新，中途未知参数/错误类型抛异常后，实例保留部分新值而缓存记录仍为旧值，污染下一次合法绘制 | 参数更新异常时移除该缓存 entry；后续从声明默认值重建，正常帧继续复用 | 独立探针失败恢复后的 green 从 .74902 恢复为 0；回归覆盖立即/延迟准备、未知名/错误类型及正常省略参数，恢复后的整幅图像与基准相同 |
| DAR-004 / P2 | 自定义全屏材质的 BlendConstants/StencilReference 未写入 draw，使用默认动态状态 | 使用现有转换及 RHI 动态状态校验接口 | 独立探针 blend=.25 时 red 由错误的 1 恢复为 .25098；回归验证实际混合像素与 draw 中 stencil reference=37 |

主 agent 已分别通过源代码调用链及原始探针证据复核四项缺陷；没有将待验证疑点或偏好当作 finding。修复入口为 [SceneRenderPipeline.cpp](../Source/Runtime/Renderer/Private/SceneRenderPipeline.cpp)、[ScenePipelineFullscreen.cpp](../Source/Runtime/Renderer/Private/ScenePipelineFullscreen.cpp)、[Lighting.hlsl](../shaders/Deferred/Lighting.hlsl)、[RHITypes.h](../Source/Runtime/RHI/Public/Hyperion/RHI/RHITypes.h)、[FullscreenPass.cpp](../Source/Runtime/Renderer/Private/FullscreenPass.cpp)。

## 实际验证

- 修复后 Debug **54/54**：[FullDebug.log](../out/DeferredAudit/FullDebug.log)。
- 修复后 Release **54/54**：[FullRelease.log](../out/DeferredAudit/FullRelease.log)。两套均包含新增 deferred_rendering 回归、材质/RHI/Graph/CSM、Viewer 和 RenderDoc 捕获及 GPU 回放。
- 独立 reviewer 将初审四个原始探针重新编译链接至修复候选，在 RTX 5080 / D3D12 debug layer enabled 下串行运行，均退出 0：[深度范围](../out/DeferredAudit/DepthRangeRepair.log)、[MRT sRGB](../out/DeferredAudit/MrtSrgbRepair.log)、[异常恢复](../out/DeferredAudit/FullscreenRecoveryRepair.log)、[动态状态](../out/DeferredAudit/FullscreenDynamicRepair.log)。
- [格式/路径检查](../out/DeferredAudit/StyleFinal.log)通过，356 个 owned 源文件；本轮 6 个修改/新增 C++ 翻译单元[语义命名](../out/DeferredAudit/Naming.log)通过；[边界检查](../out/DeferredAudit/Boundaries.log)覆盖 338 个源文件、25 个模块；[OpenSpec strict](../out/DeferredAudit/OpenSpec.log) 和 `git diff --check` 通过。
- Release 修复后性能补测：Scene 1080p、静止/移动、CSM 开关、三种管线/布局、两轮，共 24 次运行和 12,000 个采样帧。工作量/提交对应、资源稳定性断言通过，验证层错误 0；数据与测量条件见 [DeferredPerformance.md](DeferredPerformance.md) 的“审计修复后补测”。原有 96 次矩阵保留为审计前数据。

## 验证边界

本轮 GPU 运行覆盖本机 RTX 5080 / D3D12；DXIL/SPIR-V/MSL 有着色器合同测试，未在 Vulkan/Metal 设备执行。Stencil reference 的新增回归检查 draw 包传递，未新增实际 stencil 像素用例。没有进行真实 OOM/device loss、其他 GPU 硬件或无限时长资源压力验证。Legacy Forward 的 display overlay 不共享 scene-depth 是原有明确文档化边界；本轮没有扩展此契约。

审计完成后，按用户要求将变更归档至 [2026-09-11-add-deferred-render-pipeline](../openspec/changes/archive/2026-09-11-add-deferred-render-pipeline/tasks.md)，同步六份主规范后提交。归档阶段只调整规范和文档，源码保持已复审及测试的版本。
