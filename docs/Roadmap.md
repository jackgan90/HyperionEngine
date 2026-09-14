# 功能范围与扩展边界

本页描述当前代码的能力边界，不是待执行的任务清单。具体设计和已完成变更保存在 [OpenSpec 归档](../openspec/changes/archive)。

## 已实现

| 领域 | 能力与说明 |
| --- | --- |
| 平台与构建 | Windows x64 / MSVC、Visual Studio 与 Ninja、锁定依赖；见 [VisualStudio.md](VisualStudio.md) |
| 运行时 | 独立 Main/Render/RHI/IO 执行域、oneTBB Worker、反射配置和静态逻辑插件 |
| 资产 | 离线 glTF/GLB、原生模型/材质/纹理/场景/天空资产、增量导入和异步加载；见 [NativeAssets.md](NativeAssets.md) |
| 场景 | 层级、相机、方向/环境/点/聚光节点、编辑保存、BVH 视锥剔除和可复用导航 |
| 渲染 | HDR Forward/Deferred、MRT GBuffer、CSM、reversed-Z、实例批处理和增量准备 |
| 光照 | CPU 聚簇局部光、天空背景、SH 漫反射和 GGX 预过滤 IBL |
| 调试与验证 | GUI、截图验收、可选 RenderDoc 和 Tracy；见 [Verification.md](Verification.md) |

## 尚未实现

- Vulkan/Metal 运行时后端与移动平台；现有 SPIR-V/MSL 编译反射不能代替原生后端验证。
- 动画、蒙皮、morph target，以及 glTF 压缩和扩展材质导入；完整格式范围见 [AssetPipeline.md](AssetPipeline.md)。
- 多 GPU 队列调度、瞬态资源别名、MSAA/resolve、原生 ray tracing 和 mesh shader 操作。Compute/UAV 已支持，详见 [Compute pipelines](ComputePipelines.md)。
- 局部光阴影、场景遮蔽的天光、多次反弹 GI、局部反射探针捕获与混合。
- 动态插件 DLL 热重载、AST 反射生成、GC 和任意指针对象图序列化。

新增能力遵循 [Architecture.md](Architecture.md) 与 [SourceLayout.md](SourceLayout.md) 的模块、线程和资源所有权边界。
