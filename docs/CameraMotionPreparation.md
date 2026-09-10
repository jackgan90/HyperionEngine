# 镜头运动时的 CPU 准备开销

## 已确认的原因

基线为 `62bac8f`。此前优化已经让物体常量、实例数据、PSO 和描述符得到较好复用，但整个 prepared view 仍是一个整体缓存。镜头矩阵变化后，这个缓存失效，Render 和 RHI 各层重新遍历数百个 source items，分别确认同一批局部状态未变。四个阴影视图也独立经历这条路径。

这解释了静止与运动的突变：静止时可跳过整条准备链；运动时，虽然实际只有约 30 个场景/阴影 draw，CPU 仍按约 838 个可见 source items 工作。基线小幅运动的共享材质刷新平均进入约 713 次/帧，通常只需更新几个共享常量块。GPU 阴影与 Forward 合计约 0.12 ms，原生 draw 记录约 0.3 ms，主要成本在 CPU 准备阶段。

调用链为 `ForwardRenderPipeline → RenderSession::BuildViews → PrepareView → Collect / PrepareSceneSnapshot / PrepareMaterials / Batches.Build`；延迟图编译在 RHI 域调用 `BuildDraws`（重构前为 `BuildPasses`），再次做源项准入和回执发布。`TaskWait` 与嵌套 scope 的时间不能作为独立 CPU 成本累加。

## 实现

- **共享材质契约**：普通求值确认语义依赖和 instance layout 后，发布 `FMaterialSharedBinding`。稳定的 Scene/resource publication 允许保留 item 直接使用这个契约，每个视图按契约分组刷新共享值。契约不持有 Object scope，只弱引用需要比较的资源值。快速路径和普通路径共用刷新表，避免不同共享表身份拆散原本可合批的 items。注册自定义合批策略时保留原有精确参数复用路径。
- **跨阶段的局部准备证明**：`FLocalMaterialPreparation` 记录有序 primitive、ordinal、LocalItemId、准备对象和局部参数身份。只有静态发布、完整依赖、view/pass/target 环境及内置策略满足约束时，才能跳过重复的 batch membership 和 RHI 准入检查。共享 overlay 在批内仍必须一致，实例 chunk 仍必须存活。
- **可见集合变化后的单项复用**：item 另外持有不延长 source/value 生命周期的 `FLocalMaterialItem`。可见集合改变时重新分组，但未变 item 的输入、candidate 结构和已打包 record 可继续复用。record 最多记录 8 个弱视图证明；超过数量按完整值比较。输入、candidate、record 和 block 的无序缓存使用完整键相等判定，LRU 和容量约束保持显式，hash 仅选择 bucket。
- **不可变包更新**：保留 batch 的结构身份和实际 draw 到源项的映射。共享更新只刷新实际 draw 的常量绑定，重新发布不可变 packet 容器；较早图和 GPU 数据继续由原有 owner/fence 保留。失败仍回退到逐项验证、整组失败传播和修复路径。每个视图仍发布当前 frame/family/view 的回执。
- **可见性往返保留**：每个视图最多保留 2048 个近期不可见 item；Scene/resource publication 改变时失效。稠密 primitive slot 使用有界索引，稀疏编号或大 ordinal 使用排序查找。不能缓存的自定义 collection 不保留重复旧副本。`Reused` 使用字节标志，避免高频 `vector<bool>` 操作；对象仍有独立稳定地址，独立快照仍深拷贝 item 状态。

局部证明上限为 4096 items/视图，共享契约和更新表上限为 128；超出容量继续走通用路径。没有关闭 Debug `/Od /RTC1`、STL 调试检查、D3D12 validation、阴影更新或 GUI。剔除算法、shader ABI 和阴影质量均沿用现有实现。

共享契约另有最多 128 项的弱历史，用完整 program/pass、参数索引、依赖和资源值比较合并不同帧建立的等价契约。默认 provider 的输出共享已验证的单个语义副本，避免重复复制/验证；它不引用原始 scope 的整张表，因此不会保留未使用的纹理或 buffer。自定义 provider 仍执行自己的回调和结果验证。provider 清理直接遍历现有 recency 索引，只有实际回收才查找 bucket。

相关实现：[材质契约](../Source/Runtime/Renderer/Private/MaterialSharedBinding.cpp)、[视图准备](../Source/Runtime/Renderer/Private/SessionViewCache.cpp)、[局部证明](../Source/Runtime/Renderer/Private/LocalMaterialPreparation.cpp)、[合批复用](../Source/Runtime/Renderer/Private/BatchPlanCache.cpp)、[RHI packet 更新](../Source/Runtime/Renderer/Private/ScenePassRefresh.cpp)、[可见项保留](../Source/Runtime/Renderer/Private/SceneCollectionReuse.cpp)。

## 测量方法与中间结果

使用默认 `experiments/Scene.json`，1440×900、79 个模型全部 ready，关闭 VSync。常规计时关闭 Tracy；另建 Debug Tracy 版本做归因。GPU 工作串行运行。初期各阶段是独立诊断运行，最终结论以交错 A/B 为准。

| Debug GUI 条件/阶段 | 均值 ms | P95 ms |
| --- | ---: | ---: |
| 基线静止 | 6.348 | 8.064 |
| 基线小幅运动 | 20.613 | 26.104 |
| 第一阶段：共享材质快速路径 | 23.296 | — |
| 第二阶段：统一共享身份、保留 item | 17.843 | 21.560 |
| 第三阶段：局部证明与 packet 复用 | 16.962 | 19.973 |
| 第四阶段：主视图契约的生命周期修正 | 15.385 | 18.332 |
| 第五阶段：字节复用标志与 provider 检查 | 13.567 | 16.597 |

第一阶段出现真实退化：快速/普通路径发布不同共享表身份，导致额外合批检查。第四阶段修复的是另一处复用缺口：契约不应依赖加载期间临时准备对象继续存活。两次退化/缺口都有原始日志，未把中间结果替换成最终结果。

第五阶段 Debug detail trace 中，`PrepareMaterials` 为 1.425 ms/帧，`PlanBatches` 为 0.727 ms/帧，`PrepareDraws` 为 2.304 ms/帧；`AppendRetainedSceneItems` 从 1.434 降至 0.220 ms/帧。共享契约刷新约 14.3 组/帧，约 4.225 个视图/帧使用局部 packet 复用。此处是嵌套 scope 的 inclusive 时间；这些数值不能全部相加，也不能与关闭 profiling 的数据混为一组。

大幅运动（orbit step 10）还会每帧重新分组约 3 个视图。第五阶段该条件的 trace 中，planning 为 5.088 ms，其中实例数据 1.696 ms、输入检查 1.158 ms、candidate 检查 0.989 ms。单项证明加入后，后续 trace 的实例数据降到 0.650 ms，输入检查为 0.732 ms；契约分组从约 44.8 降到 14.9 次/帧。

| Debug GUI 大幅运动的后续单次诊断 | 均值 ms | P95 ms |
| --- | ---: | ---: |
| 第五阶段（两轮 A/B 中的候选中位数） | 20.769 | 23.809 |
| 第六阶段：单项证明 | 19.854 | 22.188 |
| 第七阶段：契约弱历史、清理索引 | 19.178 | 21.982 |
| 第八阶段：相邻 world 的视锥复用尝试 | 19.349 | 22.602 |
| 第九阶段：完整键 hash 索引、撤销视锥尝试 | 18.376 | 20.734 |
| 第十阶段：默认 provider 单项值共享 | 18.576 | 21.621 |

第八阶段的 trace 显示 833.7 次 cull 全部重新构建视锥，没有实际复用，因此已撤销该尝试。第十阶段的单次总帧时没有体现额外收益，不把消除重复复制等同于已经证明的 FPS 提升。最终判断仍使用冻结程序的交错 A/B。

原始证据目录为 `out/camera-motion-20260909`：包括 `baseline-*`、`stage1-*` 至 `stage5-*`、`trace-scoped-ui`、`trace-stage3-ui` 和 `trace-stage5-ui`。普通与 profiling 的程序身份分别保存。

## 交付验证

新增回归覆盖共享分组次数、静态 collection 的混合 Object/View 与 Draw 依赖回退、稠密/稀疏 slot、大 ordinal、保留上限、快照隔离、可见性往返、实际 instancing 与普通 draw 的像素一致性、材质覆盖使局部证明失效。原有测试继续检查资源变化、missing/default、旧帧常量、透明排序、自定义策略、移除和设备生命周期。

初次交付源代码在计时前冻结，44 个源文件的 SHA-256 记录于 `out/camera-motion-20260909/DeliveryManifest.json`。两种构建均完成全量构建；Debug CTest **48/48**、Release **43/43** 通过。Debug 包含 20 项 GPU/desktop 测试及 RenderDoc、离线启动和镜头移动验收。310 个 owned source 的样式检查、194 个翻译单元的语义命名检查、301 个 source / 25 个 module 的边界检查、`git diff --check` 和 37 项 OpenSpec strict 校验均通过。下述交付测量与长时运行均对应这一冻结版本，早于后续审计修复；审计修复后的验证单独记录。

### 最终交错 A/B

GUI 开启、隐藏窗口渲染、VSync 关闭、Tracy 关闭。每组 200 帧预热、400 帧采样，三轮交替 baseline/candidate 顺序。下表各列分别取三轮对应统计量的中位数，FPS 为 `1000 / 均值帧时`，不是逐帧 FPS 的算术平均。小幅和大幅运动分别使用已有输入驱动轨迹的 `--camera-step 0.1` 与 `10`；剔除和四级阴影保持启用。

| 条件 | 基线均值 → 新均值 ms | 基线 P95 → 新 P95 ms | 基线 P99 → 新 P99 ms | 均值帧时变化 |
| --- | ---: | ---: | ---: | ---: |
| Debug 静止 | 5.410 → 5.238 | 6.584 → 6.072 | 7.850 → 7.234 | −3.2% |
| Debug 小幅移动 | 19.266 → 13.289 | 22.765 → 15.897 | 24.717 → 17.398 | −31.0% |
| Debug 大幅移动 | 26.132 → 18.228 | 29.281 → 20.851 | 32.773 → 23.010 | −30.2% |
| Release 静止 | 1.182 → 1.182 | 1.510 → 1.600 | 2.057 → 2.171 | +0.06% |
| Release 小幅移动 | 2.045 → 1.833 | 2.559 → 2.214 | 2.812 → 2.436 | −10.4% |
| Release 大幅移动 | 2.581 → 2.140 | 3.136 → 2.631 | 3.578 → 3.033 | −17.1% |

每种运动/构建各 1200 个 measured frames 的主视图 draw、visible/instanced/single items、阴影 items/draws 和失败计数均逐帧一致；CPU 帧与实际 GPU submission identity 完整对应。全部模型 ready、失败项为零、原生 validation errors 为零，预热后的 PSO/descriptor 数稳定。原始 CSV、命令、程序哈希和比较结果位于 `delivery-ab-debug-ui`、`delivery-ab-debug-large-ui`、`delivery-ab-release-ui`、`delivery-ab-release-large-ui`；汇总见 `DeliveryResults.json`。

### 剩余差距

Debug 小幅移动约 **51.9 → 75.3 FPS**，三轮合计超过 16.67 ms 的采样比例从 **85.9% 降到 2.5%**；P95 达到 60 FPS 帧时预算，但 P99 仍超出。大幅移动约 **38.3 → 54.9 FPS**，仍有 **89.9%** 的样本超过预算，未达到目标。静止与小幅移动的均值差从 **13.86 降到 8.05 ms**，大幅移动的差从 **20.72 降到 12.99 ms**。因此本次改善了准备链的复用粒度，但没有实现用户期望的接近静止帧率的小幅抖动。

最终 Debug detail trace 的大幅运动中，planning 为 2.740 ms/帧（第五阶段为 5.088），其中 instance data 为 0.475、输入检查为 0.436、candidate 检查为 0.585 ms。剩余 snapshot preparation 为 1.725 ms，collection 为 1.489 ms，RHI pass preparation 为 2.961 ms；约 2.935 个视图/帧仍需重新分组。共享材质准备为 1.834 ms、刷新约 14.925 组/帧。父子 scope 重叠，不能将这些项全部相加。GUI 构建另约 1.857 ms，native draw 记录约 0.328 ms。

这说明后续要继续缩小差距，需要处理变化可见集合的分组/发布和逐项剔除成本；单独压缩 draw 记录或常量打包已不是主要方向。Release 静止均值基本不变，但本轮 P95/P99 分别增加约 0.091/0.114 ms，不能声称每个分位数都改善。

### 可复现身份和命令

证据根目录为 `out/camera-motion-20260909`，最终程序是 `delivery-debug` / `delivery-release`。旧 `candidate-*` 目录是第五阶段程序，不用于最终结论。

| 程序 | SHA-256 |
| --- | --- |
| Debug 基线 | `D475887BCE242EA61C1D9E89C0CBA3602C4658C068E13F5FC6B3950815461D68` |
| Debug 最终 | `8C7ACD64C01F8F9BAD4E9420B2764909DA987E0465D5AAAC573C50D3C3F82813` |
| Release 基线 | `A037852FF12CF38F08912907DD69C6F363C5ABE67A14A74B2565151B090CC238` |
| Release 最终 | `88083F7C6484BBB948F6F164ECA89873E3FD6AB71F8DA25B4F17DA8BBE2AE748` |

Debug 基线来自 `62bac8f` 的原始构建；Release 基线只增加了编译关闭的诊断 scope，未包含优化。四个冻结目录均保存 runtime；只有 `delivery-debug` 和 `delivery-release` 保存了对应的 `CMakeCache.txt`，两个 `baseline-*` 目录没有历史配置快照，因此不能从冻结目录独立回溯完整基线配置。常规 Debug 保持 `/Ob0 /Od /RTC1`、Tracy OFF 和原生验证。`DeliveryDebugBuild.log`、`DeliveryDebugTests.log`、`DeliveryReleaseBuild.log`、`DeliveryReleaseTests.log`、`DeliveryNaming.log`、`DeliveryOpenSpec.log` 保存交付检查证据。

```powershell
python tools/CpuSubmissionBenchmark.py --viewer out/camera-motion-20260909/delivery-debug/hyperion_viewer.exe --baseline out/camera-motion-20260909/baseline-debug/hyperion_viewer.exe --output out/camera-motion-20260909/repeat-debug-ui --counts --scene --scene-shadows on --motion both --warmup 200 --samples 400 --trials 3 --ui
```

大幅移动替换为 `--motion moving --camera-step 10`；Release 替换对应 runtime 路径。输出目录必须是新目录。归因使用单独的 Tracy Debug runtime，结果在 `trace-delivery-small-ui` / `trace-delivery-large-ui`，不混入上述常规计时。

### 画面与持续运行

关闭 GUI 后，Debug/Release 各运行静止、小幅移动、大幅移动两轮，共 **12 组 A/B、24 次运行**。每组采样 400 帧，源项覆盖、实例 draw 数和主视图/阴影计数逐帧一致，CPU/GPU submission 完整对应；第 600 帧结束时，同一构建、同一条件的 A/B PNG 哈希及两轮重复结果均相等。Debug 与 Release 的哈希分别如下：

| 构建 / 画面 | PNG SHA-256 |
| --- | --- |
| Debug 静止 | `5c0f52df08599fb617abd78942e5df633cc56fc86cc3ef7d7cc2f20e2e7afec9` |
| Debug 小幅移动 | `9fe7e96e1e7acf9feda409f29e62d1ff03663f759cb2c4caa4ab4f185d126d11` |
| Debug 大幅移动 | `3ab1a650d666acb32f51170574ec1e4e15973f24c373bb22a3592c49ab7c5235` |
| Release 静止 | `169584380e35518bb934c864105eb544664cd1fb699210bb12dad5199578a602` |
| Release 小幅移动 | `f6fa9d2f104cb1d60ac969bd6ceb41bdf348ee36bdebe1b44ab91a271d0817ff` |
| Release 大幅移动 | `c9ac93d8151bab5bf4f26b240ef5c8aee9d5abba0858dc819c4429c928b89d10` |

跨构建比较并不严格相等：静止、小幅、大幅分别有 2、1、2 个像素不同，最大单通道差分别为 45、42、39。这些差异也存在于对应基线，原因尚未确定，不能归因于本次优化。审计重算结果为 `out/camera-motion-audit-20260910/CrossBuildImages.json`。

原始运行在 `delivery-ab-debug-images` / `delivery-ab-release-images`，补充的 GPU identity 和完整计数检查记录于 `DeliveryImageValidation.json`。第五阶段的 `ab-debug-images` 曾有一组严格 PNG 比较失败：两次基线大幅移动截图自身相差 2 个像素，两个候选截图均与第一次基线相等。原因尚未确定，原始失败证据保留；最终冻结版本的上述 12 组比较均严格通过。

最终 Debug 另运行 200 帧预热、**6000 个 GUI 开启的大幅移动采样帧**，覆盖 150 个往返轨迹周期。均值 **18.269 ms**、P95 **20.696 ms**、P99 **22.856 ms**；六个连续 1000 帧窗口的均值为 18.164、18.213、18.420、18.226、18.363、18.229 ms。初始窗口填充后，后续 5000 帧的 batch 输入缓存稳定在 **947 项 / 1,931,872 bytes**，保留不可见 items 在 83–123 间变化，GPU 分配在 75,862,016–75,927,552 bytes 间变化。PSO 5 个、descriptor allocation 140 个，整个采样区间保持不变。这些已记录缓存指标没有持续增长，不代表覆盖了所有进程内存。

持续运行的 6200 个 GPU frames 全部完成，validation errors 为零，场景 79/79 ready，CPU/GPU 采样一一对应。证据为 `delivery-sustained-debug-large-ui` 和 `DeliveryLongRunValidation.json`。

独立运行带 GUI 的 600 帧小幅移动，并使用 `--capture --verify-ui --verify-model` 验证 GPU 回读，随后目视检查 `DeliveryGui.png`：场景、模型、阴影、三个调试面板均可见；阴影面板显示四级、每帧更新、2048 px，主面板显示原生验证启用、Errors 0、Tracy 未编译。此验证使用隐藏窗口和输入驱动轨迹，没有进行手动桌面交互。截图运行包含 readback 开销，不计入正式 A/B 帧时表。

```powershell
python tools/BatchPlannerComparison.py --kind viewer --baseline out/camera-motion-20260909/baseline-debug/hyperion_viewer.exe --candidate out/camera-motion-20260909/delivery-debug/hyperion_viewer.exe --output out/camera-motion-20260909/repeat-debug-images --warmup 200 --samples 400 --trials 2
python tools/CpuSubmissionBenchmark.py --viewer out/camera-motion-20260909/delivery-debug/hyperion_viewer.exe --output out/camera-motion-20260909/repeat-sustained-debug --counts --scene --scene-shadows on --motion moving --camera-step 10 --warmup 200 --samples 6000 --trials 1 --ui --timeout 240
```

## 2026-09-10 质量审计与修复验证

独立初审以 `62bac8fbb7890bc4ce8e3a75d65e5811f46b69d1` 为基线，覆盖 `InitialSnapshot.json` 的 50 个未提交文件，确认了公开收集接口缺少前一快照发布校验、跨 Debug/Release 图片相等的误述，以及基线配置快照范围的误述。审计证据目录为 `out/camera-motion-audit-20260910`，初审和复审分别见 `IndependentReview.md` 与 `ResumedReview.md`。

公开 `Collect` 现在先比较场景身份、场景 revision 和资源 publication revision，再消费前一快照。不匹配时创建新收集结果及独立回执帧，旧快照仍由原 owner 保持；新增直接 API 回归覆盖移除、更新后不可见、资源 revision 改变和跨场景快照。原移除探针的新快照保留项均为零，旧快照释放后原 item Lifetime 释放。同发布内的稠密/稀疏查找、大 ordinal、保留上限和可见性往返测试仍通过。图片与配置说明已按原始证据更正。

验证时另发现两项原有测试的时序前提缺口。`ResourceTests` 只等待几何 ready，隔离探针复现几何 Ready、材质 Preparing、零个 pass 和未就绪失败回执；现等待全部 binding（包括材质）ready 后再检查 41 个 draw。`SceneRenderTests` 精确断言只上传变换数据，却没有保证旧 GPU 缓存 owner 存活；在准备和执行之间加入 30ms 调度间隔，复现预期 288 bytes、实际 480 bytes。显式保留旧 snapshot 至断言后，同间隔恢复为 288 bytes；修正仅固定测试前提，未修改引擎退休规则或放宽断言。修正后的资源探针连续 100 次通过，两种构建的两项测试各重复 5 次均通过。重启前失败日志仍保留，原失败现场没有状态采样；这些隔离复现确认了相同断言的失败条件，不把 Windows 更新或重启认作根因。

本轮 Debug/Release 均完成全量构建及 9 项相关回归。测试前提修正后分别重建对应的两个测试目标，并完成上述重复验证。310 个 source 的命名/include/格式、194 个翻译单元的语义命名、301 个 source / 25 个 module 边界和 37 项 OpenSpec strict 检查通过；随后修改的两个测试又用原检查函数及完整 Debug 编译数据库单独验证语义命名。初次交付的 48/43 项完整测试及 6000 帧长时运行没有在本轮重新执行。

### 审计修复前后的独立 A/B

重启后串行比较原 `delivery-debug/release` 与新 `fixed-debug/release`，每种构建的静止、小幅、大幅各两轮交替运行：GUI 开启、隐藏窗口、VSync/Tracy 关闭、200 帧预热、400 帧采样、79/79 模型 ready、四级 CSM。各列取两轮统计量的中位数。这组数据仅比较审计修复前后，不与重启前初次交付的三轮数据混合。

| 条件 | 修复前 → 修复后均值 ms | 修复前 → 修复后 P95 ms | 均值变化 |
| --- | ---: | ---: | ---: |
| Debug 静止 | 4.989 → 4.970 | 5.586 → 5.625 | −0.38% |
| Debug 小幅 | 11.660 → 11.655 | 13.343 → 13.391 | −0.04% |
| Debug 大幅 | 16.085 → 15.996 | 17.944 → 17.800 | −0.55% |
| Release 静止 | 1.186 → 1.211 | 1.513 → 1.555 | +2.11% |
| Release 小幅 | 1.861 → 1.897 | 2.342 → 2.366 | +1.92% |
| Release 大幅 | 2.150 → 2.137 | 2.731 → 2.664 | −0.64% |

不能据两轮结果宣称所有条件均改善或证明长期无退化；Release 静止/小幅有小幅增加，Debug 静止/小幅 P95 也略增。Debug 大幅 P95 仍高于 16.67 ms，原性能目标尚未全部达成。

关闭 GUI 后另比较两种构建的三个条件各一轮，共 6 组 A/B、12 次截图运行，PNG 全部严格相等。24 次 GUI 运行及 12 次截图运行的主视图/阴影源项与 draw 计数逐帧一致，失败项为零，CPU/GPU submission identity 完整对应，原生 validation errors 为零。`FixEvidenceValidation.json` 保存重算结果；独立 reviewer 另重算全部 36 个 CSV、6 组图片及程序身份，结果为 `ResumedPostFixEvidenceValidation.json`。本轮没有重新进行手动桌面交互或 6000 帧持续运行。

`FixRuntimeManifest.json` 保存新冻结 runtime/DLL/CMakeCache 的完整哈希。Debug Viewer SHA-256 为 `132C1C4E3F011CFBA7733881BA53E6D04A9C58037F8F4C9F87AB2BA4332156D5`，Release 为 `1536FDD1135C6688FA78EE3122287F36C99A211C8CFF65C0BFC51F058A516C98`。之后的两处测试前提修正不属于 Viewer 的链接输入，未改变这两个已测程序。

OpenSpec change 已归档至 `openspec/changes/archive/2026-09-10-optimize-camera-motion-preparation/`，4 项要求已同步到 `openspec/specs/camera-motion-preparation/spec.md`；上述审计证据和性能限制继续保留。
