## Why

材质变体名称目前同时是开放的程序身份和实例化策略：Renderer 在编译、反射绑定、资源准备和绘制处重复判断 `"Instance"`。这使新增或重命名变体可能改变执行行为，并使缓存契约依赖隐含约定；本项是已确认的维护性改造，不宣称修复已复现的运行时故障。

## What Changes

- 在 Renderer 的变体请求与编译结果中显式表达普通/实例化执行模式，名称继续作为开放身份。
- 由模式统一控制 `HYP_ENABLE_INSTANCE`、实例记录绑定、容量和输入校验、实例程序查询及可选 GPU 准备；模式进入编译材质身份。
- 保持自动生成的 `Default`/`Instance` 身份、现有批处理算法、普通回退、GPU 生命周期以及资产和 automation 格式。
- **BREAKING**：直接构造 C++ 变体请求时，名称 `Instance` 不再隐式启用实例化；调用方必须显式选择实例化模式。仓库内生成请求一起迁移，默认请求仍为普通模式。
- 增加任意名称、名称与模式相反、非法模式/歧义实例选择、多目标编译、真实 GPU 输出和缓存回归。

## Capabilities

### New Capabilities

无。

### Modified Capabilities

- `material-system`: 变体身份与执行策略分离，编译身份和选择契约覆盖执行模式。
- `instance-uniform-rendering`: 实例能力由显式模式及反射校验共同决定，批处理和可选回退不解释变体名称。

## Impact

涉及 Runtime/Renderer 的材质准备、接口查询、GPU 准备、draw 与实例记录打包，相关测试和材质文档。将超过文件规模限制的材质准备实现按反射接口绑定与变体编译职责拆分；不扩展为常量缓存或批处理架构重写。不引入新的模块依赖、插件、GUI 功能或 automation operation。完成后保留活动 OpenSpec change，不归档、不提交。
