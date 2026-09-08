## Why

材质系统重构后，Debug 镜头运动基线从约 11.0 ms 增至 20.9 ms；实际 GPU 常量通常仅新增一个 80 B View 块，但 CPU 对每个 draw 重复复制输入、刷新完整解析容器和查询未变的常量块。通用路径还存在 transient 参数触发完整求值、长期 scope 常量历史无容量限制及重复原生绑定的问题，需要在引擎层解决并以 profiling 验证。

## What Changes

- 在 Materials / Renderer 通用路径共享不可变输入和解析数据，按有效依赖增量求值，避免未变参数的深复制、名称查找与无效发布。
- 将 Frame、View、Object、Pass、Draw 和混合依赖纳入统一更新机制；独立常量块复用已准备的 slice，资源身份不受无关数值刷新影响。
- 给参数/provider/常量历史缓存提供明确的容量、退休与清理策略，淘汰缓存引用时保留旧 CPU 快照及 GPU 在途数据。
- 在 D3D12 通用命令录制路径复用相同绑定状态，继续执行完整绑定验证及 list/reset/root/heap 失效处理。
- 增加通用测试和 profiling 计数，保存同场景 Debug / Release 优化前后数据，以及多频率和缓存压力证据。
- 以实现实验更新 design/tasks/implementation；最终 artifact 内容描述实际交付行为，保留调整原因和测量边界。

## Capabilities

### New Capabilities

无。

### Modified Capabilities

- `material-parameter-binding`: 按依赖共享不可变 CPU 结果、独立块增量绑定、有预算的缓存历史及可观察的更新量。
- `material-system`: 无变化写入不复制或重新发布有效材质状态；保留事务性验证和快照不可变性。
- `graphics-resource-binding`: 命令列表内去除重复绑定，确保所有原生状态边界后的重新绑定正确。
- `runtime-performance-profiling`: 补充通用材质工作量和缓存存量观测，要求可复现的同口径性能对照。

## Impact

涉及 Runtime/Materials、Runtime/Renderer、必要的 Runtime/RHI / Backends/D3D12、相应 Tests 与文档。CPU Materials/Scene 不引入 Renderer/RHI 依赖；不增加第三方依赖。不改变 shader ABI、序列化、CLI、plugin ID 或已有 target。

所有优化必须在引擎通用路径生效，不以 Viewer、SceneViewer、特定 PBR 名称或 194 draws 为条件。应用只作为现有端到端测量入口；任何插件接入调整只能消费通用引擎接口，不能保存私有优化副本。不在本次引入新渲染功能、改变 Debug 优化级别或关闭验证。基线证据为 `out/Profiling/MaterialInvestigation`，源码基线 `ef7a1ef`。
