## Context

基线 `0f165ce` 的 `FMaterialVariantRequest` 仅有 Usage、Name、Defines。`MaterialPreparation.cpp` 用 Name 决定宏、反射转换、容量和 SV_InstanceID 校验；`MaterialInterface.cpp`、`MaterialResourceProgress.cpp`、`MaterialDraw.cpp` 与两条实例打包路径也解释 `Instance`。编译准备已支持多个普通命名变体，但 session 的普通绘制只选择 `Default`，每个 Usage 只有一个批处理候选。本项保持该选择范围。

修改前 Debug `material_bindings`、`instance_batching` 2/2 通过。资产格式不存储本次新增的 Renderer 编译策略；无需反射迁移或新 automation adapter。

## Goals / Non-Goals

**Goals:** 开放的名称与有限执行模式分离；编译到绘制的消费者使用同一模式事实；缓存覆盖模式；保持既有自动生成、失败回退、像素、覆盖和退休行为。

**Non-Goals:** 不新增运行时变体选择器，不修改 batching 算法、shader ABI、材质持久化、RHI 生命周期、插件组成或用户操作。不重构无关的常量缓存。

## Decisions

1. Renderer 拥有 `EMaterialExecutionMode { Ordinary, Instanced }`。请求和编译 pass 都携带 `ExecutionMode`，默认 Ordinary；请求字段放在既有字段之后，普通聚合调用继续可用。非法枚举在编译准入拒绝。单纯给名称加常量仍隐含策略，因此不采用。
2. `(Usage, Name)` 仍是唯一开放身份，按名 `GetPass(Usage, Name)` 保留。每个 Usage 最多一个 Instanced 候选，重复候选在准入明确拒绝，避免遍历顺序决定执行；普通命名变体仍可有多个。`FindInstancePass` 依据模式，新增必须存在的实例查询；绘制选择接口按模式选择 `Default` 普通 pass 或唯一实例 pass，并校验普通模式。无显式名称的 `GetPass(Usage)` 也复用普通选择，保证所有既有 session 消费方执行同一校验。名称 `Instance` 可以是 Ordinary，任意名称可以是 Instanced。
3. 既有自动策略仍从 `Default` 的 Ordinary 请求尝试生成名为 `Instance` 的 Instanced 请求，继承 defines。`Default` 是现有普通选择身份，并非模式推断。已有显式 Instanced 候选时不自动生成；同名 Ordinary 占用 `Instance` 时保留其身份、记录可选候选冲突，不覆盖或误用。可选编译采用 builder 副本，失败只留下诊断。
4. 模式控制宏覆盖、记录反射转换、容量和输入校验以及可选 GPU 准备。编译材质 key 显式加入模式 token，生成候选也使用同一 key 追加逻辑。shader key 自然包含由模式产生的宏；常量/记录缓存仍依据不可变程序、binding 和实际布局；draw 缓存用类型化模式取代布尔位置，GPU PSO 继续按实际 shader/layout/state 比较，不增加无关的名称分裂。
5. 将材质反射接口 builder、数组形状匹配与 stage binding 放进 Renderer 私有 `MaterialInterfaceBuilder.h/.cpp`；变体编译保留在 `MaterialPreparation.cpp`。拆分职责形成独立接口，两个实现均低于 500 行，新增/实改函数遵循 100 行原则。

## Risks / Trade-offs

- [外部 C++ 调用依赖 `Instance` 名称] → 明确记录源码迁移：调用方显式填写 Instanced；仓库内生成请求一并迁移。资产/协议名称与版本不变。
- [可选失败污染普通接口或静态准备] → 保留隔离 trial builder 和延迟实例 GPU 准备，覆盖 shader 不支持与原生限制回退。
- [选择歧义或自动名称碰撞] → 明确准入/诊断，测试反向名称和重复候选，不按容器顺序猜测。
- [缓存或生命周期回归] → 跨 DXIL/SPIR-V/MSL 编译、真实 GPU 普通/批量像素等价、重复打包复用、旧帧保活和容量边界测试；不以性能数字替代正确性验证。

## Migration Plan

先落实契约和编译/查询，再迁移消费者及缓存身份；新增回归、同步文档，Debug/Release 构建与相关测试、格式、命名、边界和严格 OpenSpec 校验。记录实际证据，保留活动 change，不归档、不提交。回退是撤销本项完整源码变更，无持久化数据迁移。

## Open Questions

无。
