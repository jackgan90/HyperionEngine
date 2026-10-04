## Why

资产类型已有反射 descriptor 作为身份来源，但部分验证、发布和 Editor 路由仍手写相同 ID；图像导入器的标识也重复出现在注册与消费端。复用已有身份可减少拼写漂移，同时让 GUI 依据实际 C++ 类型选择控件。

## What Changes

- 将已具备合法直接模块依赖的纹理、材质、模型和场景 ID 消费点改为读取 `RecordType<T>().Id`，包括引用验证、构造及 Inspector reference metadata。
- 由图像导入器的公开声明提供唯一 `ImageImporterId`，注册与准备/发布路径共用原有字符串。
- GUI 矩阵控件使用 descriptor 的 `CppType` 识别 `FMat4`，不增加 Scene 依赖。
- 保持持久化 ID、schema、名称覆盖、发布去重、内容路由和目录布局；保留独立测试中的原始协议字符串。
- 旧迁移目录映射及未知类型 `Assets` fallback 明确留待独立阶段，不改变资产格式或反射定义。

## Capabilities

### New Capabilities

- `asset-identity-consumption`: 消费方复用领域拥有的类型/导入器身份，在已有模块边界内保留外部契约。

### Modified Capabilities

无。现有用户能力和协议保持不变。

## Impact

涉及 Runtime/AssetImport、Scene、Gui，Editor 的内容发现/打开路由及 AssetTool 的纹理迁移判断。CMake 依赖方向、target 名称、插件注册/生命周期和 automation 操作保持不变。验收覆盖现有模型/内容/导入/发布回归、类型协议快照及矩阵 inspection；按阶段独立审核后等待用户验收再归档提交。
