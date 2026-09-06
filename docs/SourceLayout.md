# 源码模块与依赖方向

源码以概念模块聚合，每个模块有自己的 `CMakeLists.txt`。公共头文件只从该模块的 `Public` 导出，实现和第三方 wrapper 留在 `Private`，没有全仓库共享的头文件搜索路径。

```text
Source/
  Runtime/
    Core/          # 日志、内存 hook、时间、profile
    Tasks/         # dedicated threads 与 oneTBB wrapper
    Math/          # 引擎数学类型与 GLM wrapper
    Reflection/    # 类型描述及 JSON 序列化 wrapper
    Config/        # 应用/实验配置，依赖 Reflection
    Plugins/       # 逻辑插件注册、依赖和生命周期
    Platform/      # 窗口、输入与 SDL wrapper
    Assets/        # 资产引用、图片、网格及导入 wrapper
    Shaders/       # DXC / SPIRV-Cross wrapper 与编译缓存
    RHI/           # 公共图形契约、能力查询、后端注册表
    Renderer/      # RenderGraph 与 IRenderPlugin
    Gui/           # ImGui / ImPlot wrapper 和引擎绘制数据
  Backends/
    D3D12/         # 独立的 D3D12 RHI provider
  Plugins/
    Triangle/      # 三角形实验
    DebugUI/       # 调试 UI 的渲染插件
  Applications/
    Viewer/        # 应用入口与模块组装
  Tests/           # 对应模块的单元测试及 Integration 验收
```

典型模块结构：

```text
Source/Runtime/Assets/
  CMakeLists.txt
  Public/Hyperion/Assets/Assets.h
  Private/AssetReference.cpp
  Private/Adapters/GltfMesh.cpp
  Private/Adapters/Images.cpp
```

调用方写 `#include "Hyperion/Assets/Assets.h"`，并在自己的 CMake target 声明依赖。只有公共接口确实依赖的模块使用 `PUBLIC`；实现细节使用 `PRIVATE`。第三方库私有链接，第三方类型不能出现在引擎公共接口中。`tools/CheckBoundaries.py` 检查模块依赖声明、私有头越界、第三方 include 和依赖方向。

后续 `Scene`、`Animation` 将作为 `Runtime` 下的独立同级模块，按需依赖 Core、Math、Assets、Tasks、Reflection。它们负责场景层级、变换、空间数据、骨骼与姿态等 CPU 数据；Renderer 通过渲染侧适配生成快照、更新 GPU 资源并构造 pass。它们不应依赖 Renderer、RHI 或某个图形后端。本次没有添加尚无实现需求的占位类或空模块。

Core 中的工具应按职责继续细分，例如 `Memory/`、`Logging/`、`Containers/`；仅供某个模块使用的工具留在该模块 `Private`，避免把所有辅助代码集中进一个无边界的 Utils 模块。当前 Core 接口数量少，后续增长时再按这些概念拆分。

## RHI 扩展位置

`Runtime/RHI` 定义 `IRHIBackend`、`IRHIDevice`、`IRHISwapchain` 和资源契约，不链接 D3D12。`Backends/D3D12/Private` 实现 `FD3D12RHIDevice`、`FD3D12RHISwapchain` 以及原生资源；它的唯一公共入口负责向注册表注册 provider，不暴露 Windows/D3D12 头文件。

加入 Vulkan 时新增 `Source/Backends/Vulkan`，实现同一组接口，在 Viewer 中注册 provider 并链接该 target。Renderer 和算法插件保持使用公共 RHI。后端实现按自身语义实现队列、同步、descriptor 与 shader 接口映射，不应在公共 RHI 中添加 API 分支。当前 Vulkan/Metal 只有后端标识，尚未实现；选择它们会明确报错。

在 Visual Studio 中，解决方案按 `Hyperion/Runtime`、`Hyperion/Backends`、`Hyperion/Plugins`、`Hyperion/Applications`、`Hyperion/Tests` 分组，每个模块内部显示 Public/Private 筛选器。生成和测试命令仍见 [VisualStudio.md](VisualStudio.md)。
