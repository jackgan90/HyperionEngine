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
| 布尔变量和查询 | 大写开头，优先表达状态或问题 | `IsReady`、`ShouldClose` |
| 项目宏 | HYP_ + 大写下划线 | `HYP_SOURCE_DIR` |
| 自有文件 | PascalCase，无 F/I/E 类型前缀 | `TaskSystem.h`、`TaskSystem.cpp` |

**布尔命名是本仓库对 UE 的明确调整**：UE 常用小写 `b` 前缀；用户要求变量大写开头，因此本仓库不使用该前缀。也不采用 UObject、Actor、Slate 专用的 U/A/S 前缀，不引入 UE 宏、容器或运行时。

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

## 函数长度与职责

单一函数原则上不应超过 **100 行**，超出应按逻辑拆分，除非逻辑上耦合极紧密，难以拆分。例外应在函数附近用简短注释说明具体原因，不能仅以“现有代码”或“拆分麻烦”为由豁免。

按完整函数定义计数，从签名首行到结束大括号，包含空行、注释和内部 lambda；构造函数同样适用。优先按职责、数据所有权和执行阶段拆分，避免机械切块、压缩排版或把长代码藏进 lambda。紧密关联的参数可组成有明确职责的上下文，但不要用无边界的共享状态掩盖耦合。

这是编码和审查原则，允许有依据的例外，不设置无条件的自动构建失败阈值。新增或实质修改的函数遵循本原则；已有超长函数在对应模块迭代时逐步处理。

## 外部约定与例外

- 保留语言入口 `main`、操作符，以及标准库/SDK 要求的重写名称（例如 PMR 的 `do_allocate`）。第三方函数、成员、头文件名不改名。
- HLSL 使用相同的自有标识符和排版规则；`register`、系统语义、内建函数和向量分量 `.xyzw` 等保持图形编译器要求的形式。
- 保留 JSON 属性 ID、插件 ID、命令行选项、缓存协议、CMake target/测试 ID。它们是运行时或工具接口，不应随 C++ 符号更名而变化。
- 保留工具识别的文件名：`CMakeLists.txt`、`CMakePresets.json`、`dependencies.lock.json`、`README.md`、`AGENTS.md`、点配置文件，以及 OpenSpec 的 kebab-case 标识和 `proposal.md`/`design.md`/`tasks.md`/`spec.md`。归档中的历史路径和验证证据不回写。
- Python 工具内部使用 PEP 8 的 snake_case，PowerShell 自有标识符使用 PascalCase；工具文件名均为 PascalCase。CMake 内部遵循其常用约定。
- `out` 为生成产物和第三方依赖，不参与自有代码格式化。

## 检查与格式化

需要 Python 3.10+ 和 LLVM 22.1.1（本机验证版本）的 `clang-format` / `clang-tidy`。脚本先检查 PATH，也会查找 Windows 默认 LLVM 安装目录。普通构建不强制安装 LLVM。

```powershell
# 检查文件名、头文件路径大小写与 C++/HLSL 排版
python tools/CheckStyle.py

# 按配置格式化所有自有 C++/HLSL 文件
python tools/CheckStyle.py --format

# 生成 Ninja 编译数据库后，检查所有自有 C++ 翻译单元的语义命名
.\tools\Build.ps1
python tools/CheckStyle.py --naming --build-dir out/build/debug
```

`--naming` 需要有效的 Ninja `compile_commands.json`；Visual Studio generator 不导出该文件。clang-tidy 理解符号所属类型，因此不会把 `std::vector::size()` 或 SDK 字段误判为引擎方法。命名检查覆盖普通声明；依赖模板、宏、HLSL 标识符及模板类型前缀仍需代码审阅。不要对整个仓库（包含 `out/deps`）执行自动修复。

CTest 的 `code_style_paths` 检查自有文件名和 include 路径的准确大小写，不需要 LLVM；完整格式/命名检查由上述命令显式运行。

clang-tidy 尚无独立的类模板前缀配置，`.clang-tidy` 对 `TAsyncState`、`TAsyncResult`、`TAssetRequest` 设置了精确命名例外，以保留仓库要求的 T 前缀。其他普通类型仍强制 F/I 前缀；新增模板也需按本规范审阅。
