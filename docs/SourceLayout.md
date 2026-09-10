# 源码模块与依赖方向

源码以概念模块聚合，每个模块有自己的 `CMakeLists.txt`。公共头文件只从该模块的 `Public` 导出，实现和第三方 wrapper 留在 `Private`，没有全仓库共享的头文件搜索路径。

```text
Source/
  Runtime/
    Core/          # 日志、内存 hook、时间、profile
    Tasks/         # dedicated threads 与 oneTBB wrapper
    IO/            # 专用 IO 队列、字节读写、存储后端
    Math/          # 引擎数学类型与 GLM wrapper
    Reflection/    # 类型描述及 JSON 序列化 wrapper
    Serialization/ # 反射记录的原生二进制内存 Archive
    Config/        # 应用/实验配置，依赖 Reflection
    Plugins/       # 逻辑插件注册、依赖和生命周期
    Platform/      # 窗口、输入与 SDL wrapper
    Assets/        # 资产引用、图片、网格及导入 wrapper
    Materials/     # CPU 材质定义、实例、semantic 和资源源数据（仅 Core/Math）
    Scene/         # 独立 CPU 模型、Main 逻辑场景、清单与反射注册
    AssetImport/   # glTF 场景适配，通过 Assets / IO 加载
    Shaders/       # DXC / SPIRV-Cross wrapper 与编译缓存
    RHI/           # 公共图形契约、能力查询、后端注册表
    Renderer/      # render session / primitive / resources / SceneInstance / Model 桥接 / RenderGraph
    Gui/           # ImGui / ImPlot wrapper 和引擎绘制数据
    Capture/       # 可选 RenderDoc API wrapper，不依赖原生 RHI 后端
  Backends/
    D3D12/         # 独立的 D3D12 RHI provider
  Plugins/
    Triangle/      # 三角形实验
    DebugUI/       # 调试 UI 的渲染插件
    ModelViewer/   # 异步静态模型显示与相机
    SceneViewer/   # 场景实体的交互、相机及空间剔除诊断
    RenderDoc/     # 可选抓帧服务的设备创建前生命周期
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

`Scene` 已作为独立 Runtime 模块实现，直接依赖 Math、Reflection、Materials，保存静态模型和节点层级，不依赖 Renderer/RHI。`AssetImport` 将外部格式转换为 Scene 数据，`Assets` 提供通用异步服务，`Renderer` 的 Model 桥接将 CPU 数据注册为 Render primitives，共享资源服务管理 GPU 资源，由 session 统一收集和提交场景 pass。完整线程/所有权契约见 [RenderPrimitives.md](RenderPrimitives.md)。未来 `Animation` 作为同级模块保存骨骼、姿态等数据，同样不依赖 Renderer、RHI 或图形后端。资产流程详见 [AssetPipeline.md](AssetPipeline.md)。

Core 中的工具应按职责继续细分，例如 `Memory/`、`Logging/`、`Containers/`；仅供某个模块使用的工具留在该模块 `Private`，避免把所有辅助代码集中进一个无边界的 Utils 模块。当前 Core 接口数量少，后续增长时再按这些概念拆分。

## RHI 扩展位置

`Runtime/RHI` 定义 `IRHIBackend`、`IRHIDevice`、`IRHISwapchain` 和资源契约，不链接 D3D12。`Backends/D3D12/Private` 实现 `FD3D12RHIDevice`、`FD3D12RHISwapchain` 以及原生资源；它的唯一公共入口负责向注册表注册 provider，不暴露 Windows/D3D12 头文件。

加入 Vulkan 时新增 `Source/Backends/Vulkan`，实现同一组接口，在 Viewer 中注册 provider 并链接该 target。Renderer 和算法插件保持使用公共 RHI。后端实现按自身语义实现队列、同步、descriptor 与 shader 接口映射，不应在公共 RHI 中添加 API 分支。当前 Vulkan/Metal 只有后端标识，尚未实现；选择它们会明确报错。

在 Visual Studio 中，解决方案按 `Hyperion/Runtime`、`Hyperion/Backends`、`Hyperion/Plugins`、`Hyperion/Applications`、`Hyperion/Tests` 分组，每个模块内部显示 Public/Private 筛选器。生成和测试命令仍见 [VisualStudio.md](VisualStudio.md)。

材质的 CPU/编译/绑定边界及扩展入口见 [Materials.md](Materials.md)。Materials 与 Scene 的传递依赖同样接受 `CheckBoundaries.py` 检查，禁止引入 RHI、Renderer 和原生后端。
