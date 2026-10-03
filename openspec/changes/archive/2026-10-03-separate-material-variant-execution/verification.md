# 材质变体身份与执行模式验证

## 范围与基线

- 实施基线：`0f165ce694d28284ed4e1ea43ff29142d32f07dd`，开始时工作区干净。
- 修改前 Debug `material_bindings`、`instance_batching` 2/2 通过，7.34 秒。
- 本项只修改 Renderer 编译/消费契约、现有 instance 测试接线及文档；没有资产格式、反射 ID、automation operation、插件依赖或 shader 源码变更。
- 按用户要求保留活动 change，不归档、不提交、不推送。

## 实现与人工复查

- `FMaterialVariantRequest` 和 `FCompiledMaterialPass` 携带 `EMaterialExecutionMode`；默认 Ordinary，显式 Instanced 控制宏、实例反射、容量、输入校验和可选原生准备。非法模式、同名身份和同一 Usage 多个实例候选在编译前拒绝。
- `GetPass(Usage, Name)` 保留准确名称查询；`GetPass(Usage)` 复用 Ordinary Default 校验；`FindInstancePass` / `GetInstancePass` 和 draw 根据模式选择实例候选。
- 自动生成仍使用 Default/Instance 身份；剩余 `"Instance"` 字符串仅用于生成身份与重名检测。可选失败保留原 builder，普通程序不被污染。
- 编译材质 key 版本为 material-v3，显式包含模式；shader key 包含派生宏。draw key 使用类型化模式。程序/绑定指针缓存依赖不可变编译对象；记录缓存仍比较格式、stride、成员和语义映射；PSO 仍比较真实 shader/layout/state。名称更改不强制拆分相同 GPU 程序。
- `MaterialInterfaceBuilder` 私有拥有反射参数积累、数组形状匹配和 stage binding，保持 schema/mapping 的生成规则与顺序。`MaterialPreparation.cpp` 由 551 行缩至 239 行，新 builder 实现 364 行；本项所有非测试 C++ 文件均不超过 500 行，新增/实改函数均不超过 100 行。未扩展到既有超限的 MaterialConstantCache 或 ShaderCompiler。
- 无高频日志、额外同步点或生命周期调整；可选实例 GPU 准备延迟到 batch 路径，frame/fence 退休规则不变。

## 回归覆盖

新增 `MaterialExecutionTests.cpp`：

- DXIL、SPIR-V、MSL 下 Ordinary `Instance` 与自定义 Instanced `Crowd` 的宏结果、容量、绑定、查询和映射。
- 同名不同模式的 key 分离；同一请求重排的稳定 key；更名保留 shader key、改变变体身份。
- 显式宏不能覆盖执行模式；Instanced `Default` 只能按名/实例模式查询，普通默认查询明确拒绝。
- 空名、重名、未知模式、多个实例候选、缺少实例数组及 shader 强制关闭实例支持的拒绝。
- 自动名称碰撞保留普通变体和诊断，多个普通命名变体仍可编译。

新增 `MaterialExecutionGpuTests.cpp`：

- 自定义 `Crowd` 经显式预编译资源请求进入真实 GPU；cached/uncached 实例记录字节与默认程序一致。
- 四个普通 draw 与一个四实例 draw 像素完全相同；再次准备复用 chunk/GPU 数据，upload 为零。
- 切换至默认实例程序后，旧的自定义实例 draw 仍保持原像素。
- 自定义实例候选的顶点输入不被原生布局支持时，材质仍 Ready；批处理明确回退四个普通 draw，像素与禁用实例能力的结果一致。
- 既有实例套件继续覆盖容量分块、非法 count/extent、成员变化、旧帧、缓存退休、原生常量限制与失败恢复。

## 构建与检查

Debug/Release 均完成以下目标的构建与链接：`instance_batch_tests`、`material_tests`、`material_binding_tests`、`material_gpu_tests`、`shader_tests`、`scene_render_tests`、`deferred_render_tests`、`shadow_render_tests`、`depth_convention_tests`、`hyperion_editor`。

构建采用 Ninja、MSVC 14.50.35717，BUILD_TESTING=ON、Tracy=OFF；保留现有 RenderDoc 选择（Debug ON，Release OFF）。两个配置均有来自未修改测试文件的警告：EngineSemanticTests.cpp 的 C4389 和 ShaderWireLayoutTests.cpp 的 C4324；本项未扩展修复这些测试。

| 检查 | 结果 |
| --- | --- |
| Debug 相关 CTest | 9/9，125.17 秒 |
| Release 相关 CTest | 9/9，31.13 秒 |
| 完整格式与源码路径 | 通过，1109 个源码文件 |
| 模块依赖边界 | 通过，1070 个源码文件、39 个模块 |
| 受影响 C++ 语义命名与声明 | 通过，12 个翻译单元 |
| `git diff --check` | 通过 |
| `openspec validate --all --strict` | 109/109 |

CTest 子集为 `material_contracts`、`material_bindings`、`material_rendering`、`shaders`、`deferred_rendering`、`depth_conventions`、`cascaded_shadow_rendering`、`instance_batching`、`scene_rendering`。两个配置串行运行 GPU 验收；实例套件在结束时检查 GPU validation errors 为零。

证据目录：`out/maintainability/MaterialExecution/`。DebugBuild.log、ReleaseBuild.log 保存各配置构建；DebugTests.log、ReleaseTests.log 保存回归；Naming.log 保存语义命名；OpenSpecValidation.log 保存全量规范校验。

## 验证边界

这是相关回归子集，不代表完整 `hyperion_check`。DXIL/SPIR-V/MSL 验证编译与反射；原生 GPU 执行在 D3D12 上验证。未做 Vulkan/Metal 原生运行、性能基准或 Visual Studio 解决方案重新生成。

## 独立审计（2026-10-03）

使用 `quality-audit` 的无上下文 reviewer 审查当前未提交改动，基线仍为 `0f165ce694d28284ed4e1ea43ff29142d32f07dd`。范围覆盖本项源码、测试、文档、OpenSpec 和直接缓存/绘制调用链；初审结束时两项开发内容的 42 个文件均匹配 `out/maintainability/AuditMaterialImport/InitialSnapshot.json`。

独立审核未发现可确认的本次引入缺陷。主 agent 另行核对模式消费者、错误回退和缓存链，并对迁移的 8 个 builder 核心函数做去空白的函数体对照，均与基线一致。未对本项生产代码作修复性改动。

Reviewer 独立运行 Debug `instance_batching`：**1/1 通过，2.75 秒**，D3D12 debug layer 开启，套件 GPU validation errors 检查通过。日志为 `out/maintainability/AuditMaterialImport/MaterialExecutionReviewerDebug.log`；其中保留已有非比较 sampler 忽略 ComparisonFunc 的 warning 1361，不记作本次引入的问题。审计轮未重复构建或重跑 Release/完整套件，先前验证与本轮独立运行分别记录。
