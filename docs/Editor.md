# 场景编辑器

`hyperion_editor` 是独立的 Windows / D3D12 应用。第一版提供 UE 风格的深色可停靠工作区：顶部菜单与工具栏、中央 Viewport、右侧 Outliner / Details、底部 Content Browser 和状态栏。

原生标题栏默认使用黑色背景和浅色文字，由 Platform 封装 Windows DWM 设置。精确标题栏颜色需要 Windows 11；不支持时保留系统可用的外观，不影响编辑器启动。

## 构建与打开 Sponza

与 Viewer 使用同一套依赖和内容挂载。检出同级 HyperionAssets 并执行 `git lfs pull`；挂载与内容准备见 [ContentFileSystem.md](ContentFileSystem.md)。从引擎仓库根目录运行：

```powershell
python tools/Bootstrap.py
./tools/Build.ps1 -Preset release -Target hyperion_editor
./out/build/release/bin/hyperion_editor.exe
```

选择 **File > Open Scene...**，在场景列表中选择 `/Game/Scenes/Sponza.hasset`，点击 **Open**。列表来自已挂载库的 `Catalog.hasset`；也可以输入原生场景的虚拟路径。首次加载期间状态栏显示进度，资源就绪后出现在视口中。失败时状态栏显示错误，可以再次打开其他场景。

工具栏的 **Open Scene** 和 Content Browser 的 **Browse / Open...** 打开同一个对话框。Content Browser 列出场景资产；此版本尚未提供通用资产缩略图浏览。

可选启动参数：

| 参数 | 用途 |
|---|---|
| `--mounts ContentMounts.json` | 指定挂载文件；默认优先使用 `ContentMounts.local.json` |
| `--scene /Game/Scenes/Sponza.hasset` | 启动后直接打开指定原生场景 |
| `--layout out/editor/Layout.ini` | 布局文件；此路径也是默认值 |
| `--frames 1200 --capture out/editor/Sponza.png` | 运行指定帧数并保存最后一帧 |
| `--benchmark out/editor/Frames.csv --benchmark-warmup 120 --benchmark-samples 300` | 资源就绪后预热并记录帧、场景、GUI 和渲染等待耗时 |
| `--benchmark-camera` | 在采样期间执行确定性的移动相机路径 |
| `--benchmark-collapsed` | 基准测试中默认收起 Outliner 层级，分离展开列表的成本 |

## 操作

- 在视口中**按住鼠标右键**，使用 **WASD** 前后左右移动、**Q/E** 下降/上升；方向键与 PageUp/PageDown 为别名。单独按移动键不移动镜头，松开右键立即停止平移。
- **右键拖动**在当前位置调整镜头朝向，鼠标向右转头、向下低头；只旋转时世界位置、镜头参数及对焦距离保持不变。
- **按住右键滚动滚轮**调整 WASDQE 移动速度：向上加速、向下减速，每格乘以或除以 1.2。仅调速不会改变镜头位置、朝向或镜头参数；移动中调速立即影响后续平移。
- **不按右键滚动滚轮**沿当前镜头前后方向推拉，保持原有步幅与对焦距离调整，不改变选定的 WASDQE 速度。
- 视口工具栏的 **Camera Speed** 显示实际平移速度，单位 **u/s**（场景单位/秒）。初始速度使用编辑器视角的 `max(1, FocusDistance)`，范围为 `0.01–100000 u/s`；失焦、打开对话框或重开场景保留选定速度，退出应用后不保存该设置。
- 复用 Runtime 的 `FSceneCameraController` 飞行模式。SceneViewer 使用默认环绕模式，其他视口宿主也可以显式选择飞行模式。
- **Home** 或工具栏 **Frame Scene** 将镜头对准场景范围。
- **Exposure** 调整视口曝光，不写回资产。
- Outliner 支持搜索和层级选择，一个完整模型实例对应一个模型对象，内部 primitive 不自动成为场景子对象。Details 展示反射属性，Apply 提交组件修改，Revert 放弃草稿。模型整体及 section 材质 override 仍属于实例，不改变共享资产；已有显式展开的场景继续兼容。
- **Edit > Undo / Redo** 撤销重做；**File > Save Scene / Save Scene As** 保存文档。标题 `*` 表示未保存修改，保存期间的新修改仍保持为脏。
- **Transform (Local)** 分为 Position、Rotation、Scale 三行，分别提供 X/Y/Z 输入。Rotation 使用度数，绕固定 X、Y、Z 轴依次旋转；悬停查看具体约定。输入数值并按 Enter 后形成草稿，Apply 提交，Revert 放弃。原有剪切信息保留，重新挂接父对象保持局部矩阵。组件菜单支持添加注册的 CPU 组件和移除非必需组件，提交前验证完整场景。
- **Rendering diagnostics (read only)** 显示对应 primitive 的已应用状态和最近 draw。属性来自 Main，诊断来自 Render 发布的只读副本。组件与扩展契约见 [SceneComponents.md](SceneComponents.md)。
- 拖动标签可调整停靠位置；**Window** 菜单可重新显示关闭的面板，**Reset Layout** 恢复默认布局。

键盘与鼠标导航受视口焦点、悬停和拖动状态约束。文本输入、菜单和模态对话框阻止相机操作；失焦、隐藏视口、最小化或切换场景会重置持续输入。退出时保存停靠位置和面板尺寸。面板显示开关在每次启动时恢复默认开启。

## 编辑器视角、初始视图与场景相机

**Editor view** 是临时浏览视角，不出现在 Outliner。导航、Frame Scene 和曝光不使场景变脏；普通 Save Scene 不保存浏览位置。打开场景时使用场景的可选 `initialView`，缺省时对可见模型取景，空场景使用稳定的默认视角；不隐式跟随 `defaultCamera`。

视口的 **Set initial view** 将当前编辑器视角的位置、朝向与镜头参数写入场景设置。它支持 Undo/Redo，保存后下次打开采用该视图。它没有 Enabled、父对象或 Outliner 节点。导航不会自动更新这个预设。

Camera 组件用于用户明确创作的场景相机。点击 **Create camera from view** 创建可撤销的相机对象；它不会自动成为运行时默认相机。选择相机后，**Preview camera** 或视口的 **View source** 可切换为该相机的实时预览。此时导航与 Frame Scene 停用，修改 Transform/Camera 组件并 Apply 后直接看到效果。**Return to editor view** 恢复进入预览前的独立浏览视角。

预览相机或其父对象禁用、相机删除、Camera 组件移除时，视口清空并显示不可用原因，不保留旧相机副本，也不切换到其他相机。**Apply editor view to camera** 把保留的编辑器视角应用到所选相机；有父对象时换算为局部变换，父变换不可逆则拒绝整次修改。该操作同时写入镜头参数，支持 Undo/Redo。存在组件草稿时须先 Apply/Revert。当前版本不提供 Pilot、相机画中画或视锥 Gizmo。

## 模块与帧顺序

```text
Editor panels / scene selection / viewport input
          |                         |
        Gui                   SceneInstance + CameraController
   private ImGui adapter            |
          |                  SceneRenderPipeline
      FGuiDrawData                  |
          |                 retained viewport texture
          +-------- GuiRenderer ----+
                          |
                     RenderGraph
                          |
                    engine RHI / D3D12
```

ImGui Docking 仅负责控件、停靠、布局和三角形绘制数据。GuiRenderer 统一负责字体、顶点/索引 buffer、资源绑定和绘制，DebugUI 也复用该模块。增加常用控件时扩展 Gui 的引擎接口；增加编辑器面板时在 Editor 中组装，无需直接依赖 ImGui。DockBuilder 等内部接口集中在 Gui 私有 adapter，依赖版本固定于 lock 文件。

Main 构建 UI 并路由相机输入，更新 Scene 后冻结场景帧；Render 将场景绘制到与视口物理尺寸一致的保留纹理，再构建 GUI 合成 pass。GUI 的采样读取显式声明在 RenderGraph 中，纹理源及生命周期随帧保留。此版本逐帧等待 Render/RHI 完成，再处理下一次 UI/场景变更，优先简化所有权；没有改变 Viewer 的 CPU 帧管线。

场景色调映射写入 RGBA8 纹理的 sRGB RTV，GUI 通过 UNORM SRV 读取显示编码值，现有 GUI shader 解码后再写入 sRGB backbuffer。D3D12 为 RGBA8 颜色纹理提供 linear / sRGB 两个 RTV；其他颜色格式仍只允许 linear 视图。

## 当前边界

- 单个原生窗口，窗口内部可停靠和浮动；未启用跨原生窗口的 ImGui multi-viewport。
- 一个场景视口，默认 Deferred / reversed-Z；此版本不开放 offscreen 深度调试预览。
- Details 支持反射编辑、保存与撤销重做，并提供从编辑器视角创建相机的入口；尚无 Gizmo、Play、通用资产选择器、通用对象创建删除工具或插件式面板注册。模型替换和新实例创建使用 Runtime 或导入工具。
- 界面使用内置 Roboto 字体和英文标签。字体 atlas 在启动时生成；中文字符显示与动态字体更新尚未加入。

## 验证入口

`gui_docking` 验证布局保存恢复和纹理 ID；`gui_texture_rendering` 验证真实 RHI 离屏 sRGB 输出、GUI 采样和无效绑定拒绝。`scene_viewer_controls` 覆盖共享控制器的默认环绕模式、飞行模式的按键门槛、固定位置旋转、滚轮调速/推拉分流、速度与位移一致性、速度边界及输入中断恢复。`editor_acceptance` 使用真实控件位置产生 Platform 格式的输入事件，覆盖菜单打开 Sponza、右键移动门槛、松开右键停止、原地旋转、右键滚轮调速、普通滚轮推拉、模态输入隔离、视口隐藏与恢复、窗口缩放、场景重开、加载错误恢复和加载中退出。GPU 验收需要 D3D12 环境及挂载的 Sponza 资产。

文档验收通过 `--exercise-document OUTPUT.hasset` 点击反射属性和 Apply，验证草稿保护、Render 已应用状态、Undo/Redo、异步保存期间的新修改、重载、无效/过期提交拒绝、只读挂载保存失败和独立视口相机。`--report` 输出验收结果及保存耗时；输出场景写到指定测试目录。

文档验收还使用真实键盘输入编辑 Position X、Rotation Z 和 Scale Y，检查度数到矩阵的转换、精确 Undo/Redo 及保存重载。

`--exercise-views OUTPUT.hasset` 验证初始视图按钮、相机创建与世代变化、父级坐标换算、不可逆父级拒绝、应用视角、预览失效清屏、返回原编辑器视角以及保存重开；同样纳入 `editor_acceptance`。

`tools/ComponentEditorBenchmark.py` 可重复运行 Debug/Release、Editor/Viewer、静止/移动相机组合，保留逐帧 CSV、日志、程序哈希和进程内存采样。Viewer 的流水线吞吐耗时与 Editor 的逐帧等待耗时应分别比较。本次组件化的测量条件、回退修正与结果见 [性能对比报告](ComponentEditorPerformance.md)。
