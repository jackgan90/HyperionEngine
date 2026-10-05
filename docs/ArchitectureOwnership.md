# 当前架构所有者与接入路径

本页描述当前代码所有权。模块目录与直接依赖规则见 [源码组织](SourceLayout.md)，线程与 GPU 退休契约见 [架构契约](Architecture.md)。这里的“无渲染依赖”指 CPU 数据模块的实际链接图。

| 事实或职责 | 唯一所有者 | 消费和扩展路径 |
| --- | --- | --- |
| Tasks、Main 泵、时间、退出状态 | Runtime/Application 的 FApplicationHost | 应用选择静态目录和 native provider；功能插件使用 Start/Update/Quiesce/Stop |
| 文档、Selection、History、dirty、revision、保存 | Runtime/SceneEditing 的 FSceneEditDocument 和领域操作 | Editor 与 automation 使用同一 SceneEditing 请求 |
| 节点、组件、局部变换、相机和光源 CPU 数据 | Runtime/Scene 的 FScene | 组件描述符、ComponentChanges；不依赖 Renderer/RHI |
| 材质、纹理、Environment 的 CPU 数据和处理 | 对应 Runtime 数据模块 | 自有类型和算法；不依赖 Renderer/RHI 或 native backend |
| CPU 图像尺寸、RGBA 存储与颜色编码 | Runtime/ImageData | 仅标准库的值类型；Assets 编解码、RHI 上传/回读、GUI 字体和 Renderer 输出共用 |
| Engine/Game 内容环境、切换准入和失效 | Runtime/Content 的 FContentRootService | IContentRootParticipant 作用域注册，dirty/busy 检查和退休共享 |
| 资产引用、IO、加载图；编辑与导入草稿 | Assets / AssetEditing / AssetImport | 原生资产服务和事务；importer 描述符统一转换、能力发现、选项支持和准入，workspace 构造时选择并冻结 |
| 场景到渲染桥接、浏览相机、primitives、资源准备 | Runtime/Renderer | Main 发布快照，Render 收集，RHI 0 创建和退休，GPU fence 决定释放 |
| 渲染设置、视口、阴影/光源、输出、诊断和抓帧控制契约 | Runtime/RenderControls | CPU DTO、反射和纯校验；Renderer 持有生产适配，原 Renderer 头兼容导出 |
| GUI 手势、窗口协调、领域服务 provider | Plugins/Editor | FEditorPlugin 组合各职责，通过领域接口服务 GUI/automation；层级拖动由私有 FEditorReparentController 完整持有 |
| 标准化 GUI、绘制数据复制和渲染 | Runtime/Gui、Runtime/GuiRenderer | GUI 在 Main，复制后的绘制数据交给 Render/RHI；SDK 保持私有 |
| 设备和 swapchain 的 native 实现 | Backends/D3D12 | 应用注册 provider；Runtime 使用 RHI 契约和 Platform 不透明 FNativeSurface |
| 操作目录、反射请求/结果、任务和传输 | Runtime/Automation 与功能注册适配器 | 类型化领域服务；transport 无功能分支，稳定 operation/schema/revision |

RHI 的图像契约直接依赖 ImageData，链接闭包不包含 Assets 服务。Assets.h 兼容导出同一份图像类型，加载、编解码和文件写入仍由 Assets 提供。Renderer 为原生资产消费者保留 Assets 依赖。

RenderControls 公开七组控制接口及其权威数据定义，依赖 Scene/SceneEditing、Math、Reflection 和 RasterOptions，其生产闭包不包含 Renderer/RHI、native backend 或具体插件。RasterOptions 持有数值 FViewport，RHI 兼容导出同一类型。管线转换、配置文件读写、设备/任务采样、回读/图片文件完成和包含 MainCameraView 的 FRenderFrameStatistics 继续由 Renderer 提供；诊断使用其 CPU 统计基类，既有反射字段不变。

Automation 插件使用 RenderControls 的公共入口，直接依赖及其 CPU 闭包无需 Renderer。Automation 应用仍通过 ApplicationServices 聚合图形服务，整个 CLI 的依赖闭包仍包含 Renderer；公共契约消费者的边界证明不等同于整个 CLI 可以纯 CPU 构建。

控制契约消费者 `public_render_controls` 覆盖七组接口的 schema、默认值和 wire 往返；`public_rhi_image` 验证独立图像上传/回读签名。两者都只链接所测公开 target。

层级拖动控制器只持有手势快照、具名的根/节点投递状态、token 和一次性展开请求。它同步借用 GUI、SceneEditing 文档和窄动作接口，不保存 provider、文档引用或回调；Selection、History、dirty、revision 和 KeepWorld 提交继续由 SceneEditing 管理。文档/内容替换与 Editor 停止先 Reset 控制器，再退休文档与 GUI；部分启动时允许 GUI 尚未取得。

## 依赖验证

RenderGraph 在接受 pass 时保存共同依赖和 graphics/compute 专属 payload 的 variant。legacy 单色附件在 Add 时归一化到唯一列表，Color 与 Colors 同时存在会在准入时拒绝；原生命令布局保留兼容选择。拓扑与内容检查先于解析，物理描述检查先于 Prepare，回调结果检查先于帧获取和提交。

`tools/Build.ps1` 和 `tools/GenerateSolution.ps1` 成功配置后通过 `TargetGraph.py` 保存构建目录的 `TargetGraph.json`。收集器消费 CMake 实际展开执行的 target、源文件和链接声明，保留 PUBLIC/PRIVATE/INTERFACE 与声明位置。

```powershell
.\tools\Build.ps1 -Preset debug
python -B tools/CheckBoundaries.py --build-dir out/build/debug
python -B Source/Tests/Tools/BoundaryTests.py
```

CTest 的 `dependency_boundaries` 使用自己的构建目录。CMake 输入、源文件清单或缓存变化后须重新配置；缺失或陈旧元数据会失败。直接运行 CMake 不生成这份元数据，应使用上述入口，或用 `TargetGraph.py --cmake <cmake路径> --build-dir <目录> -- <配置参数>` 包装配置。

每份图只覆盖一次成功配置实际执行的条件分支；报告列出配置和选项。Ninja 的 Debug/Release 分开验证；Visual Studio 图记录配置阶段声明及多配置清单。当前源文件和链接声明没有需要逐配置解释的 generator expression；新增此类表达式、FILE_SET、别名/imported owned target、图属性修改、目录级 link_libraries、PUBLIC/INTERFACE 源传播、未知 helper 中的 target 声明或歧义相对路径会明确失败，须扩展收集器和夹具后使用。Target 必须在所属模块 CMakeLists.txt 直接声明；Source/Tests 内现有 hyp_test 通过 trace 调用栈核对调用目录后支持。Dependencies.cmake 中现有第三方 wrapper/imported target 声明属于第三方图。编译选项、测试命令和第三方自身构建图中的表达式由 CMake 处理。

生产源按实际 target 归属检查直接依赖，公共头要求直接公开依赖。生产图检查真实环和 CPU/Runtime/backend 约束。Source/Tests 的 target，以及非应用目录的验证 executable，单独列为测试图；它们保留私有头与 SDK 约束，不参与生产分层结论。测试复用的生产实现仍按每个生产编译 owner 检查。

未选模块和未编译源列为“仅源隔离检查”，不据此声称已验证其链接关系。未编译 CPP 对 Source/Tests 支持头仅验证存在与路径范围，不猜测其测试身份；任何实际生产编译 owner 仍禁止使用该支持头。已知布尔构建选项确定关闭的条件 include 不形成该配置的直接依赖结论；未知表达式保守检查两边，私有/SDK 源隔离覆盖全部真实 include。BUILD_TESTING 关闭时的 Unavailable 实现、可选插件开关等，需用对应配置验证。公共消费者 `public_native_model`、`public_render_output`、`public_gui_diagnostics` 分别只链接所测公开 target，避免完整应用的偶然 include 链掩盖声明错误。
