## Context

基线 `ef7a1ef`；证据 `out/Profiling/MaterialInvestigation/Findings.md` 与 `Summary.json`。Debug runtime-off 静止/运动三轮 mean 中位数为 11.008/20.883 ms。Detail trace 中 PrepareMaterials 为 0.779/7.605 ms，BindMaterialConstants 为 0/2.706 ms，运动平均只上传约 80 B 场景常量。

Materials 保存 CPU 定义/实例；Renderer 负责冻结输入、provider、解析、反射打包和设备缓存；D3D12 负责原生命令与 fence 生命周期。此次优化沿现有边界进行。

## Goals / Non-Goals

**Goals:**
- 未变输入按不可变引用共享；变化只影响有效依赖相关参数和块。
- 通用 Frame/View/Object/Pass/Draw 与混合依赖正确更新，保留 override/default/required 语义和旧帧回放。
- 高频历史有明确缓存预算，GPU 在途对象不被提前释放；统计区分缓存占用和外部保活。
- 用操作计数及同口径 Debug/Release 测量证明优化；完成必要回归和仓库规范检查。

**Non-Goals:**
- 不加入 Application、插件 ID、PBR 名称或固定 draw 数的优化条件。
- 不改变 shader ABI、渲染顺序、同步语义、Debug 优化级别或验证强度。
- 不以整个进程硬内存上限代替缓存预算；不引入材质编辑器、bindless 或新渲染后端。

## Decisions

1. **不可变输入与局部解析。** `FMaterialInputValues` 在构造/替换边界排序并验证完整输入，复制时共享整个不可变值树；Frame/view 引用未变 Global/Scene 内容。无变化 setter 在复制 snapshot 前退出。解析表采用每页 8 个句柄的 copy-on-write 存储，前 4 页引用内联，超过 32 参数时使用扩展引用表；依赖表同样共享，深度固定且没有历史链。scope qualifiers 与 provider 结果共享不可变存储。初始化时预计算未被 override 的 provider 参数索引，刷新时不重复解析 override 名称。
2. **依赖驱动的通用求值。** 预先建立参数索引、provider 映射和块成员映射。有效依赖考虑 override、provider 缺失/default 和混合 Object×View。Frame/Pass/Draw 变化只刷新相关内容；Object 派生矩阵先更新再供混合 provider 使用。资源值及其 owner 变化单独控制资源身份，纯数值更新不重做资源/PSO 准备。
3. **块级准备复用。** 未改变块直接引用已有 slice；相同有效输入及兼容 ABI/mapping 的共享块只做一次昂贵查找/打包。支持任意反射 cbuffer，混合块仍整体打包。不能只比较裸地址/hash/revision：公共调用方相同 key 不同值、布局变化仍必须正确；快速身份仅来自受控不可变发布。
4. **有界缓存。** 保留 owner 过期退休，补充参数/provider/常量历史预算及淘汰计数。每 primitive 的 Object/evaluation 缓存各 64 条，按最近访问帧淘汰。provider 默认 4096 条、16 MiB owned value-tree 估算字节（不计外部资源和全部分配器开销）；内置 provider 不强持无关 scope 值。常量候选默认 4096 条、16 MiB 对齐后的 slice extent，准备快表最多 512 块；同一完整逻辑 key 的新值替换旧记录，容量压力按访问时间淘汰。淘汰只释放缓存引用，不覆写旧 slice。收集覆盖无新帧、异常和 Close。`PageBytes` 记录真实页面容量（包括外部保存帧与活跃 draw 的保活），不能把候选 slice 字节预算误称为进程或全部页面硬上限。View/Pass 与 Frame/Draw 使用短期页面池；小预算、长期存活 owner 的压力回归证明容量约束与旧帧回放。淘汰仅在插入时按需进行，没有额外的每帧全缓存预算扫描。
5. **命令列表内绑定状态。** D3D12 记录相同 root signature、heap、CBV、descriptor table 状态，避免重复命令。list reset/新录制/失败恢复隔离状态，root/heap 切换使对应绑定失效。保留逐 draw 语义校验，不通过重排 draws 获取收益。
6. **按测量迭代。** 中间实验推动了 override 计划预计算、内联页引用、共享 qualifiers 和减少刷新上下文复制。保留 8 项页粒度，避免逐帧 arena 引入额外保活协议；不引入链式 overlay。页面只按短期/长期两组分配，不扩展每 scope 的多个空闲池。GUI 常量路径、publication 索引及批量 reservation 未修改：基线真实上传约 0.021 ms/帧、逐 draw validation 未随镜头运动增加，不足以支持本轮扩展这些次要路径。所有代码修改位于 Materials/Renderer/RHI/D3D12 及通用测试，无 Application/Plugin 特判或消费者改写。

## Risks / Trade-offs

- [共享对象增加引用计数/间接访问] → 比较实际复制量、分配量和 scope 时间；不建立逐更新链式快照。
- [过粗失效或不完整依赖导致旧值] → 多 scope、override/default 切换、资源切换、正负零和同 key 不同值测试。
- [淘汰引发重传或回收尖峰] → 小预算压力、正常帧与长期运行、缓存存量和 P95 联合测量；已提交对象仍由 fence 保活。
- [原生状态缓存漏重绑] → 多 list、root/layout/heap 切换、旧帧回放和失败 Present 测试。
- [测量噪声/新瓶颈] → 同编译器和配置、相同 draw 数、交错重复样本；保留原始 trace，不承诺预估 FPS。

## Migration Plan

保留 CPU 作者接口和 shader/serialized 协议；Renderer 内部解析表示及新增统计的调用方同步迁移。按任务分批构建测试，行为失败时先修复再推进性能比较。无需数据迁移或外部发布。

## Resolved Implementation Choices

上述存储粒度、默认预算、淘汰时机和两组页面池均已实现并有容量/像素回归。默认预算是可调整的保守起点，不保证全部工作负载都最优；若活跃集大于预算，允许重新求值/上传。Material snapshot、program 或显式名称覆盖改变仍进入完整验证，纯频率输入变化走增量解析。原生状态缓存只在一次 list 录制栈内存在，root 变化清全部参数，heap 变化清 tables；当前设备使用一组全局 descriptor heaps，因此真实跨 heap 切换不单独构造测试。

独立审计修正了缓存边界策略：Object/evaluation 的 64 项限制按帧保护已访问热集；溢出项只在存在旧帧冷项时入缓存，工作集更换仍能替换旧项。完整命中和增量刷新均触碰对应 Object 的访问帧。Provider/常量候选改用稳定链表节点和独立访问顺序索引，O(1) 更新顺序及选择最旧项，随后 O(log N) 定位删除桶；同 key 替换、Collect、Clear 和插入失败保持索引/统计一致。这样默认 4096 项容量下不再逐出一次就遍历全表。准备快表的独立 512 块策略维持原实现。

Draw key 完整限定 frame/family/view、primitive Scene/Slot/Generation、可选 LocalItemId 的存在和值以及 collection ordinal，避免多个 primitive 的局部 ordinal 碰撞。Close 销毁常量缓存时同时将实时 `LivePages/PageBytes` 清零，累计统计保留。新增用例覆盖身份区分、64/65/128 项循环扫描和更换、LRU 命中/替换/清理、默认 provider 容量及字节压力的多项逐出。

最终数据与验证边界记录于 `implementation.md`、`audit.md` 和 `docs/MaterialPerformance.md`。用户已授权实施中同步 artifacts，常规方案调整无需再次确认。
