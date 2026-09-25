# Renderer 增量更新优化

Renderer 按实际变化更新共享材质参数、可见项和实例块，保留旧帧所需的不可变资源。

## 实现

- 稳定 local 材质结果与不可变 `FMaterialSharedParameters` 分开保留。兼容 engine 参数按组刷新；batch、instance packing 和实际 draw 都读取当前有效 shared/local 值。混合依赖、default/缺失输入、覆盖、资源变化、Draw 依赖与自定义策略保留正确 fallback。
- 静态 primitive 缓存完整就绪的 collection 和材质/几何元数据；`FRenderItemList` 在裁剪、排序、重用时转移 item 拥有句柄，显式复制则深拷贝。旧 snapshot 不被后续 view 消耗。自定义 collection 继续使用现有 vector 发射接口，透明排序和回执保持原语义。
- 实例记录按完整反射布局、参数映射与有效值验证。不可变块按布局及有序记录身份复用，可以跨兼容 view/pass 共享 CPU 字节与 GPU slice。Object 变化只重新打包对应记录、拼装 Object 块；未变 Surface 块继续复用。实例 draw 跳过随后会被替换的单实例常量准备。
- Batch signature 分离不可变结构和共享数值列表。结构池只在完整兼容性比较成功后合并身份，哈希仅选择候选；共享列表要求覆盖全部非实例数值。自定义策略仍获得完整当前 signature。
- View 历史最多 64 份、120 个未使用 frame；静态 primitive collection 最多 64 项；记录、块、chunk、layout 与 GPU block 历史均有预算及退休路径。常量发布仍不可变，GPU fence 保活和 native 验证没有削弱。

接口与预算详见 [Materials.md](Materials.md) 和 [InstanceBatching.md](InstanceBatching.md)。`FRenderSceneSnapshot::Items` 现在是支持范围/索引访问的 `FRenderItemList`，不承诺连续 `FRenderItem` 存储；`FInstanceConstantBlock::Bytes` 为共享不可变字节，`FRenderBatchSignature` 引用结构与值列表。这些 C++ 接口随仓库调用方一并更新；未改变资产、shader CBV ABI、CLI 既有参数、插件 ID 或构建目标。


性能采集入口与当前 Editor 工作负载约束见 [渲染诊断](RenderDiagnostics.md)。
