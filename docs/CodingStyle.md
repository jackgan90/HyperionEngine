# 代码规范

本仓库默认参考 [Epic C++ Coding Standard](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine)，采用适用于独立 C++20 渲染器的部分。本文件、根目录 `AGENTS.md`、`.clang-format` 和 `.clang-tidy` 一起约束后续开发。

## 命名

| 对象 | 规则 | 示例 |
| --- | --- | --- |
| 命名空间 | PascalCase，以 Hyperion 为根 | `Hyperion`、`Hyperion::Private` |
| 普通类、结构体、值类型别名 | F 前缀 | `FTaskSystem`、`FAppSettings` |
| 抽象接口 | I 前缀 | `IRenderPlugin` |
| 枚举、枚举值 | E 前缀类型，PascalCase 值 | `ELogLevel::Info` |
| 类模板 | T 前缀 | `TResourcePool<ElementType>` |
| 函数、成员、局部变量 | PascalCase，不带下划线前后缀 | `InitializeLog`、`FrameIndex` |
| 参数 | In + PascalCase；输出参数使用 Out | `InWidth`、`OutImage` |
| 布尔变量 | b + PascalCase，包含成员、局部变量、常量和原子布尔标志 | `bIsReady`、`bShouldClose` |
| 布尔参数 | bIn + PascalCase；输出参数使用 bOut | `bInEnabled`、`bOutSuccess` |
| 布尔查询函数 | PascalCase，优先表达状态或问题 | `IsReady()`、`ShouldClose()` |
| 项目宏 | HYP_ + 大写下划线 | `HYP_SOURCE_DIR` |
| 自有文件 | PascalCase，无 F/I/E 类型前缀 | `TaskSystem.h`、`TaskSystem.cpp` |

布尔变量遵循 UE 的小写 `b` 前缀约定，其余变量继续采用大写开头的命名。布尔引用参数同样适用；存放布尔值的集合仍按集合命名，返回布尔值的函数不加 `b`。不采用 UObject、Actor、Slate 专用的 U/A/S 前缀，不引入 UE 宏、容器或运行时。

名称应表达含义，避免不必要的缩写。已有领域缩写如 RHI、DX12、GUI 可以沿用。构造参数使用 `In`，避免与无下划线后缀的成员同名。公开接口在前，受保护/私有实现和成员在后。

## 排版和文件

使用 Allman 大括号；控制流即使只有一条语句也写大括号。用 Tab 缩进，显示宽度为 4 列；对齐可用空格。每条声明只定义一个变量。头文件用 `.h` 和 `#pragma once`，实现用 `.cpp`；C++/HLSL 使用 LF 换行，由 `.editorconfig`、`.clang-format` 和 `.gitattributes` 保持一致。按 [源码组织](SourceLayout.md) 的概念模块和 Public/Private 边界组织新增文件；不要把不相关功能平铺在公共源码目录。

```cpp
namespace Hyperion
{
class FFrameCounter
{
public:
	void Advance(std::uint64_t InCount)
	{
		if (InCount > 0)
		{
			FrameIndex += InCount;
		}
	}

private:
	std::uint64_t FrameIndex = 0;
};
}
```

优先显式类型；lambda、复杂迭代器和模板相关的推导可使用 `auto`，已有 STL 密集的适配器允许在类型明确时使用 `auto`。保持 const 正确性，不为模仿 UE 替换标准库或改动算法。Windows/COM 头文件存在顺序要求时保持单独 include 分组。

## 代码规模与拆分

推荐单一函数不超过 **100 行**，较长函数仍推荐按逻辑边界拆分；仅当逻辑极为内聚且难以有意义地拆分时，才允许超过这一建议值。可按需用注释帮助理解不直观的关联或执行流程；代码结构和现有注释可以自然体现保持完整函数的必要性，不要求仅因超过 100 行就额外添加理由注释。

按完整函数定义计数，从签名首行到结束大括号，包含空行、注释和内部 lambda；构造函数同样适用。优先按职责、数据所有权和执行阶段拆分，避免机械切块、压缩排版或把长代码藏进 lambda。紧密关联的参数可组成有明确职责的上下文，但不要用无边界的共享状态掩盖耦合。

除测试文件外，推荐单个自有 C++ 文件不超过 **1000 行**，较大文件仍推荐按逻辑边界拆分为多个文件；仅当逻辑极为内聚且难以有意义地拆分时，才允许超过这一建议值。范围包括 `.cpp`、`.h` 和承载 C++ 的 `.inl`，按整个文件的物理行数计数，包含 include、空行和注释；第三方代码和自动生成文件不纳入此建议。测试文件按实际用途和构建归属识别，不能只靠文件名或目录名取得豁免；测试文件不要求低于 1000 行，但仍遵循上述函数长度原则。函数符合例外条件并不自动证明整个文件也符合例外条件。

拆分应形成可命名的职责、数据所有权或执行阶段，保持模块 Public/Private 边界、接口契约和对象生命周期。不要机械地按行数切块、压缩排版、堆入含义不明的 Utils 文件，或借助 include 片段隐藏同一实现的长度。

上述数值是代码审查中的推荐做法，不要求仓库所有函数和文件在任意时刻都低于建议值，也不作为仅凭行数判定失败的门禁。新增文件和新增或实质修改的函数应在审查中评估逻辑内聚性及拆分边界；已有较长代码在对应模块实质修改时评估并逐步拆分，局部修复不应顺带扩展为无关的大规模重构。超过建议值的例外仍须由人工审查确认，不能仅因检查通过就视为合理。

`python tools/CheckStyle.py --sizes-only` 自动报告 `Source` 下的 `.cpp/.h/.inl` 物理文件行数，不依赖 LLVM 或已配置的构建目录，也不受当前构建开关影响。LF 和 CRLF 均按物理行计数，末尾没有换行的最后一行也计入。未豁免文件超过 1000 行时输出路径、行数和审查提示，不因行数或文件增长返回失败；函数长度、函数和文件的内聚性例外仍由人工审查。

`tools/SourceSizePolicy.json` 仅维护扫描豁免，不再维护较长生产文件的清单、行数基线或增长上限。

测试、生成或第三方源码若位于 `Source` 内且需要豁免，使用 `exclusions` 逐文件记录 `kind`、用途 `reason` 和构建归属、生成器或第三方来源 `evidence`，不使用目录或文件名通配规则。未分类文件一律检查，较短测试无需提前登记。检查器仍对无效记录结构、非精确或不存在的路径、重复记录及读取错误返回失败，实际用途及构建归属由审查确认；用途或归属变化时同步复核记录。被测试目标复用的生产实现仍按文件规模建议报告。`out/deps` 和构建目录生成物位于扫描范围外。

## 可维护性与语义表达

以下规则适用于新增和修改的自有代码；具体模块仍须遵守各自的领域契约。

1. **逻辑使用稳定、带类型的语义标识，与展示文本分离。** 使用枚举、类型化 ID 或明确的状态表达行为，不根据标签、翻译、菜单顺序或格式化字符串判断。字符串在输入、持久化或协议边界解析和校验，未知值按契约拒绝；不能因展示文字变化而改变行为，也不能为了统一内部类型擅自改动已有外部标识。
2. **同一领域事实只有一个权威定义，其他表示由它推导。** 明确由哪个模块或领域服务拥有定义，通过查询、显式转换或生成机制提供给消费者，避免分别维护字段布局、掩码、绑定偏移或操作目录。各层可以校验自己负责的契约，但应复用权威事实；不要把分层校验误当成重复定义而删除。
3. **用具名、带类型的数据表达含义和关联。** 不让裸数字、位置数组、平行数组、枚举序号或无类型载荷隐含业务关系。关联字段组成明确的记录，有限选择使用枚举或有判别类型，跨表示关系通过显式映射表达。协议数值、位掩码和外部布局可以保留，但其含义、映射和有效范围须有权威定义；仅给魔数换个常量名并不能消除隐含关系。
4. **独立策略显式建模，由领域所有者校验并执行。** 不从 pass 名称、资源可读性或另一个碰巧相关的状态推断写入权限、路由、访问模式等独立决策。GUI、CLI 和 automation 适配层负责输入转换与结果展示，共享领域操作负责校验、事务、历史、持久化及副作用，避免各入口分别实现同一流程；详见 [插件架构](PluginSystem.md) 和 [自动化接入契约](Automation.md#新功能接入契约)。
5. **缓存、复用、失效和通知取决于实际语义输入。** 缓存键和失效条件覆盖所有影响结果的输入；对变更先规范化，再比较前后语义状态并决定需要的更新。新增字段、策略或注册类型时检查完整依赖链，不只维护手写的内建类型白名单。既要避免漏更新，也要避免无语义变化时重建资源或重复通知。

## 外部约定与例外

- 保留语言入口 `main`、操作符，以及标准库/SDK 要求的重写名称（例如 PMR 的 `do_allocate`）。第三方函数、成员、头文件名不改名。
- HLSL 使用相同的自有标识符和排版规则；`register`、系统语义、内建函数和向量分量 `.xyzw` 等保持图形编译器要求的形式。
- 保留 JSON 属性 ID、插件 ID、命令行选项、缓存协议、CMake target/测试 ID。它们是运行时或工具接口，不应随 C++ 符号更名而变化。
- 保留工具识别的文件名：`CMakeLists.txt`、`CMakePresets.json`、`dependencies.lock.json`、`README.md`、`AGENTS.md`、点配置文件，以及 OpenSpec 的 kebab-case 标识和 `proposal.md`/`design.md`/`tasks.md`/`spec.md`。归档中的历史路径和验证证据不回写。
- Python 工具内部使用 PEP 8 的 snake_case，PowerShell 自有标识符使用 PascalCase；工具文件名均为 PascalCase。CMake 内部遵循其常用约定。
- `out` 为生成产物和第三方依赖，不参与自有代码格式化。

## 运行日志

- 日志集中在拥有操作结果的共享服务边界：启动选择、内容切换、导入/保存完成、异步失败及可恢复降级。GUI 与 automation 不重复记录相同事实；不要在每个 throw/catch 中机械添加日志。
- 正常逐帧、逐对象、绘制、任务调度、缓存命中和状态查询不写日志。异步操作只在首次进入完成/失败状态时记录；轮询同一状态不重报。取消、过期任务和用户主动放弃不当作引擎错误。
- 消息说明操作、相关路径或任务/资产/节点标识、阶段、原因及真实结果。成功只在提交后记录；部分成功说明剩余失败。使用 UTF-8 路径，不仅输出缺少上下文的 `what()`，不输出密钥、会话令牌或完整请求载荷。
- Info 记录关键成功结果，Warning 记录仍可继续的降级，Error 记录操作或清理失败，Debug 记录低频的准备细节。底层错误继续向上抛出时，上层只补充有价值的操作上下文；已被转换成状态的错误必须在所属边界进入日志。清理中的日志失败不能中断清理。

## 检查与格式化

shader 的 `HyperionUniforms.generated.hlsli` 及拥有者的 `<Domain>Parameters.generated.hlsli` 是编译侧生成并注入的虚拟 include，由 `Materials/ShaderParameters.cpp` 提供。路径检查接受公共 include 的准确拼写，以及有实际 `HYP_SHADER_DOMAIN` / `HYP_SHADER_DECLARATIONS` 声明头文件支持的局部 include；不豁免任意 generated 文件，也不要求存在同名磁盘源文件。

局部 shader 以显式 cbuffer 引用生成的 `F<Name>Uniform`，例如 ContactShadows.hlsl 对应 ContactShadowParameters.inl。声明文件用 `HYP_UNIFORM_BEGIN/END`、`HYP_UNIFORM_FIELD(Type, Name, Scope)` 单次声明字段，由固定的 BEGIN/END 和 statement 宏配置保持缩进。公共兼容 shader 中完整 uniform 声明的宏调用以分号结束，例如 `HYP_UNIFORM_FrameInfo(b0);`，使声明边界对格式化工具明确，避免随后顶层函数或资源声明被误排为续行。

新增或修改用户可操作功能时，同时遵守 [自动化接入契约](Automation.md#新功能接入契约)。请求/结果复用反射，GUI 与 agent 共享领域逻辑；尚未注册的能力记录在覆盖表，避免以后另建一套实现。

需要 Python 3.10+ 和 与仓库格式配置兼容的 LLVM（格式基线为 22.1.1）的 `clang-format` / `clang-tidy` / `clang-query`。脚本先检查 PATH，也会查找 Windows 默认 LLVM 安装目录。普通构建不强制安装 LLVM。

```powershell
# 仅报告 C++ 文件规模建议并校验扫描豁免，无需 LLVM
python tools/CheckStyle.py --sizes-only

# 检查文件名、头文件路径大小写及 C++/HLSL 排版，并报告 C++ 文件规模建议
python tools/CheckStyle.py

# 按配置格式化所有自有 C++/HLSL 文件，并报告写回后的 C++ 文件规模建议
python tools/CheckStyle.py --format

# 生成 Ninja 编译数据库后，检查所有自有 C++ 翻译单元的语义命名
.\tools\Build.ps1
python tools/CheckStyle.py --naming --build-dir out/build/debug
```

`--naming` 需要有效的 Ninja `compile_commands.json`；Visual Studio generator 不导出该文件。clang-tidy 理解符号所属类型，因此不会把 `std::vector::size()` 或 SDK 字段误判为引擎方法。命名检查覆盖普通声明；依赖模板、宏、HLSL 标识符及模板类型前缀仍需代码审阅。不要对整个仓库（包含 `out/deps`）执行自动修复。

`.clang-tidy` 允许 `b` / `bIn` / `bOut` 前缀，`--naming` 同时用 clang-query 的类型信息检查布尔变量、布尔引用和 `std::atomic<bool>`，包括推导为布尔值的普通 `auto` 变量；也会拒绝非布尔变量借此前缀绕过普通命名规则。生成的字段 enum 镜像布尔成员的 `b` 拼写；HLSL 类型 token 查找宏的明确例外保留 `float4/uint/bool` 等语言拼写。泛型模板的实例化不会把通用参数误判为布尔专用参数；布尔集合和查询函数仍遵循普通命名。

CTest 的 `code_style_paths` 检查自有文件名和 include 路径的准确大小写，`code_style_sizes` 报告 C++ 文件规模建议并校验扫描豁免，`source_size_checker` 验证计数边界、非阻断提示、豁免和无效策略失败路径；这些检查不需要 LLVM。常规 `CheckStyle.py` 也报告文件规模建议，完整格式/命名检查由上述命令显式运行。超出建议行数不会单独导致 CTest、常规检查或格式化模式失败。`--sizes-only` 与 `--paths-only`、`--format`、`--naming` 是互斥模式。

clang-tidy 尚无独立的类模板前缀配置，仓库通过 [`.clang-tidy`](../.clang-tidy) 中的精确命名例外保留类模板的 T 前缀；例外清单以该配置为准，不在文档另行维护。其他普通类型仍强制 F/I 前缀；新增模板也需按本规范审阅。上述代码规模和语义表达规则的人工审查范围见 [构建与验证指南](Verification.md#规范审查与工具覆盖)。
