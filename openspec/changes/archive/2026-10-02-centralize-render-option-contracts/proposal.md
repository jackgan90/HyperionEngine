## Why

Pipeline、GBuffer preset 与 Visualizer 的合法值、显示映射和转换分别维护在 Config、Renderer、Editor、DebugUI 和 HLSL 中；未知字符串在部分内部转换中静默落入默认值，Visualizer 的展示位置还承担 shader 数值协议。M09 需要在扩展前建立一个领域拥有的权威映射，同时保持当前配置、自动化和画面语义。

## What Changes

- 新增 CPU-only `Runtime/RasterOptions`，集中定义现有 raster pipeline、GBuffer preset、Visualizer 的类型化稳定身份、配置 token、显示标签和严格映射；Config 不依赖 Renderer。
- Config、Renderer、Editor 和可选 DebugUI 消费同一描述。GUI 只在对应控件实际变化时更新该选项，修改 VSync 等字段保持其余选择。
- Reflection 的旧属性描述增加可选纯值校验回调，Config 提供领域校验；通用反射解码在任何字段赋值前完成校验，编码在文件写入前完成校验，直接字段赋值复用相同规则。
- 从 Visualizer 描述生成 GBuffer debug pixel shader 的命名 defines，通过现有 material shader define / shader cache 输入路径传递，替换 HLSL 数字分支，保持 0–6 含义与布局。
- 保持 string/uint wire 形态、operation/type IDs、版本、默认值和有效范围，拒绝未映射值；主视口继续消费完整已提交设置，3D 预览保持独立的管线/GBuffer/Visualizer/曝光，只共享深度约定，保留既有窗口 VSync 节流策略和 GPU 生命周期。
- 补齐描述重排、固定数值与像素期望、配置往返、正式 discovery/invocation、GUI 等价和可选插件缺失构建的验收。

## Capabilities

### New Capabilities

- `raster-option-contracts`: CPU-only 光栅渲染选项目录、严格边界解析、Visualizer CPU/shader 协议和兼容性验证。

### Modified Capabilities

- `configuration-and-plugins`: 渲染选项在现有配置入口使用权威映射验证，未映射值不得替换有效配置。
- `editor-render-diagnostics`: 共享设置与 named visualizer 采用稳定选项身份，展示重排及无关控件修改不改变选择，保持既有 GUI/Automation 契约。

## Impact

- 新增 `hyperion_raster_options` target；根 CMake、Config、Renderer、Editor、可选 DebugUI 和相关测试声明直接依赖，边界检查禁止该模块依赖 Renderer/RHI/插件/后端。
- 主要消费端为 Config/AppSettings、Renderer/RenderSettings/SceneViewport/ScenePipelineTargets/ScenePipelineFullscreen、Editor 设置/HUD/viewport 服务、DebugUI 控件，以及 `Content/Shaders/Deferred/Debug.hlsl`。
- Reflection/FProperty 与 JsonSerialization 增加通用纯值校验接入，不增加 Reflection 对 RasterOptions 的依赖或 Config 专用分支，也不扩展为任意 setter 异常的回滚机制。
- 复用 `FMaterialShader::Defines`、MaterialPreparation::CompileOptions 与现有 ShaderCacheKey；无需新编译框架、Materials/Shaders 反向依赖、资产迁移或新增 automation operation。
- 本 change 只覆盖 M09；不实现新 pipeline、GBuffer 格式、Visualizer、pass routing、组件传播、draw-plan 重构或 shader 编译策略 M10。
