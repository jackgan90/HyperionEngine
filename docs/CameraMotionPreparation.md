# 镜头运动时的 CPU 准备开销

镜头变化会使视图整体缓存失效。Renderer 通过共享材质契约、局部准备证明和单项复用，更新视图参数并保留未变的物体与资源状态。

## 实现

- **共享材质契约**：普通求值确认语义依赖和 instance layout 后，发布 `FMaterialSharedBinding`。稳定的 Scene/resource publication 允许保留 item 直接使用这个契约，每个视图按契约分组刷新共享值。契约不持有 Object scope，只弱引用需要比较的资源值。快速路径和普通路径共用刷新表，避免不同共享表身份拆散原本可合批的 items。注册自定义合批策略时保留原有精确参数复用路径。
- **跨阶段的局部准备证明**：`FLocalMaterialPreparation` 记录有序 primitive、ordinal、LocalItemId、准备对象和局部参数身份。只有静态发布、完整依赖、view/pass/target 环境及内置策略满足约束时，才能跳过重复的 batch membership 和 RHI 准入检查。共享 overlay 在批内仍必须一致，实例 chunk 仍必须存活。
- **可见集合变化后的单项复用**：item 另外持有不延长 source/value 生命周期的 `FLocalMaterialItem`。可见集合改变时重新分组，但未变 item 的输入、candidate 结构和已打包 record 可继续复用。record 最多记录 8 个弱视图证明；超过数量按完整值比较。输入、candidate、record 和 block 的无序缓存使用完整键相等判定，LRU 和容量约束保持显式，hash 仅选择 bucket。
- **不可变包更新**：保留 batch 的结构身份和实际 draw 到源项的映射。共享更新只刷新实际 draw 的常量绑定，重新发布不可变 packet 容器；较早图和 GPU 数据继续由原有 owner/fence 保留。失败仍回退到逐项验证、整组失败传播和修复路径。每个视图仍发布当前 frame/family/view 的回执。
- **可见性往返保留**：每个视图最多保留 2048 个近期不可见 item；Scene/resource publication 改变时失效。稠密 primitive slot 使用有界索引，稀疏编号或大 ordinal 使用排序查找。不能缓存的自定义 collection 不保留重复旧副本。`Reused` 使用字节标志，避免高频 `vector<bool>` 操作；对象仍有独立稳定地址，独立快照仍深拷贝 item 状态。

局部证明上限为 4096 items/视图，共享契约和更新表上限为 128；超出容量继续走通用路径。没有关闭 Debug `/Od /RTC1`、STL 调试检查、D3D12 validation、阴影更新或 GUI。剔除算法、shader ABI 和阴影质量均沿用现有实现。

共享契约另有最多 128 项的弱历史，用完整 program/pass、参数索引、依赖和资源值比较合并不同帧建立的等价契约。默认 provider 的输出共享已验证的单个语义副本，避免重复复制/验证；它不引用原始 scope 的整张表，因此不会保留未使用的纹理或 buffer。自定义 provider 仍执行自己的回调和结果验证。provider 清理直接遍历现有 recency 索引，只有实际回收才查找 bucket。

相关实现：[材质契约](../Source/Runtime/Renderer/Private/MaterialSharedBinding.cpp)、[视图准备](../Source/Runtime/Renderer/Private/SessionViewCache.cpp)、[局部证明](../Source/Runtime/Renderer/Private/LocalMaterialPreparation.cpp)、[合批复用](../Source/Runtime/Renderer/Private/BatchPlanCache.cpp)、[RHI packet 更新](../Source/Runtime/Renderer/Private/ScenePassRefresh.cpp)、[可见项保留](../Source/Runtime/Renderer/Private/SceneCollectionReuse.cpp)。


性能采集入口与当前 Editor 工作负载约束见 [渲染诊断](RenderDiagnostics.md)。
