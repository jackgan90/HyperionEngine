## Context

纹理、材质、模型和场景已有 `RecordType<T>()` descriptor；消费方仍重复手写部分 ID。图像导入器的注册与准备/发布分支也重复同一字符串。Gui 能访问 `FMat4`，但其反射特化属于 Scene，Gui 不依赖 Scene。

## Goals / Non-Goals

**Goals:**

- 在已有合法模块依赖内复用类型 descriptor 的 ID。
- 图像导入器注册与消费共享由 AssetImport 拥有的 ID。
- 矩阵 Inspector 根据实际 C++ 类型分派，并保持编辑结果。
- 保留全部外部类型/导入器 ID、schema、引用 metadata、目录、去重、名称覆盖和 Editor 路由。

**Non-Goals:**

- 不修改反射定义、持久化协议、插件生命周期或 automation 操作。
- 不引入新类型注册系统、模块依赖或迁移目录策略。`Assets/AssetAuthoring.cpp` 的旧迁移目录表与未知类型 `Assets` fallback 保留。
- 不将独立协议测试中的预期字符串改为引用被测常量。

## Decisions

1. AssetImport 的纹理识别、AssetTool 的纹理迁移、Editor 场景筛选/打开、Scene 的材质引用和模型 Inspector metadata 使用 `RecordType<T>().Id`，并直接 include 所需类型声明。现有 target 已直接依赖这些领域模块，无需公共字符串注册表或额外 CMake 依赖。
2. 在 `ImageImport.h` 定义 `inline constexpr char ImageImporterId[] = "hyperion.image"`。注册、准备阶段名称覆盖和直接发布的名称覆盖共享此值；外部 importer 注册仍允许扩展，不引入封闭枚举。
3. Gui 使用 `Type.CppType == typeid(FMat4)` 分派矩阵编辑器。该身份由已有 descriptor 提供，不迁移 Scene 中的反射定义，不为取得字符串 ID 引入反向模块依赖。
4. 验证优先复用模型、内容浏览、原生发布、共享发布和导入 workspace 测试。补充矩阵 inspection 的实际鼠标编辑以及图像直接导入/准备后发布的名称与 provenance 检查。反射 describe 快照核对已公开的纹理、材质、模型和矩阵协议。

## Risks / Trade-offs

- [新增 descriptor 查询可能触发初始化依赖] → 检查所涉反射构造调用链并运行 Scene/模型回归，保持反射声明本身不变。
- [字符串替换意外改变目录或未知类型行为] → 限定消费点，不修改旧迁移目录表或 fallback；审核最终 diff。
- [GUI 测试只覆盖 descriptor 值而未覆盖控件] → 使用帧与输入事件验证矩阵值实际写回，验证与显示/序列化 ID 解耦。
- [类型协议快照不能覆盖所有 Editor 类型] → 独立运行内容路由、场景及反射测试；不将 standalone 未注册类型算作新增能力。

## Migration Plan

无需数据迁移。按任务实现、构建并运行定向回归，独立审计后冻结 diff，等待用户验收后再归档并提交。
