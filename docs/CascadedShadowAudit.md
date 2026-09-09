# CSM 开发质量审计

审查基线：`c717ea86c89d825756e88035f4575d4a76b29d7a`。范围为本次 CSM 的未提交改动及直接调用链，重点覆盖多 view/pass 的数据独立性、复制、cache 失效和性能证据。独立 reviewer 使用不继承开发会话的上下文审查初始 99 文件快照；主 agent 对五项 P2 finding 分别复现或追踪验证，再实施范围内修复。审计阶段未进行 Git commit 或 OpenSpec 归档；之后按用户要求完成的规范归档见 [CSM 使用说明](CascadedShadows.md)。

| ID | 已确认问题与影响 | 修复与回归 |
| --- | --- | --- |
| CSM-01 | 隐藏深度预览后切换分辨率，旧 preview binding 保留一张旧 cascade，触发约每 2 ms 的资源退役轮询。实测 150 ms 空闲内新增 104 个任务；重新打开预览后降为 0 并释放 4 MiB。 | 预览记录 attachment 的弱 lifetime，RHI 资源回收先清理过期的纹理与 binding。覆盖显示→隐藏→1024/2048 双向切换及任务空闲。 |
| CSM-02 | 阴影 view 继承 session 主 depth 格式；主视图设为 D32S8 时，显式 D32 shadow attachment 在图编译中被拒绝。 | 有显式 attachment 的 view 使用自身 D32 格式。真实 D32S8 swapchain/session 加四张 D32 阴影，生产模型绘制和像素检查通过。 |
| CSM-03 | CSM 直接读原始 Scene 输入，Forward 则读注册的 light provider，可能投影与照明方向不一致。固定 provider 下仅改变原始输入，接收面像素变化约 0.169。 | 通过 session 冻结 provider 路径解析方向，仅允许 Global/Frame/Scene；局部依赖关闭全局阴影。覆盖固定覆盖值、原始输入缺失及 View/Pass/Material/Object/Draw 五种局部依赖。复审发现的 Pass 遗漏也经过先失败、再修复通过的回归。 |
| CSM-04 | CSV 每个 CPU 行取最近 GPU 结果，重复采样并遗漏其他已完成提交；原 800 行样本曾有 84 次重复、82 处跳号。原 GPU mean/P95 属于近似结果。 | 增加 opt-in、限量的完成提交收集，正常 shutdown fence 完成后按提交 ID 匹配测量区间，拒绝缺失、重复及溢出；UI 保持最近结果。GPU 回归覆盖逐提交顺序和容量溢出。 |
| CSM-05 | 阴影 Eye 沿用主相机 Eye；即使四级矩阵未变，亚 texel 相机运动仍造成材质 scope 更新和批次规划重建。40 模型复现每帧 80 次 shadow strategy 求值，instance packed bytes 最终仍为 0。 | 阴影 Eye 使用量化光相机原点，静止光源复用 basis；亚 texel 回归中阴影 strategy 求值降为 0，每级仍清理并绘制。真实 transform 和对象移除仍触发重新规划。 |

同时移除了 `RenderSceneClient → RenderScene → Snapshot` 中两次额外 view 复制，以及 `RenderSession` 从 const async 结果取出已准备 pass 时的一次整包复制。交接改为在既有 task 等待完成后移动所有权；未改变任务调度和提交顺序。

独立审查确认的关键边界：

- View identity、pass usage、目标格式参与相关求值、batch 与 GPU cache key。四级阴影视图分别准备目标、矩阵和参数，主视图保留自身相机、材质、深度状态。
- 场景空间索引每个 family 维护一次，各 view 独立查询。各 view 的 CPU 材质求值完成后才进入 RHI 阶段；冻结 frame 和解析值为不可变共享对象。
- 模型几何、材质纹理及 GPU 资源通过共享 handle 引用；每 view 增加的主要是 draw packet、参数、批次控制数据。`RenderGraph::Compile() const` 仍按契约复制命令计划。
- 资源值 identity 与数值 scope 分离；数值更新不重建未变化的 descriptor table，资源变化仍更新完整身份。自定义 batch strategy 可读取候选数据，因此本轮保留其严格的重新规划条件，未通过放宽完整值检查来复用可能失效的分组。
- 阴影纹理、clear-only pass、sampled depth 和 barrier 的引用均跟随已提交 work/fence。预览和分辨率切换的特殊路径现有独立回归。

最终代码复审已关闭 CSM-01..05 及 CSM-03-R1。独立 reviewer 自行运行最终 Debug GPU 回归通过，前后 100 个代码/测试/工具/OpenSpec 文件哈希均与 `FinalCodeSnapshot.json` 匹配（SHA256 `9867307f26ff747d42cea1234d711487dcbae14c6e5a2c9cf363619a01c13943`）。该代码快照排除本报告和性能说明两份文档，文档在测量完成后单独核对。

实际验证：

- 修复集通过 Debug 全量 48/48（含 RenderDoc，122.94 s）及 Release 全量 43/43（55.35 s）。随后补齐 Pass 白名单，最终 Debug/Release 全量构建通过，相关 material、graph、batch、scene、CSM 回归各 8/8 通过。
- 所有 286 个自有源码格式/路径、177 个翻译单元语义命名及 277 源码/25 模块依赖边界检查通过。最终 GPU 回归由主 agent 和独立 reviewer 分别执行；新增 Pass 用例有修复前失败日志。
- 复制优化保留 task 等待及 immutable/fence lifetime；真实对象变换、移除、资源改变仍执行原有失效检查。批次规划的通用完整值判断未被放宽。

最终 Release/RTX 5080、debug layer 开启、4×2048 的完整提交流复测：运动 CPU 增量约 1.9–2.3 ms，仍未达到最初 1 ms 的工程目标；18,000 帧 shadow GPU mean/P95 为 0.050/0.055 ms。每个 GPU 提交恰好记录一次，PSO 固定 4、描述符固定 136；GPU allocation 峰值相对预热后增加约 4.38 MiB，并观察到多次回收。少量约 4–5 ms GPU 尖峰保留在统计中，来源未定位，不能只用平均值代表最坏帧。

修复后的完整提交计时与 CPU 开销见 [CSM 使用与性能说明](CascadedShadows.md)。原始复现、独立审查与复审、快照、构建/测试日志及测量保存在 `out/audit-csm`。本轮不扩展到通用 batch planner 或材质缓存的大规模重构；未重新做任意场景比例下的视觉校准，也不将有限测试等同于所有 temporal/artifact 情况均无问题。
