## Context

核查基线为 `4da30568bb09e4f8ec60be596df7f822f77724b1`。`RenderSettings.cpp`、`AppSettingsRecord.cpp`、Editor 和可选 DebugUI 独立解释 forward/deferred、compact/high；`MakePipelineSettings` 对未知值回退。`EditorHud.cpp` 的数组位置、多个 `DebugMode > 6` 校验及 `Deferred/Debug.hlsl` 的数值分支共同定义 Visualizer。现有两种合法 pipeline/preset 没有证实被 GUI 错改；本变更消除可扩展性风险及直接转换的未知值回退。

Config 仅依赖 Reflection/Tasks，Renderer 已依赖 Config，因此共享定义不能放在 Renderer 后让 Config 反向依赖。公开配置/automation 值仍需保持 string/uint；`render.settings.get/set` 当前 operation version 为 2，save 为 1。GUI 与 agent 已共用 `IRenderSettings` / `ISceneViewport`，不需要新增领域服务或 transport 分支。

Shader 已有精确接入点：`FMaterialShader::Defines`（MaterialState.h），`MaterialPreparation.cpp::CompileOptions` 复制这些 defines，`ShaderCompiler.cpp::ShaderCacheKey` 纳入它们。`ScenePipelineFullscreen.cpp::LightingMaterial` 已通过 `MakeFullscreenMaterial()->GetDescription()` 构造局部材质变体。无需扩展 uniform 布局系统或编译器协议。

## Goals / Non-Goals

**Goals:**

- 一个 CPU 领域目录定义现有稳定身份、token、label 与 Visualizer shader 常量，所有消费端严格解析。
- 展示顺序不改变选项含义，无关设置操作保持其他字段，现有 wire/schema/revision、主视口与预览的设置边界及渲染效果保持。
- 生产 shader 和直接编译测试使用同一 define 适配，并用固定数值/像素期望独立验证语义。
- 新模块可在可选插件未编译或服务缺失时使用，避免 Config/Scene/Materials 被迫依赖图形执行层。

**Non-Goals:**

- 新增 pipeline、GBuffer 格式、Visualizer、开放注册表、通用代码生成器或 GUI 自动生成框架。
- 修改 GBufferDebugV1 的 uniform 布局、材质资产格式、默认画质、曝光/其他选项范围、operation IDs、版本或任务生命周期。
- 将主视口的全部 render settings 传播到 3D asset preview，或为通用反射引入任意 setter 异常的事务回滚。
- M03 pass 参与策略/route、M07 组件传播、M08 draw plan、M10 编译目标策略；这些独立领域行为不属于本 change 的功能范围。

## Decisions

### 1. 领域所有者为 Runtime/RasterOptions

新增 `Source/Runtime/RasterOptions` / `hyperion_raster_options`，公共入口 `Public/Hyperion/RasterOptions/RasterOptions.h`，实现 `Private/RasterOptions.cpp`。它仅使用标准库，不依赖 Reflection、Config、Materials、Shaders、Renderer、RHI、GUI、Plugins 或 backend。名称限定为本 change 涉及的光栅渲染选项，不建设含所有图形数据的 RenderTypes 集合。

把现有 `ESceneRenderPipeline` 定义移入此模块，保留 namespace、类型名、underlying uint8 和当前显式值 `Deferred=0, Forward=1`，原 `SceneRenderPipeline.h` 继续包含它，保证现有 include 用法成立。新增具体 `EGBufferPreset`、`EGBufferVisualizer`，不改变 `FGBufferLayout` 的完整格式能力：compact/high 只是用户 preset，RHI 设备能力验证仍归 Renderer。

用小的不可变描述数组关联 typed ID、稳定 token、label；Visualizer 还关联 shader define 名称。描述使用枚举项，避免另复制数字列表。显式 lookup/parse/serialize API 遇到未知 ID、token 或数值抛出受控参数错误，不做默认回退。默认项与范围从该目录取值。查找是极小常量集合的操作，在设置/视图构造边界进行，不引入每 primitive 的查询或动态注册。

展示位置仅是该次描述 span 中的位置，提供按稳定 ID 查位置及按位置取 ID 的纯值帮助函数。测试用同一帮助函数传入重新排列的描述副本；不为测试添加新 production mode。目录检查保证 ID/token/define 名称唯一、完整，避免新增 enum 却遗漏描述。

选择此模块而不是放入 Core，是因为规则属于光栅选项领域；不放入 Config 是为了让 shader/Renderer 的领域身份不归应用配置所有；不在 Reflection 定义领域枚举，也不增加全局 registry。

### 2. 保持边界 DTO 和严格解析

`FAppSettings.RenderPipeline/GBufferLayout`、`FRenderSettings.Pipeline/GBuffer` 保持 string；GBufferDebug 保持现有 signed int，DebugMode/viewport Visualizer 保持现有 uint/optional uint wire。负 signed 值在转换为 unsigned 前拒绝。默认仍 deferred、compact、Lit=0。

Config 的旧 `SettingsType` / `LoadSettings` / `SaveSettings` 与新 `RecordType<FAppSettings>` 都消费同一目录，不能只改 AppSettingsRecord 而遗漏旧反射文件入口。旧入口的具体接入是在 Reflection 模块的 `FProperty` 末尾增加默认空的可选 `std::function<void(const FValue&)> Validate` 回调，保持已有 aggregate 初始化和未设置回调的属性行为。回调只校验输入值并报告错误，不接收或修改目标对象。Config 为 pipeline、GBuffer preset 和 visualizer 属性提供消费 RasterOptions 的纯值校验函数，旧 `SettingsType` 的回调和对应直接 `Property.Set` 均复用这些函数，后者必须先校验再赋值。Reflection 只调用通用回调，不依赖 RasterOptions，也不识别 Config 字段或其他领域选项。

`JsonSerialization.cpp::ReadValue` 在已有类型/数值范围检查和 `FValue` 转换之后、返回值之前调用可选回调。因此 `DecodeReflected` 必须在收集整个 `Pending` 列表时完成所有字段的纯值校验，随后才调用任何 `Property.Set`；直接 `DecodeReflected` / `LoadReflected` 即使输入含先出现的合法 `title` 修改和后出现的非法 token，也保持目标对象的每个字段不变。`EncodeReflected` 已通过 `ReadValue` 校验 getter 输出，继续由同一回调拒绝非法值；`SaveReflected` 保持先成功完成 Encode、再创建/写入临时文件并替换目标的顺序。直接 `SaveReflected` 和 `SaveSettings` 因领域校验拒绝时，既有文件内容不变。此保证针对解析及纯值校验失败，不承诺已进入赋值阶段后任意 setter 异常的完整回滚。

`LoadSettings` / `SaveSettings` 包装入口及 `RecordType<FAppSettings>` 继续遵守各自已有的完整对象校验与保存流程，领域规则来自同一目录。其他字段的既有范围不变，尤其不要统一 Config 与 live exposure 本来不同的范围。验收分别覆盖旧通用反射直接入口、包装入口、直接属性赋值及 record 路径，不能仅以返回新对象的 `LoadSettings` 证明已有对象不会部分赋值。

`ValidateRenderSettings`、`MakePipelineSettings`、`FSceneRenderPipeline::Configure`、`ValidateViewportOptions` 使用共享解析/合法值检查；直接调用 MakePipelineSettings 也拒绝非法选项。内部分支使用 typed pipeline/preset/visualizer，GPU 写入经具名转换而不是 GUI 下标。FScenePipelineSettings 的公开 DebugMode 兼容入口可保持 uint，但在 Configure/编码边界解析成 typed ID。preset 转为 `FGBufferLayout` 的具体 native-independent 格式组仍位于 Renderer。

现有反射 record IDs、versions、字段顺序、required/optional、wire kind、数字范围与默认值均保持；Inspector 标签/描述需要选项文本时从目录组合，不能通过改为 enum-valued record 成员改变 string wire。不会新增 JSON schema enum 限制或 operation。schema 兼容验收比较正式生成的字段形状/约束和固定有效值，不以 C++ 类型编译代替。

### 3. Visualizer 数值与 HLSL 使用现有 define 机制

固定协议如下；显示顺序可重排，数值不随之改变：

| typed identity | wire value / current shader code | 原标签 | shader define |
|---|---:|---|---|
| Lit | 0 | Lit | HYP_GBUFFER_VIEW_LIT |
| BaseColor | 1 | Base color | HYP_GBUFFER_VIEW_BASE_COLOR |
| ShadingNormal | 2 | Shading normal | HYP_GBUFFER_VIEW_SHADING_NORMAL |
| MaterialChannels | 3 | Metallic / Roughness / AO | HYP_GBUFFER_VIEW_MATERIAL_CHANNELS |
| Emissive | 4 | Emissive | HYP_GBUFFER_VIEW_EMISSIVE |
| SceneDepth | 5 | Scene depth | HYP_GBUFFER_VIEW_SCENE_DEPTH |
| GeometryNormal | 6 | Geometry normal | HYP_GBUFFER_VIEW_GEOMETRY_NORMAL |

稳定 typed enum 值是现有 wire 数值；`ToVisualizerWireValue` 与 `ToVisualizerShaderCode` 分别表示外部值与 GPU 编码边界，当前显式采用相同稳定协议值，不从 label 或展示位置计算。Shader define 名称属于 descriptor，define 数值由 shader-code 转换派生，不在 HLSL 重复维护 0–6 映射。

具体生产适配为 `MakeGBufferVisualizerShaderDefines()`：在现有 Renderer `ShaderParameters/DeferredLightingParameters.h` 声明，私有小实现文件实现，返回现有 `std::vector<FMaterialShaderDefine>`。它遍历 RasterOptions 描述，使用固定 define name 和十进制 shader code。生成顺序按稳定身份规范化，排除 label/GUI 顺序，以保证只改呈现不会产生 cache miss。

`ScenePipelineFullscreen.cpp` 创建静态 GBuffer debug material 时复用既有 description clone 模式，为 `Description.Passes.front().Pixel.Defines` 填入此适配结果；不要为所有材质或编译器全局注入这些定义。`Debug.hlsl` 使用具名 defines 比较 Mode，必要定义缺失时 `#error`，没有回退数字。Lit 仍由 pipeline 选择正常 lighting/output，BaseColor 仍为默认 debug 颜色，其余分支保持现有数学、有效像素判断、sRGB target 和深度解释。

`Shaders/DeferredShaderTests.cpp` 直接编译 Debug.hlsl 时也调用该生产适配，将 Name/Value 机械复制到 `FShaderCompileOptions::Defines`。测试不可另造同名数值表来代替生产适配；独立固定预期只用于断言，而不是给被测 shader 提供实际映射。缺定义编译失败、全部目标 DXIL/SPIR-V/MSL 编译、define 改变导致 cache key 改变、原 define 恢复后命中均需覆盖；SPIR-V/MSL 只报告编译/反射，不报告未实现 backend 的 GPU 执行。

此路径不修改 Materials/Shaders 依赖或 FShaderCompileOptions；也不增加 generated include。候选虚拟 include 会要求扩大当前 uniform contract 或 material 描述能力，对七个命名常量没有额外收益，因此本次采用既有 define 输入。

### 4. GUI 与共享服务继续分工

EditorRenderSettingsGui 的 combo 由共享描述生成 label 并找当前 ID；只有对应 combo 返回 changed 才改 Candidate.Pipeline/GBuffer。VSync/其他 checkbox 不重写选项。EditorHud 的 selector/HUD label 通过 stable visualizer ID 查表，禁止 `Visualizers[DebugMode]`。EditorViewportService/EditorInspector 的 pipeline 准入改为 typed 查询，仍委托原 SetRenderSettings/SetViewportOptions，不重复校验、history 或 persistence。

保留 Forward 中已有 latent Deferred visualizer：切到 Forward 后显示 Lit/禁用选择器，但保留 settings.DebugMode；无关 HUD 设置仍可修改；新请求另一个非 Lit visualizer 按当前共享服务规则返回 unavailable。切回 Deferred 后恢复原选择。

可选 DebugUIPlugin.cpp 也消费同一目录，不再用两个 checkbox 隐含二元选项映射；使用现有 FGui combo 与目录 label，gbuffer debug 文案同源。它是已有 FAppSettings UI 的消费方，不增加新业务服务或 automation 分支。现有 field IDs 和其他控制行为不变。

FEditorPlugin::SetRenderSettings 的 stale revision、contact-shadows 缺失拒绝、设备布局验证及 commit 顺序不变。主视口由 `EditorFrame` 捕获完整的已提交 `Rendering` 为 owned `FrameSettings`，再经 `MakePipelineSettings` 构造管线参数。

3D asset preview 保持现有独立策略：`AssetPreview` 接收已提交的 `EDepthConvention`，构造默认 `FScenePipelineSettings`，保留 Deferred / Compact / Lit，并使用每个 `Entry->Exposure`。仅深度约定共享主视口已提交设置；`EditorAssetWindow` 的 VSync 仍取自 `!bMainDrawable && Options.ExerciseAssets.empty() && Options.Benchmark.empty()` 的现有窗口节流策略，不取 `Rendering.bVsync`，也不新增 VSync 设置广播。主视口的 pipeline、preset、visualizer、exposure 和 VSync 修改不得覆盖这些预览值或窗口策略。现有、新开和恢复的预览遵守相同边界，不新增全量 FrameSettings 传播。两类视图均保留 owned frame、资源 generation/fence 退休及复用边界。

### 5. 构建与插件缺失边界

| 文件/target | 最小变化 |
|---|---|
| 根 CMakeLists.txt | 在 Config 之前无条件 add_subdirectory(RasterOptions) |
| Runtime/RasterOptions/CMakeLists.txt | 新静态 CPU-only target，使用 hyp_module，无新增运行时依赖 |
| Runtime/Config/CMakeLists.txt | 公共头默认值消费目录时 PUBLIC 依赖 raster_options |
| Runtime/Reflection/FProperty 与 JsonSerialization | 可选纯值校验回调接入通用读写预检，不新增领域依赖或分支 |
| Runtime/Renderer/CMakeLists.txt | PUBLIC 依赖 raster_options（公开 pipeline enum），编入小 define 适配实现 |
| Plugins/Editor/CMakeLists.txt | 对直接使用目录的私有实现声明 PRIVATE 依赖 |
| Plugins/DebugUI/CMakeLists.txt | PRIVATE 直接依赖；仍完全置于已有 HYP_ENABLE_DEBUG_UI 条件内 |
| Source/Tests/CMakeLists.txt | CPU 目录测试及消费方显式测试依赖；shader_tests 已链接 Renderer，仍核对新增直接 include 依赖 |
| tools/CheckBoundaries.py | 将 RasterOptions 纳入 CPU 依赖保护，显式限制其无引擎依赖；不放宽 Materials 依赖白名单 |
| docs/SourceLayout.md / docs/Materials.md / docs/Editor.md | 记录 owner、shader define 接入、稳定选项/呈现分离，避免写入本地验收次数 |

正常 Debug/Release 构建后，使用独立构建目录配置 `BUILD_TESTING=OFF`、`HYP_ENABLE_DEBUG_UI=OFF`、`HYP_ENABLE_TRIANGLE=OFF`、`HYP_ENABLE_RENDERDOC=OFF`，编译链接 Editor、automation CLI、asset tool；不改用户现有 build cache。当前仓库没有通用“禁用 Renderer 构建”开关，不虚构该配置；单独构建 RasterOptions/Config 的依赖和边界证明其 CPU 性质。另跑 runtime kernel-only、graphics/gui/editor/contact-shadows 显式禁用及 automation 缺少 IRenderSettings provider 的现有路径，确保目录存在不会激活插件或让 unavailable operation 伪报成功。插件 Start/Update/Quiesce/Stop 不变。

### 6. 可判定的验收

1. CPU：固定已知 token↔enum↔wire 对照；全部描述重排后映射/选择不变；未知 token、无效 enum、negative app debug、7/max uint 拒绝；同一输入的 canonical shader defines 不受 label/order 变化影响。
2. 边界：两类 AppSettings 反射文件路径、FRenderSettings JSON/wire 完整往返；直接 `DecodeReflected` 和 `LoadReflected` 分别输入合法的先序 `title` 修改与非法 pipeline/GBuffer token，逐字段证明整个已有对象不变；直接 `EncodeReflected` 拒绝非法选项，直接 `SaveReflected` 和 `SaveSettings` 拒绝时既有文件逐字节不变。覆盖直接 setter、record 路径及 visualizer 校验；通用回调测试证明所有校验先于任何 setter，未设置回调的既有属性行为不变。直接 MakePipelineSettings 和 Configure 失败不改状态；所有合法 pipeline/preset 组合修改 VSync 后其他值逐字段相等。
3. Shader：通过生产 define 适配编译三目标，缺 define 必须失败；保持 GBufferDebugV1 反射；define 影响 cache，presentation 不影响 cache。使用独立临时 compile options 的数值扰动验证缓存，不提供 production 覆盖开关。
4. GPU：扩展 deferred_rendering fixture，构造有已知 base RGB、metallic/roughness/AO、emissive、深度及不同 shading/geometry normal 的不对称样本；固定 raw wire 1–6 逐一断言独立 CPU 数学预期，包含输出 sRGB 转换和格式量化容差。Lit=0 对照正常 lighting。覆盖 compact/high 和 Standard/Reversed Z；正常与重排选择到同一 ID 的画面一致，不能仅比较六张图互不相同。
5. 正式协议：api.search/api.describe/types.describe 对 render.settings.get/set/save、view.get/set 及 hyperion.render.settings、hyperion.viewport.options 检查既有 IDs/version/shape；invocation 覆盖往返、stale revision、未知 token/debugMode 拒绝且状态不变、Forward latent visualizer、save/reopen、无 provider unavailable。保持 GUI/agent 同源服务，无新 adapter 需要延期登记。
6. GUI/集成：扩展 EditorRenderControls 的语义输入验收，实际选择两种 pipeline/preset/七个 visualizer，实际切 VSync 后检查候选未污染。主视口验证完整已提交设置；现有、新开和恢复的 3D preview 验证共享深度约定继续生效，同时主视口 pipeline/preset/visualizer/exposure 的修改不改变预览 Deferred / Compact / Lit 及每个 Entry 的曝光，主视口 VSync 修改不改变资源窗口的既有节流策略。保留 old-frame/fence 生命周期、无 scene dirty/history 变化和零 D3D12 validation errors。
7. 现有入口：`render_controls`、`configuration_plugins`、`shaders`、`automation_scene`、`editor_render_controls`、`editor_render_acceptance`、`deferred_rendering`、相关 asset preview、`plugin_runtime`、`plugin_applications`，加新 CPU 目录测试。必要 CMake absence build、style/naming、CheckBoundaries、OpenSpec validation 均在实施后执行。只增加静态目录查找及材质一次性 defines，不新增每帧日志/全量缓存失效；重复稳定帧保留资源与材质复用。

## Risks / Trade-offs

- [改变 record 成员类型会改变 wire/schema] → DTO 保持 string/uint，typed 解析在边界执行，验证真实生成 schema 与 invocation。
- [旧 Config 反射路径发生部分赋值或绕过保存校验] → FProperty 纯值回调进入通用 ReadValue，解码收集完全部已校验值后才赋值，编码先校验再触及文件；直接 setter 复用规则，覆盖 Decode/Load/Encode/SaveReflected 及包装/record 入口。
- [生成 defines 顺序跟随 GUI 顺序] → 以稳定 ID 规范化生成并排除 label，重排测试检查 exact define 集和 cache identity。
- [CPU 与 HLSL 同错仍能 round-trip] → 固定 raw 数值和独立像素数学预期；缺 define 明确失败，测试不生成第二套实际输入映射。
- [新模块引入 Config→Renderer 或可选插件链接依赖] → zero-engine-dependency module、直接依赖与负向边界检查、独立 no-optional-plugins build。
- [未知值拒绝使以前直接调用默认回退的代码失败] → 这是明确修正的边界语义；所有现有合法值、保存文件、公开版本不变，不伪造兼容默认值。
- [把主视口设置错误扩散到预览] → preview 只共享深度约定，验收独立 Deferred / Compact / Lit、每 Entry 曝光和既有窗口 VSync 节流策略。
- [live settings 影响缓存/旧 GPU 资源] → 保持主视口快照、preview 既有参数构造和现有 resource generation；不改变 ShaderCacheKey 算法或 target signatures。

## Migration Plan

先加入 CPU 目录和固定协议测试，再接入通用反射纯值预检并迁移 Config/Renderer 与生成 defines，然后迁移 UI、补自动化/GPU及缺失构建证据。保持主视口与 preview 各自原有设置和资源所有权。每步保持已有合法配置可读，不进行资产/用户配置批量改写。源码与 HLSL define 消费在同一可构建提交中完成；新 shader key 自然隔离旧编译产物。回退整个 change 即恢复旧实现，原文件和 wire 无迁移负担；不需要清空用户全局缓存。

## Open Questions

无阻塞架构问题。实际 C++ helper 名称可按仓库风格微调，但领域目录归属、define 生产适配、wire 保持、缺失构建和独立像素验收不可省略；若实施发现需扩大全局 shader 描述或改变 schema，应回到主 agent 审核设计再继续。
