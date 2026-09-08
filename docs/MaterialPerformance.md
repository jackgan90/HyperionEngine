# 材质参数更新性能

2026-09-08，`optimize-material-parameter-updates`。优化落在 Materials、Renderer、RHI 和 D3D12 的通用路径；未修改 Applications、Plugins、shader、场景数据、CLI 或现有 target。完整决策与验证记录见 [OpenSpec 实施记录](../openspec/changes/archive/2026-09-08-optimize-material-parameter-updates/implementation.md)，接口见 [Materials.md](Materials.md)。

## 同程序条件下的前后结果

保留源码基线 `ef7a1ef7e6f3cdac006ae9c1248573867cc98c32` 构建的 `hyperion_viewer_baseline.exe`，与优化后的 `hyperion_viewer.exe` 交错运行。每个构建、每种静止/运动状态各测新旧三轮；第二轮反转新旧顺序，共 24 次 runtime-off 运行。表中 mean/P95 分别取三轮对应统计量的中位数，不是将 1800 帧合并后重算 P95。

| 构建 / 相机 | 优化前 mean / P95 ms | 优化后 mean / P95 ms | mean 降低 |
| --- | ---: | ---: | ---: |
| Debug / 静止 | 12.555 / 15.334 | 10.531 / 13.004 | 16.1% |
| Debug / 运动 | 24.356 / 29.369 | 14.249 / 17.436 | 41.5% |
| Release / 静止 | 2.558 / 3.146 | 2.392 / 2.937 | 6.5% |
| Release / 运动 | 3.859 / 5.103 | 2.663 / 3.192 | 31.0% |

运动 mean 对应 Debug 约 41.1 → 70.2 FPS、Release 约 259.1 → 375.5 FPS。原始三轮 mean：

| 构建 / 相机 | 优化前 ms | 优化后 ms |
| --- | --- | --- |
| Debug / 静止 | 12.293、12.555、13.142 | 10.243、11.002、10.531 |
| Debug / 运动 | 24.032、25.434、24.356 | 14.054、14.249、14.349 |
| Release / 静止 | 2.626、2.514、2.558 | 2.646、2.392、2.337 |
| Release / 运动 | 3.859、3.852、3.901 | 2.846、2.663、2.663 |

初始调查中的 Debug runtime-off 中位数为静止 11.008、运动 20.883 ms；本次交错复测旧程序更慢，说明环境有时间波动。最终收益采用上表同期新旧对照，未将初始单轮或最快样本混入。Release 静止第一轮略慢，静止收益应结合其较小差异和波动理解。

条件保持一致：RTX 5080、MSVC 19.50.35725、1440×900、`experiments/Scene.json`、78 模型、194–195 scene draws；GUI 和 D3D12 debug layer 开启，VSync 与 RenderDoc 关闭，隐藏窗口，不附加调试器。每轮预热 240 帧、采样 600 帧。Debug 为 `/Ob0 /Od /RTC1`，Release 为 `/O2 /Ob2 /DNDEBUG`。两种测量 exe 均编入 Tracy，runtime-off 不连接 collector、不启用 scope；没有通过改变 Debug 优化级别或关闭验证得到收益。所有 GPU 测量串行进行，期间未并行构建或跑 GPU 测试。

## CPU 热点与操作量

下表使用初始调查和最终各一轮 Detail+GPU trace，只用于解释工作消除，不替代上面的重复统计。每轮均为 240+600 帧。最终 Debug 静止/运动帧 mean 为 11.989/15.605 ms，Release 为 2.388/2.970 ms；采集开销和环境噪声均包含在内。

| 运动 CPU 区间，ms/帧 | Debug 初始 | Debug 最终 | Release 初始 | Release 最终 |
| --- | ---: | ---: | ---: | ---: |
| PrepareMaterials | 7.605 | 3.002 | 0.604 | 0.326 |
| RefreshMaterialEvaluation | 6.915 | 2.518 | — | 0.280 |
| BindMaterialConstants | 2.706 | 0.954 | 0.537 | 0.116 |
| PrepareDraws | 4.095 | 2.436 | — | 0.212 |
| RecordGraphicsBindings | 0.402 | 0.271 | — | 0.208 |

Refresh 是 PrepareMaterials 的子区间，BindConstants 是 PrepareDraws 的子区间，不能逐行相加。最终 Debug 的 EvaluateChangedParameters 约 1.532 ms/帧，CommitMaterialEvaluation 约 0.544 ms/帧；它们仍有逐 item 的依赖检查和当前解析对象维护成本，不代表所有运动 CPU 工作都消失。

最终运动 600 帧的材质计数：

- 600 次 pack、48,064 B 上传，平均 80.107 B/帧；598 帧为 80 B、一帧为 0、一帧为 224 B，与基线相同。节省来自 CPU 准备，不是减少正确的 View 数据上传。
- 平均 2.005 次完整常量查找和 776.102 次已准备块复用；快表按不可变 program/binding 区分，不同程序的兼容块仍经通用完整缓存共享。基线每次 draw 刷新都会查询四个块，约 778 次/帧。
- provider 实际求值平均 2.002 次/帧，约 387.062 次共享结果命中；新增对象仅触发一次完整材质求值。
- 静止 600 帧无材质 pack/upload，全部 194 个 draw 复用解析结果。
- 全部 600 帧没有新建 binding set 或 PSO；唯一一次 set/PSO 缓存查询对应新可见对象。纯数值刷新保留资源身份。
- D3D12 每帧四次 list 录制汇总后，root/heap 各 2 次实际绑定；Debug 运动 CBV 约 369.947 次、descriptor table 176 次。计数按实际命令累计，重复 draw 仍经过完整校验。

## 通用实现与预算

冻结 scope 输入和值树只在发布边界验证/排序，之后共享不可变存储；Global/Scene 相同写入保留 revision/token。Material 相同写入在复制 snapshot 前返回。解析值/依赖表采用每页 8 项的 copy-on-write 表，前 4 页引用内联，扩展页支持更大的 schema；scope qualifiers、provider 结果同样共享。初始化时计算有效 provider 参数索引，避免刷新时反复解析 override 名称。

增量求值支持全部 scope，包括 Frame/Pass/Draw 和混合 Object×View。覆盖/default/required 规则保留；作者 snapshot、program 或显式名称覆盖变化仍进入完整验证。任意反射 cbuffer 都可复用未变 slice，混合块在有效成员值变化时重新打包。公共缓存调用保留完整布局、映射、值和 scope 检查，不能只凭裸地址或调用方 revision 跳过验证。

| 缓存 | 默认政策 | 字节边界 |
| --- | --- | --- |
| 每 primitive 的 Object token / evaluation | 各最多 64 项，保护本帧已访问热集、替换旧帧冷项 | 当前活跃输入与值表，无历史链；溢出可直接求值 |
| Provider | 4096 项，16 MiB 值树估算，插入时按访问顺序索引淘汰 | 包含缓存输入/输出及 qualifiers；不含外部资源载荷和全部分配器开销 |
| 常量候选 | 4096 块，16 MiB 对齐 slice extent | 同逻辑 key 新值替换旧记录；不限制外部保活 |
| 跨 draw 准备快表 | 512 块，每 program/binding 保存最近状态 | 默认 64 KiB 页面下至多表示 32 MiB slice；实际页面可能被其他 owner 保留 |
| Prepared draw | 每个活跃资源组合只保存当前状态，随弱 owner 退休 | 活跃集仍由资源租约决定，无全局硬字节预算 |
| 页面 | 64 KiB，短期/长期两组；最多一张空闲页 | 旧 frame/list/fence 未释放前绝不覆写 |

最终 Debug 运动 trace 中 provider 为 587–594 项、约 613–619 KiB 估算值树；常量候选 201–204 块、约 50–51 KiB，准备快表 8 块，真实常量页容量 128–192 KiB。Release 对应候选最多 205 块。正常场景没有预算淘汰；容量约束通过独立压力用例验证。活跃纹理/PSO/descriptor 的生命周期管理保持原协议，不能把上述缓存预算描述为进程内存上限。

3,600 帧连续运动（240 帧预热）mean 14.789 ms、P95 18.153 ms；六段各 600 帧 mean 为 14.443、16.015、14.526、14.748、14.528、14.473 ms，无持续变慢趋势。进程 Private Bytes 五秒均值从 5–10 秒的 452.815 MiB 到 50–55 秒的 453.871 MiB，后段持平；Working Set 从 239.327 到 240.516 MiB。此有限时长观测包括分配器和 benchmark 样本存储，不是对所有工作负载的内存证明。

## 验证与复现

Debug/Release（Tracy ON、RenderDoc OFF）全量构建通过，各 **40/40 CTest**，分别 145.04/95.01 秒。覆盖材质、场景、失败 Present 与真实 trace 接收/导出。新增通用 GPU 用例使用自定义八常量块 shader、24 draws，验证 Frame=1 pack、Pass=1、Draw=24、View=1+24 混合块、Object=48 对象/混合块，并检查旧新帧像素和资源/PSO 查询计数；不依赖 Viewer。

缓存压力将所有 scope owner 保持存活，连续 512 次修改同 key 值并变化 View revision，分别用 entry 限额和字节限额触发淘汰；旧 slice 在 Clear 后仍绘出原像素，等待 GPU 后回到一张空闲页。provider 压力覆盖两种预算、旧结果保活及无关资源退休；解析表覆盖 65 参数的扩展页和无变化写入。原生回归覆盖相同 draw 重复绑定、新 list、不同 root 的 A/B/A/A 切换、实际像素、原有取消及失败 Present 恢复。设备当前只使用一组全局 heaps，未单独构造实际 heap 切换；未做真实硬件 device removal 或系统 OOM。

独立 Debug 配置 Tracy OFF、RenderDoc ON 全量构建通过，完整 CTest **45/45**（108.59 秒），包含默认无监听、RenderDoc replay 和捕获失败恢复。分离 entry/byte 限额后的两个 profiling 构建重建受影响测试，Debug/Release 材质回归各 **2/2**（4.19/2.83 秒）。格式、151 编译单元的语义命名、241 模块源码/25 模块边界与 OpenSpec strict 检查通过。完整日志见 OpenSpec `implementation.md`。

原始证据均保留于 `out/Profiling/MaterialOptimization/Final`：`Summary.json` 保存 28 次短运行的命令、统计、exe/CMake SHA256；`Analysis.json` 保存 trace 汇总及长运行数据；每次 detail 目录保留原始 Tracy、CSV 和日志。旧基线原始数据位于 `out/Profiling/MaterialInvestigation`。这些生成数据在 `out` 下，不加入版本控制。

```powershell
# Debug 同配置复测；--static 切换到静止。输出目录须不存在。
python tools/Profile.py --out out/Profiling/MaterialRepeat --viewer out/build/material-perf-debug/bin/hyperion_viewer.exe --no-build --mode off --warmup 240 --frames 600 --timeout 180
# 将 viewer 改成同目录 hyperion_viewer_baseline.exe 可复测保留的旧二进制。
# --mode detail-gpu 增加真实 trace；Release 使用 out/build/profile/bin/hyperion_viewer.exe。
```

没有实施 GUI 私有常量缓存、publication 查找索引或新 arena：初始真实上传和 validation 数据不足以支持扩展这些次要路径。本轮保留剩余热点与明确边界，后续可基于新的工作负载继续优化。

## 独立审计补充

上文 28 次短运行及 3600 帧长运行对应审计前的交付快照。随后两名无上下文继承的 reviewer 分别检查语义/通用性和常量/GPU/原生路径，发现并复核四类问题：跨 primitive 的 Draw key 碰撞；64 项最近访问淘汰在 65/128 项循环扫描下全部失去复用；provider/常量默认容量满载后逐出扫描全表；Close 后 `PageBytes` 残留。修复沿原有模块与生命周期完成，没有扩大公共 API 或绑定 Application。

逐出改为稳定节点加访问顺序索引，选择最旧项为 O(1)、定位删除桶为 O(log N)；64 项解析缓存保护本帧热集，完整命中和增量刷新都维护 Object 访问帧。逐出仍只释放自身引用，原有旧帧像素/容量/回收回归保留。详细审计版本、修复前后压力探针、后续验证和限制见 [独立审计报告](../openspec/changes/archive/2026-09-08-optimize-material-parameter-updates/audit.md)。

原 reviewer 对修复冻结的 51 文件版本复审，5 项 finding 均关闭。修复后 Debug 全套 45/45（101.77 s）、Release 全套 40/40（89.87 s），格式/命名/模块边界通过。每配置一次同期 Viewer 运动复测仍有收益：Debug 20.981→13.016 ms、Release 3.959→2.762 ms，均预热 240、样本 600；不将此单轮复测代替原重复矩阵或长跑。

容量压力中，4096 draws/三帧 owner 的常量 CPU 层由审计候选约 52.865 ms 降到 3.113 ms，同期旧基线 4.330 ms；8192 draws/一帧 owner 的低复用对照仍为旧基线 8.722、修复后 9.738 ms（中位数约 +11.6%）。全部变化时的有界缓存维护成本因此列为优先后续方向，没有承诺全部负载提速。证据 `out/Profiling/MaterialOptimization/Audit/Summary.json`、`out/MaterialOptimizationAudit/Main/FixChurnSummary.txt` 与独立复审报告。
