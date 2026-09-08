# Material 优化独立审计

2026-09-08。基线 `ef7a1ef7e6f3cdac006ae9c1248573867cc98c32`；审查未提交工作区，不归档、不提交。首轮冻结 49 文件：`out/MaterialOptimizationDelivery/Manifest.json`；修复复审冻结 51 文件：`out/MaterialOptimizationAudit/Fixed/Manifest.json`，均有逐文件副本和 SHA256。复审期间不修改候选文件，两个 reviewer 独立核验 51/51 相符。复审完成后仅补充本报告及相关 OpenSpec/文档证据，源码保持复审版本。

## 结论

优化以引擎 Materials/Renderer/RHI/D3D12 的通用路径为基础：不可变输入共享、有效依赖增量求值、任意反射常量块复用，以及单次 native list 内绑定状态复用。没有 Application、插件 ID、PBR 名称或固定块数特判。已有 Viewer 数据与独立非内置材质测试支持其常见场景收益。

首次独立 review 找到 5 项 finding，合并为 4 类问题。主 agent 分别读取实际创建/消费链并复现，全部确认后在原有边界内修复；原 reviewer 针对修复复审，5 项均关闭，未发现修复直接引入的新可确认缺陷。没有因此跳过公共参数校验、改变 shader ABI、覆盖优先级、渲染顺序或 fence 保活。此结论受下述验证范围限制，不能推导为所有规模、所有输入均更快。

仍有明确的性能取舍：8192 个每帧完全变化、无共享机会的常量块微基准中，修复后相比旧基线多约 11.6% 缓存层 CPU 时间。原先数量级的全表逐出退化已消除；剩余有界缓存维护和低命中成本尚未进一步拆分，保留为后续方向，没有将其解释成全部是测量噪声。

## 独立性与范围

两名首次 reviewer 均以 `fork_turns: none` 创建，未参与实现，不继承主会话。简报分别覆盖语义/通用性及常量/GPU/原生路径，读取仓库规范、基线 diff、当前调用链与原始证据；仅在 `out` 写探针、日志和报告。主 agent 未将自审结论作为 reviewer 前提，并对各 finding 自行复核。

首轮报告：`out/MaterialOptimizationAudit/Semantics/Review.md`、`Gpu/Review.md`。针对性复审：相应目录 `ReReview.md`。CPU reviewer 独立运行语义/容量探针，GPU reviewer 独立构建旧新常量 CPU mock 探针并核算 28 份已有 Viewer CSV；复审时又核对 18 份修复后压力日志。GPU 集成套件由主 agent 执行，两名 reviewer 阅读并区分这些日志与自己的实验。

## Findings 与处置

| ID / 优先级 | 主 agent 复核结果、修复和验证 | 复审 |
| --- | --- | --- |
| SEM-01 / P2 | **确认**：collection ordinal 是 primitive 局部编号，旧 Draw key 缺少 primitive 身份，两个 ordinal=0 的 draw 会错误复用自定义 provider。key 补全 Scene/Slot/Generation、LocalItemId 存在和值，保留 frame/family/view/ordinal。真实 session 和独立探针覆盖相同/不同/缺省 ID；两个 provider 执行、key/seed 不同。 | 关闭 |
| SEM-02 / P2 | **确认**：64 项 LRU 在固定顺序扫描 65/128 项时，暖帧由基线的 64 次复用变为 0 次。保留本帧已访问热集，溢出不抢占本帧条目，旧帧冷项仍可被替换；完整命中/增量刷新同步触碰 Object token。64/65/128、工作集更换、View 刷新计数通过。 | 关闭 |
| SEM-03、GPU-01 / P2 | **确认并合并原因**：provider/常量每个预算逐出都全表找最旧项，默认 4096 项产生 O(新项数 × 驻留项数) 成本。稳定 bucket list 加独立访问顺序索引，O(1) 选择 victim、O(log N) 找桶；命中 splice、替换/Collect/Clear/插入回滚同步。预算、完整 key/value 比较和旧 slice 保活保持。 | 两项关闭 |
| GPU-02 / P3 | **确认**：Close 复制含一张空闲页的统计，只清 LivePages 后析构缓存，PageBytes 留下 65536。同步清两个实时指标，累计工作量保留；完整 session 关闭后读取公开统计的断言通过。不是实际泄漏。 | 关闭 |

回滚路径的静态复核包括 entry/list-recency 两次插入失败：只在节点和访问索引都存在后增加计数；失败清理新节点和空桶。允许已逐出的缓存命中丢失，但不损坏外部冻结值和已发布 slice。未动态注入各 allocation site 的 `bad_alloc`。

## 压力证据

### 通用解析与 provider

独立调用真实 `ReuseEvaluation/CacheEvaluation`，64/65/128 项的暖帧 reuse/full 分别为 **64/0、64/1、64/64**，恢复基线 admission 的命中数。真实 session 测试进一步覆盖冷工作集整体更换、增量 View 刷新与 descriptor/PSO 查询数。

独立 Debug provider 探针：4096 次预热后计时 1024 次 revision/value 更新，scope owner 一直存活，byte 限额统一为 64 MiB。4096 条默认容量由修复前 **305.76 ms** 降到 **32.07 ms**；8192 条无逐出对照为 17.01 ms，128 条为 18.99 ms。该跨时段小型 CPU 实验用于确认算法热点，不是正式 Viewer FPS 比较。

证据：`out/MaterialOptimizationAudit/Main/SemanticsFixed/Probe.log`、`CacheThrash.log` 与可运行脚本。64 次全 scope 增量/完整求值差分、混合 View/Object、同 key 内容变化、provider 消失/default/返回、失败保留旧解析与之后恢复均通过。

### 常量缓存

相同 MSVC `/O2`、profiling OFF、CPU mock device，每 draw 一个 16-byte block、一个有效 float，每 draw 每帧不同值。每进程 12 帧，忽略前 3 帧，对余下 9 帧取均值，再取 3 次交错旧新进程中位数。测试层是常量缓存，不是实际 D3D12/GPU 帧时。

| draws/帧 | owner 保留帧 | block 限额 | 同期旧基线 ms/帧 | 修复后 ms/帧 | 两边 pack 数 |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 4096 | 3 | 4096 | 4.330 | 3.113 | 49152 |
| 8192 | 1 | 4096 | 8.722 | 9.738 | 98304 |
| 2048 | 3 | 128 | 1.695 | 1.276 | 24576 |

首轮 4096/3/4096 的修复前候选约 52.865 ms；修复后仍逐出 45056 次、pack 数不变，下降不是扩大预算或减少正确上传所得。8192/1/4096 的三次配对均略慢，中位数 +11.6%；当前证据不足以将剩余成本分别归因于某一种分配、索引或共享句柄操作。

证据：`out/MaterialOptimizationAudit/Main/FixChurnSummary.txt`、18 份 `Churn-*.log`；复核 `Gpu/FixEvidenceCheck.json`。逐出/清空后的旧 GPU 像素、等待 GPU 后回到一张空闲页、独立 entry/byte 限额的原有回归均继续保留并通过。

## 修复后 Viewer 与集成验证

四次运动场景同期旧新复测，均 240 帧预热、600 样本、194–195 draws；同编译器/配置，runtime profiling OFF、GUI/validation 开启、无 vsync/RenderDoc，顺序运行而不与构建/测试并行。这是每配置各一次旧新运行，支持修复后收益方向，不代替审计前 28 次重复测量或 3600 帧长跑。

| 配置 | 旧基线 mean / P95 ms | 修复后 mean / P95 ms |
| --- | ---: | ---: |
| Debug | 20.981 / 22.302 | 13.016 / 14.121 |
| Release | 3.959 / 4.743 | 2.762 / 3.351 |

证据在 `out/Profiling/MaterialOptimization/Audit`，每次保留 `Metadata.json`、`Frames.csv`、CMake 编译器信息；`Summary.json` 保存二进制与配置 SHA256、命令和 CSV 重算。审计前长跑及多轮原始数据保留原版本归属，未将其冒充为修复后长跑。

| 修复后检查 | 结果 | 日志（`out/MaterialOptimizationAudit/Main/`） |
| --- | --- | --- |
| 独立 Debug 全量构建，Tracy OFF / RenderDoc ON | 通过 | `ChecksBuild.log` |
| 独立 Debug 全套 CTest | **45/45，101.77 s** | `ChecksCTest.log` |
| Release 全量构建，Tracy ON / RenderDoc OFF | 通过 | `ReleaseBuild.log` |
| Release 全套 CTest | **40/40，89.87 s** | `ReleaseCTest.log` |
| 初始修复后 Debug 材质绑定/渲染 | **2/2，4.63 s** | `FixTests.log` |
| 格式、语义命名 | 246 源码、151 编译单元通过 | `Style.log` |
| 模块边界 | 241 源码、25 模块通过 | `Boundaries.log` |

全套包含通用材质、场景、多视图、旧帧像素、失败 Present、默认离线启动、RenderDoc 与捕获失败恢复、真实 trace 接收/导出。不同配置的注册测试不同，不将 45/40 理解为同套测试缺项。

## 后续可选方向

1. **优先压低低命中、高变化输入的缓存维护成本。** 用命中率/依赖频率指导通用 admission，或减少访问索引和候选节点的重复分配。先用 128/4096/8192 draws、1–3 帧 owner、多个 block/program 对照 full lookup、pack、上传、容量与 P95；保持公共完整校验和旧 slice 所有权。此项直接回应已测的低复用额外开销。
2. **参数计划中使用预解析的 provider/scope 索引。** 仅在多参数 schema、稀疏变化时的名称查找与参数遍历成为热点后实施。用 32/65/256 参数及自定义混合依赖验证，保留 override、default 和 provider 消失/返回。
3. **按实际需求构造 Object 派生输入。** 标准 provider 可只计算所需矩阵，任意自定义 provider 仍需满足其声明可读取的 scope 内容。以 Object-heavy 场景衡量，覆盖 Normal/WVP、clip-space、signed zero 和混合依赖；不能按 shader 名称猜测需求。
4. **共享编译后的结构化 ABI/mapping 身份。** 让兼容的不同 program 更早共享 Global/View 块，降低完整 lookup。保持碰撞后的完整相等检查，并用多 program/多 view 的实际查找次数和 PrepareDraws 时间验证。
5. **publication 查找索引为较低优先级。** 只有 `ValidateConstant` 成为新热点后再替换当前线性查找，同时保留 stale publication、range、device 等负向校验。本次没有证据要求扩展此路径。

上述是后续候选，尚未实施。未测试真实硬件 device removal、系统 OOM、所有 allocation site 故障或真实跨 heap 切换；当前设备只有一组全局 descriptor heaps。任意持续变化的工作集顺序和全部反射极限布局也未穷举。独立审核和现有测试提供范围内证据，不等同于无任何风险。
