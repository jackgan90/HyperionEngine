## Why

当前材质路径将 `FModelMaterial`、五张纹理、512 字节模型常量和强制深度目标贯穿 Renderer 与 D3D12，限制了独立表面定义、自定义 Shader 参数及跨对象资源共享。需要以独立材质定义/实例、统一反射和通用绑定替换这套模型专用约定，使 triangle、model-viewer、scene-viewer 使用同一套可扩展材质系统。

## What Changes

- 新增独立 CPU `Runtime/Materials` 模块：不可变定义、带 revision 的实例快照、按用途组织的 pass、类型化参数及拥有源数据的资源值、显式 semantic 映射和可查询属性。
- 按 Global/Frame/Scene/View/Pass/Material/Object/Draw 作用域解析参数；支持标准布局的 GPU 常量块共享，以及任意命名和自定义布局的反射打包。
- 完善 DXIL/SPIR-V/MSL 来源元数据、参数成员布局、Shader 接口合并和缓存一致性；支持显式编译 defines，以及 Worker 反射接口到 Main CPU schema 的异步准备/接收。
- **BREAKING**：替换 `FPipelineDesc`、`FDrawPacket` 和 `FRenderMaterialDesc` 中的模型专用布局，提供显式图形状态、绑定布局、buffer slice、资源数组和 sampler 绑定；迁移仓库内直接 RHI 调用方及替身。
- 将材质/纹理/PSO/binding set 共享身份从模型几何批次中解耦，区分静态资源就绪和逐帧绘制准备，保留现有 Worker 准备、Render 快照、RHI 0 管理及 fence 退休契约。
- **BREAKING**：通用 primitive 和 Scene 实例支持独立材质引用、section 选择和类型化覆盖；现有三项 PBR 覆盖保留兼容入口。glTF 的 `FModelMaterial` 与现有序列化保持稳定，由 Renderer 适配为内置 PBR 材质。
- 迁移三个场景插件，适配 DebugUI 的底层接口，保持现有插件 ID、CLI、配置键、构建目标和画面/交互行为。
- 首版执行能力为 VS/PS、constant buffer、只读 buffer view、2D sampled texture/固定长度资源数组、独立 sampler、单颜色目标和可选 depth/stencil。完整阴影渲染、MSAA/resolve、MRT、任意离屏图、UAV/compute、其他原生后端、材质图编辑器和热重载不在本次开发范围。对这些能力进行描述/查询时必须明确返回未启用或不兼容，不能声称已实现。

## Capabilities

### New Capabilities

- `material-system`: 与模型无关的定义/实例/pass、有效状态查询、版本和模块边界。
- `material-parameter-binding`: 类型化参数、保留及扩展 semantic、作用域上下文、共享 uniform buffer 与自定义打包。
- `graphics-resource-binding`: 通用 RHI 绑定、显式图形状态、布局/PSO 缓存、能力和生命周期验证。

### Modified Capabilities

- `shader-pipeline`: 所有产物具备统一可用反射、完整成员布局和编译选项缓存契约。
- `shared-render-resources`: 材质与几何独立共享、版本化替换、包含新绑定对象的退休契约。
- `render-primitives`: primitive 持有通用材质快照及显式坐标/保守 bounds 契约。
- `scene-management`: CPU Scene 的材质选择、覆盖及桥接同步。
- `static-model-rendering`: glTF 默认 PBR 适配与通用材质替换，不再由 Model 强制管线状态。
- `render-graph-plugins`: 基于有效材质 pass 组织绘制，保持全场景顺序，增加有限 depth/stencil 状态验证。

## Impact

影响 Materials（新增）、Shaders、RHI、Renderer、Scene、D3D12、Triangle、ModelViewer、SceneViewer、DebugUI、对应测试和模块/材质开发文档。第三方 API 继续封装在 Shaders 私有 adapter 和 D3D12 私有实现中，不新增第三方依赖。

本次交付阶段只生成和审计计划，不执行实现、不归档、不提交。实现前必须完成独立 plan review。发现为完成材质功能必须引入上述范围之外的大量开发时，停止该依赖方向的工作，列出证据、影响及有界替代方案，由用户决定是否扩大范围。
