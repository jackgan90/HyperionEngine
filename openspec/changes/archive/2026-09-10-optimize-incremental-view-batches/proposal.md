## Why

当前 Debug SceneViewer 的静止、小幅运动和大幅运动平均帧时分别为 5.04、12.05、16.68 ms。大幅运动时主视图相邻帧仍保留 99.03% 的可见项，但计划复用率只有 0.5%；成员完全不变的阴影视图也反复收集。需要将视图参数更新、可见成员变化和局部内容变化分开，避免少量变化触发整个视图的准备与分组。

## What Changes

- 为静态发布源保留每视图的可见成员与准备结果，使用准确的当前裁剪结果决定成员变化；成员未变时复用有序存储和局部准备。
- 为内置实例策略引入有界的持久兼容分组、稳定批次身份和成员反向索引，增删仅更新受影响的批次，避免首成员变化引发后续容量分块移动。
- 将共享视图参数与局部项准备分离，并按稳定批次保留提交准备，继续更新当前 GPU 绑定、源项回执与失败结果。
- 减少缓存维护中的重复遍历，保留源失效、容量限制、旧帧不可变和 GPU fence 退役保障。
- 增加可见性变化量、受影响批次和复用诊断；将固定机位转向、首/中/尾单项变化、CSM 独立可见性和真实帧 A/B 纳入验证。

## Capabilities

### New Capabilities

- `incremental-view-preparation`: 静态局部内容下按每视图成员变化增量准备、按稳定兼容组和批次局部更新、保留当前视图参数及有界生命周期。

### Modified Capabilities

无。现有 camera-motion-preparation、实例批处理、材质和资源生命周期契约继续适用。

## Impact

- 主要影响 Runtime/Renderer 的 scene collection、view/material preparation、batch planning、instance cache 和 scene pass preparation；必要的可见性辅助操作由 Math/Renderer 自有接口承担。
- Viewer 诊断、Renderer 回归、性能工具及验证文档同步更新；保持 CLI、目标名、模块边界和默认关闭 Tracy 的行为。
- 使用当前 `97b2244` 的冻结程序作为基线，完成 Debug/Release 功能与性能验证。变更完成后保留待审阅工作区，不执行 Git commit。
- 不引入 GPU-driven culling、实例 GPU 间接寻址或新的第三方依赖。
