## 最终实现

基线和原始数据：`out/Profiling/MaterialInvestigation`，基线源码 `ef7a1ef`。保留同目录 `hyperion_viewer_baseline.exe` 可执行文件，便于最终交错复测。

2026-09-08。修改位于通用 Materials/Renderer/RHI/D3D12、对应测试与文档，无 Applications/Plugins/shader/场景特判，无新依赖或 shader ABI/序列化/CLI/target 变化。

- 冻结 scope 值树、qualifiers 和 provider 结果共享不可变存储；输入在发布时验证/排序，Material/Global/Scene 的无变化写入不复制或失效。
- 解析值与依赖使用每页 8 项的 copy-on-write 表，前 4 页引用内联，扩展页支持更多参数；无历史链。provider 参数索引在完整准备时建立，刷新不重复解析 override 名称。
- 全 scope 有效依赖控制增量求值，Frame/Pass/Draw 不强迫完整解析。混合 Object×View 在派生矩阵更新后求值；保留 override/default/required、正负零和资源 owner 规则。作者 snapshot/program/显式名称覆盖变化仍完整验证。
- 每个 prepared draw 复用未变反射块，program/binding 快表共享当前块；公共 Bind 保留完整布局、映射、值和 scope 检查，同 key 新值替换历史。纯数值更新保留资源身份。
- Object/evaluation 各 64 条；provider 默认 4096 条、16 MiB 估算；常量候选 4096 块、16 MiB extent；准备快表 512 块。预算压力按最近访问淘汰，owner 过期正常 Collect。64 KiB 页面分短期/长期两组，淘汰不覆写旧 frame/list/fence 保存的 slice。
- 原生绑定状态只活在一次 Record 内，root 变化清全部参数、heap 变化清 tables、新录制从空状态开始；逐 draw 校验及 GPU 保活不变。增加完整查找、准备复用、淘汰、存量、真实页面容量和原生命令数统计。

详细接口及统计口径见 [Materials.md](../../../docs/Materials.md) 和 [Profiling.md](../../../docs/Profiling.md)。

## 实验与计划调整

第一次运动实验 `Stage1Moving` mean 28.018 ms，暴露增量路径临时加入的 override 名称解析开销；已将其移到缓存建立时。第二次 `Stage2Moving` mean 20.020 ms，PrepareMaterials 4.363 ms，RefreshMaterialEvaluation 3.849 ms/frame，BindMaterialConstants 2.937 ms/frame。两者均为独立 Debug、detail-gpu、240 warmup + 600 samples、194–195 draws；仅为中间实验，不能当作最终结果或重复样本统计。

Stage3 完成反射块与原生复用后 mean 17.455 ms；Stage4 加入内联页引用、共享 qualifiers 和避免无关上下文复制后为 14.937 ms。最终函数分解与新增 Detail 区间后采用正式交错测量，不用中间单轮数字计算收益。未引入 arena 或链式 overlay，未为每 scope 建独立空闲池。GUI 常量、publication 索引与 reservation 保持现状：初始上传约 0.021 ms/帧、ValidateDraws 未随相机运动增长，不足以支持扩展次要路径。

## 正式性能证据

条件为 RTX 5080、MSVC 19.50.35725、1440×900、78 模型、194–195 draws，GUI/debug layer 开启，VSync/RenderDoc 关闭。Debug `/Ob0 /Od /RTC1`，Release `/O2 /Ob2 /DNDEBUG`，均编入 Tracy；runtime-off 不采集，未以关闭验证或改变 Debug 优化级别获取收益。GPU 测量串行，与构建/测试分开。

24 次新旧交错 runtime-off 运行，每次 240 预热+600 样本，第二轮反转顺序。下表为三轮 mean/P95 各自的中位数。

| 构建 / 相机 | 之前 mean/P95 ms | 之后 mean/P95 ms | mean 降低 |
| --- | ---: | ---: | ---: |
| Debug / 静止 | 12.555 / 15.334 | 10.531 / 13.004 | 16.1% |
| Debug / 运动 | 24.356 / 29.369 | 14.249 / 17.436 | 41.5% |
| Release / 静止 | 2.558 / 3.146 | 2.392 / 2.937 | 6.5% |
| Release / 运动 | 3.859 / 5.103 | 2.663 / 3.192 | 31.0% |

初始调查 Debug runtime-off 为 11.008/20.883 ms，本次旧 exe 复测更慢；最终收益使用同期交错数据。Detail 单轮用于定位：Debug 运动 PrepareMaterials 7.605→3.002 ms、BindMaterialConstants 2.706→0.954 ms；Release 为 0.604→0.326、0.537→0.116 ms。子区间不与父区间重复累计。

运动 600 帧仍上传 48,064 B、pack 600 次，平均完整常量查找 2.005 次/帧、准备复用 776.102 次/帧。快表按 program/binding 区分，不同程序的兼容块仍经完整缓存共享。静止无材质上传；运动没有新建 set/PSO，仅新可见对象一次完整准备。Debug 运动 provider 最多 594 项/633,998 估算字节，常量候选最多 204 块/52,224 extent 字节，准备块 8 个，真实页面 128–192 KiB。

3,600 帧持续运动 mean 14.789 ms、P95 18.153 ms；六段为 14.443、16.015、14.526、14.748、14.528、14.473 ms。Private Bytes 五秒均值从 452.815 到 453.871 MiB，后段持平，无持续退化趋势；这是有限观测，不是全局硬内存保证。

完整方法、全部三轮数据和剩余热点见 [性能报告](../../../docs/MaterialPerformance.md)。正式 28 次短运行及一次长运行在 `out/Profiling/MaterialOptimization/Final`，保留 trace/CSV/日志、`Summary.json`、`Analysis.json`。baseline SHA256 与初始保存记录一致：Debug `2869392377198ae24d34e7a206d6ae8691247f8437c341fc02efe992b64b6b36`，Release `e16c46a121174171f6dd699837d66b0b2df312b0f340bfe08f1ad8cf76070701`。优化后分别为 `79728659eaf64b574f721c474412c928218a1305ba2199749da5e308ba36938a`、`61d97f891ed6d7a9f7af49ba454ccdeee83ab4346fd8f3906ea0779d59e1123c`，全部短运行哈希一致，29 次日志均为零 validation errors。

## 验证

| 检查 | 结果 | 日志 |
| --- | --- | --- |
| Debug / Release 全量构建，Tracy ON、RenderDoc OFF | 通过 | `out/MaterialOptimizationDebugAllBuild.log`、`out/MaterialOptimizationReleaseAllBuild.log` |
| Debug 全套 | 40/40，145.04 s | `out/MaterialOptimizationDebugCtest.log` |
| Release 全套 | 40/40，95.01 s | `out/MaterialOptimizationReleaseCtest.log` |
| Debug 全套，Tracy OFF、RenderDoc ON | 45/45，108.59 s | `out/MaterialOptimizationChecksCtest.log` |
| 分离 entry/byte 限额后的 Debug / Release 材质回归 | 2/2、2/2，4.19/2.83 s | `out/MaterialOptimizationFinalDebugMaterialTests.log`、`out/MaterialOptimizationFinalReleaseMaterialTests.log` |
| 格式 / 语义命名 | 246 源码、151 编译单元通过 | `out/MaterialOptimizationStyle.log`、`out/MaterialOptimizationFinalNaming.log` |
| 模块边界 | 241 源码、25 模块通过 | `out/MaterialOptimizationBoundaries.log` |
| OpenSpec strict | 通过 | `openspec validate optimize-material-parameter-updates --strict` |

独立 Debug `material-optimization-checks` 配置 Tracy OFF、RenderDoc ON，全量构建与完整套件通过，包含 `offline_startup`、RenderDoc replay 验收和 capture failure recovery。对应构建日志为 `out/MaterialOptimizationChecksBuild.log`；后续只补充分离 entry/byte 限额的测试，两个 profiling 构建重建并重跑受影响的两项测试均通过，引擎实现和测量 exe 未再变化。

新增通用八常量块 shader/24 draws 验证 Frame/Pass 各 1 pack、Draw 24、View 1+24 混合块、Object 48 对象/混合块，并核对真实旧新帧像素与资源/PSO 查询。512 次缓存压力保持 owner 存活，分离 entry/byte 限额并变化同 key 值和 View revision，Clear 后旧 slice 仍正确，WaitIdle 后一张空闲页。provider 压力、旧结果保活、无关资源退休和 65 参数扩展页均覆盖。原生重复 draw、不同 root A/B/A/A、新 list、原有取消/失败 Present 回归覆盖状态和生命周期。

设备当前只有一组全局 heaps，未单独构造真实 heap 切换；未注入实际硬件 device removal 或系统 OOM。预算不覆盖外部保存帧、GPU 在途对象、全部活跃资源与分配器开销。直接操作 Renderer 的调用方已迁移不可变输入、共享 provider 结果、分页表和 qualifiers 访问方式，CPU 材质作者协议保留。

完成后保留待审阅的工作区与 artifacts，不自动归档或提交。

## 独立审计后的更新

后续用户要求的两组独立 review 发现并复核 5 项问题（4 类）：Draw 身份碰撞、64 项扫描抖动、provider/常量逐出的全表扫描及关闭后 PageBytes。修复和原 reviewer 的针对性复审已完成；详见 [audit.md](audit.md)。修复复审冻结版本为 `out/MaterialOptimizationAudit/Fixed/Manifest.json` 的 51 文件，之后仅同步报告/规范/任务，源码哈希保持一致。

修复后独立 Debug（Tracy OFF、RenderDoc ON）全套 45/45、101.77 秒；Release（Tracy ON、RenderDoc OFF）全套 40/40、89.87 秒；格式、151 编译单元命名和 25 模块边界检查通过。实际日志见 `out/MaterialOptimizationAudit/Main/{ChecksBuild,ReleaseBuild,ChecksCTest,ReleaseCTest,Style,Boundaries}.log`。

审计压力探针确认：65 项恢复 64 reuse/1 full；provider 默认容量的 1024 次更新约 305.76→32.07 ms；4096 draws/三帧 owner 的常量 CPU 层从审计候选约 52.865 ms 降为 3.113 ms，同期旧基线 4.330 ms。8192 draws/一帧 owner 的低复用对照仍比旧基线高约 11.6%，作为明确的后续优化方向记录，未声称所有负载都提速。

修复后每配置一次同期 Viewer 运动复测：Debug 20.981→13.016 ms、Release 3.959→2.762 ms，均预热 240、样本 600、194–195 draws，证据与 SHA256 位于 `out/Profiling/MaterialOptimization/Audit/Summary.json`。前文 28 次/3600 帧数据保持审计前版本归属，未重跑完整性能矩阵或长跑；实际 heap 切换、硬件 device removal、系统 OOM 和逐分配故障注入仍未验证。
