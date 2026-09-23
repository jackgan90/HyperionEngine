# 场景与资产编辑器

`hyperion_editor` 是独立的 Windows / D3D12 应用。提供 UE 风格的深色可停靠工作区：顶部菜单与工具栏、左侧 Place Object、中央 Viewport、右侧 Outliner / Details、底部 Content Browser 和状态栏。

原生标题栏默认使用黑色背景和浅色文字，由 Platform 封装 Windows DWM 设置。精确标题栏颜色需要 Windows 11；不支持时保留系统可用的外观，不影响编辑器启动。

## Editor preference 与抓帧

通过 **Edit > Editor preference** 打开偏好弹窗。首个选项 **Enable RenderDoc capture** 默认关闭，修改后立即保存到 `out/editor/Preferences.ini`，下次启动保留；`--editor-preferences <path>` 可指定独立的本地偏好文件。该文件与场景、布局及界面缩放配置分开，保存失败会在弹窗中显示错误并提供重试。

需要通过 `tools/Build.ps1 -RenderDoc` 或 `tools/GenerateSolution.ps1 -RenderDoc` 编入 RenderDoc 支持，并安装 RenderDoc。首次启用后重启编辑器，使 hooks 在图形设备创建之前加载。开启偏好后，Viewport 工具栏右侧显示相机形抓帧按钮；点击会捕获包含场景、GUI 和 Present 的完整帧，保存到 `out/captures`，并启动 RenderDoc 打开本次生成的 RDC。与 Scene Viewer 共用抓帧和自动打开逻辑。

关闭偏好立即隐藏按钮；插件 hooks 在退出前保留，不会运行中卸载。未编入支持、运行库不可用、尚未重启或显式传入 `--disable-plugin renderdoc` 时，按钮禁用，悬停提示及偏好弹窗显示原因，其他编辑功能仍可用。抓帧或打开失败也可在这些位置查看状态。

## 界面缩放

**Window > Application Scale** 统一调整文字、控件、间距、工具栏和状态栏，下一帧生效。默认 125%，支持 100% / 125% / 150% / 175% / 200% 预设、自定义数值及恢复默认。左右拖动 Custom 数值可实时调整，Ctrl+单击可直接输入倍率；鼠标停下后倍率保持不变。字体按目标字号重新生成，停靠分区比例、场景数据和相机状态保持不变。放大后面板可显示的内容减少，可使用滚动或调整停靠分区。

正常退出时偏好保存到 `out/editor/UiScale.ini`，独立于场景和 `Layout.ini`。`--ui-scale 1.5` 覆盖启动倍率，`--ui-preferences <path>` 指定偏好文件；有效范围为 1–2。缺失或损坏的偏好回退到 125%。验收和 benchmark 运行不读写用户偏好。Viewer 诊断面板也提供 Application Scale，其交互运行偏好保存在 `out/viewer/UiScale.ini`。

Application Scale 不调整原生标题栏、场景镜头或渲染分辨率；framebuffer 像素密度仍独立处理。这不是自动的跨显示器 DPI 布局策略。

## 构建与打开 Sponza

与 Viewer 使用同一套依赖和内容挂载。检出同级 HyperionAssets 并执行 `git lfs pull`；挂载与内容准备见 [ContentFileSystem.md](ContentFileSystem.md)。从引擎仓库根目录运行：

```powershell
python tools/Bootstrap.py
./tools/Build.ps1 -Preset release -Target hyperion_editor
./out/build/release/bin/hyperion_editor.exe
```

选择 **File > Open...**，在系统目录对话框中选择资产根目录，例如 `F:\HyperionAssets`；该目录直接映射为 `/Game`。然后通过 **File > Open Scene...** 选择 `Scenes/Sponza.hasset`，点击 **Open**。列表和路径输入框显示相对于资产根目录的路径，内部仍使用 `/Game/Scenes/Sponza.hasset` 加载。列表异步递归扫描当前根目录中的 `.hasset`，根据文件内的资产类型识别场景，不要求额外的目录资产或固定子目录；也可以直接输入相对根目录的场景路径。首次加载期间状态栏显示进度，资源就绪后出现在视口中。失败时状态栏显示错误，可以再次打开其他场景。

工具栏的 **Open Scene** 打开同一个对话框。对话框中单击条目只选中，双击条目等价于选中后点击 **Open**，同样遵循未保存修改提示。

### 资产根目录与 Content Browser

Content Browser 的根节点和顶部路径显示为 **All**，子目录路径显示为 `All/Scenes` 等；`/Game` 仅作为内部挂载标识。状态栏、保存提示、错误信息、Open Scene、Save Scene As 和 Details 中的资产路径统一省略 `/Game/` 前缀。资产路径输入按当前根目录解析，内部引用和保存数据仍保留完整虚拟路径；本地绝对路径与其他虚拟挂载路径不受影响。

**File > Recent** 显示最近最多五个成功选择的根目录，按最近使用排序并去重。下次启动自动恢复最后成功选择的目录，保持空场景；显式 `--scene` 仍可指定启动场景。显式 `--asset-root <directory>` 优先于保存的根目录。首次启动没有保存的目录时保持 `/Game` 未挂载，Content Browser 显示选择目录提示。保存目录已经失效时显示错误并保持 `/Game` 未挂载，可重新选择目录；不会悄悄使用另一个 Game 目录。根目录和历史与 RenderDoc 偏好一起保存在 `Preferences.ini`。

Content Browser 左侧显示 `/Game` 目录树，右侧显示所选目录的直接子目录和 `.hasset` 文件，文件夹排在文件前面。中间分隔条可拖动；双击文件夹进入，双击场景打开场景文档，双击 Model、Texture、Material、Sky 打开各自资产页签，损坏文件和未知类型显示读取错误。采用文件夹/文件图标，暂不生成资产内容缩略图。默认布局中浏览器位于 Viewport 下方，右侧 Outliner / Details 保留完整高度；已有布局保持不变，**Window > Reset Layout** 可应用新默认布局。**Refresh**、进入目录和保存资产后更新目录；尚未监听系统文件变化。Open Scene 对话框每次打开和点击 Refresh 都重新扫描。

单击文件只改变选择，首次双击即可打开；从资产窗口或其他应用切回主窗口时，激活窗口的点击也计入双击，无需先选中文件或点击空白处。

所有 `.hasset` 默认可见，无需内部资产开关；`.cache`、`.git` 和发布临时状态不展示、不扫描。资产类型来自文件头，扫描不会读取纹理/几何 bulk。Shader 保留文本文件，不出现在 Content Browser，也没有专用编辑器。Import 界面尚待实现。Refresh 会同步更新当前 Game 资产的内存索引和引用选择列表。

切换根目录前验证新目录和资产索引。任意场景或资产页签有未保存修改时，可选择 **Save and switch**、**Discard changes** 或 **Cancel**；未命名场景先选择保存路径，保存完成前 `/Game` 仍指向旧目录。取消、保存失败或新目录无效都会保留当前文档。成功切换时结束旧扫描、加载和渲染工作，释放旧场景、选择、撤销历史与预览，再替换 `/Game` 并进入空文档；`/Engine` 映射保留。重复选择同一规范目录只更新最近使用顺序。

可选启动参数：

| 参数 | 用途 |
|---|---|
| `--asset-root <directory>` | 显式选择 Game 资源目录，优先于保存的根目录 |
| `--engine-content <directory>` | 指定引擎资源目录；开发构建默认使用仓库 Content |
| `--read-only` | 本次启动将 Game 根设为只读 |
| `--scene /Game/Scenes/Sponza.hasset` | 启动后直接打开指定原生场景 |
| `--layout out/editor/Layout.ini` | 布局文件；此路径也是默认值 |
| `--frames 1200 --capture out/editor/Sponza.png` | 运行指定帧数并保存最后一帧 |
| `--benchmark out/editor/Frames.csv --benchmark-warmup 120 --benchmark-samples 300` | 资源就绪后预热并记录帧、场景、GUI 和渲染等待耗时 |
| `--benchmark-camera` | 在采样期间执行确定性的移动相机路径 |
| `--benchmark-collapsed` | 基准测试中默认收起 Outliner 层级，分离展开列表的成本 |

## 独立资产编辑

双击非场景资产会打开独立的 **Hyperion Asset Editor** 桌面窗口，多个资产在该窗口内以页签组织；重复打开同一资产身份会定位已有页签并恢复窗口。主窗口的场景 Viewport、Outliner 和场景 Details 始终独立可用，可以一边观察场景一边编辑资产。资产窗口包含 **Asset Preview** 和 **Asset Properties** 两个可停靠面板，属性不再占用场景 Details。

**Window > Asset Editor** 可重新显示资产窗口。资产窗口是主窗口的非模态附属顶层窗口：点击场景或 Content Browser 时仍显示在主窗口上方，输入焦点留在实际点击的窗口；切换其他应用时遵循正常桌面层级，不全局置顶。两个窗口可分别移动和缩放；单独最小化资产窗口不影响场景，最小化主窗口会同时收起其资产窗口，恢复主窗口时一起恢复。Application Scale 跟随主窗口设置，资产面板布局单独保存到主布局路径加 `.assets.ini` 的文件。此版本不支持资产页签跨原生窗口拖拽或拆出多个资产窗口。

资产属性采用左侧字段名、右侧输入控件表示可编辑字段，灰色信息值表示只读字段。文件名后的 `*` 表示未保存修改。所有资产都有名称、类型、ID、路径、schema、保存版本、依赖与错误信息。

| 类型 | 主展示与只读信息 | 可保存编辑 |
| --- | --- | --- |
| Texture | 2D / Cube 六个面、Mip、RGBA/单通道、棋盘背景、浮点曝光、像素值；尺寸、格式、mip 数和字节数 | 名称；RGBA8 2D 的 Linear/sRGB。切换编码保留 mip0，并原子重建下层 mip；浮点和 Cube 编码只读 |
| Model | 原生材质的 3D 预览；节点层级、稳定 ID、顶点/索引/三角形、属性通道、包围盒和材质槽 | 名称、节点/primitive 名称、节点局部 PRS（保留剪切）、现有 primitive 的材质槽、现有槽的 Material 引用 |
| Sky | 原生天空背景、漫反射/反射参考球；Radiance、Specular、BRDF 产品尺寸/格式、SH9 和 bake convention | 名称；曝光和方向只改变预览 |
| Material | 使用实际 Shader/Pass 的 Sphere / Plane / Cube 预览，支持自定义材质；参数类型、来源、语义和 Shader 路径 | 允许覆盖的标量/向量/颜色、简单固定结构叶值、Texture 引用、Sampler、PBR UV0/UV1；Reset 移除显式覆盖并使用声明默认值 |

Material 的运行时语义、锁定参数、矩阵布局、Shader/Pass 结构和需要协调 Pass 的 AlphaMode / DoubleSided / Unlit 保持只读。必需参数没有声明默认值时，不能清空其唯一显式值。此版本没有材质节点图，也不编辑几何拓扑、层级或 Sky 烘焙结果。

3D 预览复用场景相机导航，**Frame** 重新取景；Texture 支持滚轮缩放、左键平移、**Fit** 和 **1:1**。预览参数、相机和选中节点不进入资产历史。各 3D 页签有独立的场景、视图会话和渲染目标，共享渲染资源服务；隐藏页签停止绘制。

资产窗口中的 **Ctrl+S** 和工具栏 **Save** 保存当前页签；**File > Save All Assets** 保存所有非场景资产。**Ctrl+Z / Ctrl+Y** 或工具栏 Undo/Redo 操作该资产的历史；主窗口的保存和 Undo/Redo 始终操作场景，两者互不抢占输入。一次连续输入/拖动合并为一条历史，Esc 取消当前交互。保存期间可以继续编辑，完成后只把提交时的状态记为已保存。

未保存的草稿只影响自己的预览。保存成功后，当前场景和其他打开的预览自动重新准备受影响依赖；场景名称/变换、选择、局部材质覆盖和 Undo/Redo 历史保留。场景历史恢复时重新绑定最新已发布资源。保存失败保留草稿，依赖刷新失败另行报告。

资产页签关闭支持 **Save and Close / Discard / Cancel**；关闭整个资产窗口支持 **Save All and Close / Discard / Cancel**，不会关闭主场景或退出应用。保存失败保留窗口和草稿。退出应用支持 **Save all and exit / Discard changes / Cancel**；切换资产根目录保护全部文档并关闭旧资产窗口。纹理编码重建期间也计为未保存修改，保存与 Undo/Redo 在重建完成后恢复可用；此时关闭仍需确认，Discard 可明确丢弃正在准备的编辑。保存检查打开时的 ID 和 revision，外部修改或删除、只读挂载不会被静默覆盖；需要关闭并重新打开已变化的资产，解决基线冲突。

## 操作

### 放置对象

**Window > Place Object** 打开放置面板。支持名称搜索，以及 All、Basic、Shapes、Lights 分类；同一种对象可属于多个分类，All 只显示一次。首版提供 Cube、Sphere、Cylinder、Cone、Plane、Directional Light、Point Light 和 Spot Light。已有布局保留，**Window > Reset Layout** 恢复包含左侧放置面板的默认布局。

将条目或灯光缩略图拖入 Viewport：基本形状持续显示三维网格，光源显示图标。优先放到鼠标射线命中的可见模型表面，并按形状边界避免穿入表面；没有命中时先使用 Y=0 平面，再使用相机对焦平面。不自动旋转或吸附网格。CPU 查询沿用静态三角形限制，不复现材质透明裁剪或顶点变形；查询未就绪时不能提交。

在视口内松开创建并选中一个对象，记录为一次 Undo/Redo。Esc、失焦、视口外松开、隐藏视口、打开模态窗口、改变视口尺寸/缩放或切换文档会取消；移出视口但仍按住鼠标只隐藏预览，可以移回继续。预览不进入 Outliner、保存内容、光照、阴影或撤销历史。资源未就绪或失败的条目禁用并显示原因。

基本形状来自 `/Engine/Models/Primitives`，默认材质和依赖也属于 Engine 内容。尺寸为一场景单位，Plane 位于 XZ 平面并朝 +Y。无需先打开场景，可直接在 Untitled 文档放置，然后 **File > Save Scene As...**。空场景没有隐式光照，添加光源前正式网格可能呈黑色；拖拽预览使用独立的明暗着色。

灯光图标始终面向相机，尺寸随界面缩放而保持屏幕大小，叠加显示在场景几何前方。单击图标可选择灯光；gizmo 优先。方向光和聚光灯另画方向箭头。**Viewport options > Show light icons** 控制已有灯光的图标显示，禁用的对象不显示图标。当前只由 MainDirectionalLight 提供方向照明：没有主光时，首个新方向光成为主光；已有主光保持不变，可在 Details 使用 **Set as main directional light** 显式切换并撤销。

注册表 `FObjectPlacementRegistry` 位于 Scene，工厂返回未加入场景的 CPU 节点；分类列表与创建逻辑独立。Gui 提供复制载荷的 `DragSource` / `DropTarget`，Renderer 提供 `FViewportPlacementSession`、射线落点计算和 `FTransientGeometry` 渲染贡献。后续资产浏览器可以复用这些契约；Main 冻结预览快照，`IRenderFeature` 在 tone mapping 前声明颜色/深度依赖并绘制。灯光图标通过现有 GuiRenderer 纹理合成路径绘制。

### 选择与编辑

- 在 Outliner 或 Viewport 选中对象后按 **Del** 删除所有选中对象及其子对象；同时选中父子时只删除一次子树，删除后清空选择。**Ctrl+Z** 撤销删除会恢复全部子树、场景引用、原选择顺序与主对象；重做删除再次清空选择。文本输入、弹窗、放置和操纵拖拽期间不会触发对象删除，长按 Del 不连续删除。

- **左键单击模型**选中最近命中的模型对象，**单击空白处**清除选择。选择同步到 Outliner / Details，不修改场景或撤销历史。按下后移动超过 4 个逻辑像素视为拖动；gizmo 手柄优先，右键导航、失焦、对话框、视口或相机变化会取消本次点击。
- **Ctrl+左键**在 Outliner、模型和灯光图标之间增减选择；再次点击已选对象会取消该对象。最后加入的对象为主对象，拥有唯一的 gizmo；移除主对象后使用最后一个仍选中的对象。Ctrl 点击空白保留选择，普通点击替换整个选择。所有已选 Outliner 行高亮，搜索和折叠不清除选择。
- 在视口中**按住鼠标右键**，使用 **WASD** 前后左右移动、**Q/E** 下降/上升；方向键与 PageUp/PageDown 为别名。单独按移动键不移动镜头，松开右键立即停止平移。
- **右键拖动**在当前位置调整镜头朝向，鼠标向右转头、向下低头；只旋转时世界位置、镜头参数及对焦距离保持不变。
- **按住右键滚动滚轮**调整 WASDQE 移动速度：向上加速、向下减速，每格乘以或除以 1.2。仅调速不会改变镜头位置、朝向或镜头参数；移动中调速立即影响后续平移。
- **不按右键滚动滚轮**沿当前镜头前后方向推拉，保持原有步幅与对焦距离调整，不改变选定的 WASDQE 速度。
- 视口工具栏的 **Viewport options**（滑杆图标）菜单显示 **Camera speed**，单位 **u/s**（场景单位/秒）。初始速度使用编辑器视角的 `max(1, FocusDistance)`，范围为 `0.01–100000 u/s`；失焦、打开对话框或重开场景保留选定速度，退出应用后不保存该设置。
- 复用 Runtime 的 `FSceneCameraController` 飞行模式。SceneViewer 使用默认环绕模式，其他视口宿主也可以显式选择飞行模式。
- **Home** 或工具栏 **Frame Scene** 将镜头对准场景范围。
- **Viewport options > Exposure** 调整视口曝光，不写回资产。
- Outliner 支持搜索和层级选择，一个完整模型实例对应一个模型对象，内部 primitive 不自动成为场景子对象。Details 展示反射属性，合法输入实时更新场景，无需 Apply/Revert；无效中间输入不写入场景。模型整体及 section 材质 override 仍属于实例，不改变共享资产；已有显式展开的场景继续兼容。
- 多选时 Details 只显示所有对象共有的组件。同一属性完全相等时显示数值，否则显示 **Multiple Values**；向量和展开的颜色通道独立判断。显式输入只修改该字段，输入主对象已有的数值也会应用到所有对象。Optional override 单独展示混合存在状态，启用时保留已有 override，并仅为缺失项创建默认值。模型 section 只有在模型资产和 primitive 身份对应时可批量编辑，不对应的集合只读。多选支持 Enabled；名称、增删组件和单对象相机/主光操作仅在单选时提供。
- **Ctrl+Z** 或 **Edit > Undo** 撤销，**Ctrl+Y / Ctrl+Shift+Z** 或 **Edit > Redo** 重做。一次连续输入、拖动或颜色选择器会话合并为一条历史，切换属性、结束输入或关闭选择器后开始新的记录。即使值改回原值，本次操作仍标记为修改，直到撤销回到保存点。连续撤销按操作逐项恢复；撤销后进行新编辑会清除重做分支。Inspector 文本输入也使用文档级撤销，其他文本框保留自身编辑行为。保存、切换对象或场景操作会结束当前编辑；**File > Save Scene / Save Scene As** 保存文档。标题 `*` 表示未保存修改，保存期间的新修改仍保持为脏。
- **Transform (Local)** 分为 Position、Rotation、Scale 三行，分别提供 X/Y/Z 输入。Rotation 使用度数（不显示 deg 后缀），绕固定 X、Y、Z 轴依次旋转；悬停查看具体约定。数值框支持单击输入和按住左键左右拖拽调节；悬停显示左右箭头，拖拽期间隐藏指针，松开恢复。一次拖拽作为一条撤销记录。输入数值即时生效，Enter 或失焦结束本次编辑，Ctrl+Z 恢复本次编辑前的值。原有剪切信息保留，Parent ID 不在属性面板中显示，已有父子关系保持不变。组件菜单支持添加注册的 CPU 组件和移除非必需组件，提交前验证完整场景。
- 多选 **Transform (Local)** 始终绝对赋值，包括在 Details 中拖动数值：例如 Position X 输入 3，会将各对象的局部 X 都设为 3，保留其余分量。父子同时选中时，各自局部值都按输入修改；这与 gizmo 的成组变换语义不同。批量修改先验证全部对象再一次发布，任一目标无效时整次拒绝；一次属性交互对应一条历史，Undo/Redo 恢复各对象各自的值，并保留当前选择。
- 灯光 **Color** 显示颜色预览块，点击打开色轮选择器，选色过程中实时更新场景，Close 或点击外部关闭，Ctrl+Z 撤销整次选色。展开 Color 可输入 0～255 的 R/G/B 分量（sRGB），实时生效。引擎保留线性 RGB 存储，亮度通过 Intensity 调整。
- **Rendering diagnostics (read only)** 显示对应 primitive 的已应用状态和最近 draw。属性来自 Main，诊断来自 Render 发布的只读副本。组件与扩展契约见 [SceneComponents.md](SceneComponents.md)。
- 拖动标签可调整停靠位置；**Window** 菜单可重新显示关闭的面板，**Reset Layout** 恢复默认布局。

键盘与鼠标导航受视口焦点、悬停和拖动状态约束。文本输入、菜单和模态对话框阻止相机操作；失焦、隐藏视口、最小化或切换场景会重置持续输入。退出时保存停靠位置和面板尺寸。面板显示开关在每次启动时恢复默认开启。

## 视口 PRS 操纵组件

在 Outliner 选择对象后，视口的三个图标按钮切换 **Position / Rotation / Scale**：四向箭头表示平移，环形箭头表示旋转，方框与外扩箭头表示缩放。悬停显示用途，当前模式使用蓝色背景。矢量线条随界面缩放保持清晰。工具栏保持单行，提供 PRS、视口选项和视角切换；窄视口可在选项菜单中切换视角。

组件以对象世界原点为中心，X/Y/Z 分别使用红、绿、蓝色，悬停或拖拽的手柄显示黄色。组件在场景图像上叠加显示，不受模型遮挡，大小随界面缩放保持稳定；相机预览模式下停止操纵，但仍可点选模型。

多选时组件位于主对象原点。Position 对所有对象应用相同世界位移；Rotation 围绕主对象原点同时改变其他对象的位置和朝向；Scale 同时缩放其他对象相对主对象的偏移及自身形状。主对象的结果与原有单选操纵保持一致，非均匀父变换下可能产生仿射剪切，保留完整矩阵。父子同时选中时只改写最上层选中根节点，子对象通过继承变换一次。每帧预览都从按下时的快照计算，整组提交、取消和撤销。

点选使用当前 Main 场景的静态三角形几何，遵循父级启用状态、模型和 section 可见性、材质面的剔除规则及镜头近远裁剪面。相机预览使用所选相机；预览不可用或加载中的几何无法确定命中时保留选择。已准备模型可以独立参与查询。该方式不模拟材质透明度/alpha discard、顶点位移和其他 shader 变形，也不重建上一张已呈现图像的场景状态。

- **Position**：拖动箭头沿世界 X/Y/Z 平移；拖动中心方框在相机平面内自由平移。父变换正常时换算回局部坐标，并保留局部线性矩阵。父变换不可逆时显示灰色并禁用平移，避免不唯一的世界到局部映射。
- **Rotation**：拖动彩色圆环绕局部旋转轴转动，保留位置、带符号缩放与剪切。圆环接近侧视时使用按下处的屏幕切线；切线也退化时不启动该手柄。穿过环中心的输入不改变角度。
- **Scale**：拖动方块沿对应局部轴缩放，中心方块同时缩放三个轴。以按下时的缩放值计算比例，允许穿过零点成为镜像；某轴初始值恰为零时按单位灵敏度恢复该轴。拖拽期间固定参考轴，避免跨零点时轴翻转。正对视线、无法定义屏幕拖动方向的轴不显示，可调整视角或使用 Details 数值框。

矩阵仍是变换的唯一持久化数据；镜像和奇异变换沿用 `DecomposeAffine` 的确定性分解约定，负号可能规范化到 X 轴。全零矩阵无法保存独立旋转方向，再次开始编辑时使用确定性补全基。非有限或无法通过场景校验的编辑不写入模型。

多选额外要求主对象父变换及所有选中根节点的父变换可逆；成组 Scale 还要求主对象按下时三个缩放分量非零，否则拒绝启动并显示原因。已开始的合法缩放仍可穿过零点成为负值。零缩放可通过 Details 或单选恢复；成组平移和旋转不需要反演主对象自身的缩放。

一次拖拽松开后形成一条 Undo/Redo 记录，**Esc** 恢复按下前的精确矩阵并保留已有重做分支；无变化的拖拽不产生历史。拖拽可在视口外松开；失焦、最小化、视口隐藏或尺寸变化、切换模式/对象及保存会结束当前拖拽。操纵期间阻止浏览相机输入。

`Runtime/Renderer` 的 `FTransformGizmo` 提供每视口独立的投影几何、命中测试和拖拽计算，不拥有场景、不依赖 GUI 或具体 Editor 插件。宿主传入相机、视口、局部与父变换，消费屏幕坐标绘制数据与新的局部矩阵。Editor 管理选择和文档事务；Gui 的 `DrawImageOverlay` 在当前窗口裁剪范围内绘制，经现有 GuiRenderer/RHI 路径合成。

## 编辑器视角、初始视图与场景相机

**Editor view** 是临时浏览视角，不出现在 Outliner。导航、Frame Scene 和曝光不使场景变脏；普通 Save Scene 不保存浏览位置。打开场景时使用场景的可选 `initialView`，缺省时对可见模型取景，空场景使用稳定的默认视角；不隐式跟随 `defaultCamera`。

视口选项菜单的 **Set initial view** 将当前编辑器视角的位置、朝向与镜头参数写入场景设置。它支持 Undo/Redo，保存后下次打开采用该视图。它没有 Enabled、父对象或 Outliner 节点。导航不会自动更新这个预设。

Camera 组件用于用户明确创作的场景相机。在视口选项菜单点击 **Create camera from view** 创建可撤销的相机对象；它不会自动成为运行时默认相机。选择相机后，**Preview camera** 或视口的视角下拉框可切换为该相机的实时预览。此时导航与 Frame Scene 停用，修改 Transform/Camera 组件时直接看到效果。视口选项菜单的 **Return to editor view** 恢复进入预览前的独立浏览视角。

预览相机或其父对象禁用、相机删除、Camera 组件移除时，视口清空并显示不可用原因，不保留旧相机副本，也不切换到其他相机。**Apply editor view to camera** 把保留的编辑器视角应用到所选相机；有父对象时换算为局部变换，父变换不可逆则拒绝整次修改。该操作同时写入镜头参数，支持 Undo/Redo。当前版本不提供 Pilot、相机画中画或视锥 Gizmo。

## 选中描边

所有选中的模型显示不受场景遮挡影响的橙色外轮廓。视口选项 **Selection outline** 默认使用 **Union**，也可切换 **Per object**；**Smooth outlines (2x)** 提供可选抗锯齿质量。设置不修改文档。选中灯光的已有图标高亮，文件夹、相机及禁用/隐藏对象以原点菱形标记辅助识别；原点须位于镜头裁剪范围内。选择父节点不会自动选择其子节点。多目标效果对比、材质边界和 Renderer 接口见 [选中物体轮廓](SelectionOutlines.md)。

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

- 一个主场景窗口和一个按需创建的原生资产窗口，各自内部可停靠和浮动；未启用跨原生窗口的 ImGui multi-viewport，也不支持将任意面板拖出为新的原生窗口。
- 一个场景视口，默认 Deferred / reversed-Z；此版本不开放 offscreen 深度调试预览。
- Details 支持单选/多选反射编辑、保存与撤销重做，并提供从编辑器视角创建相机的入口；可通过 FGuiPanelEvent 订阅绘制扩展面板；内置文档面板仍属于同一个 Editor 插件。PRS 支持成组操纵，Del 支持删除选中子树，Place Object 支持内置形状和三种灯光；尚无 Play、框选、范围选择、自定义轴心或通用资产选择器。任意资产的模型替换和实例创建使用 Runtime 或导入工具。
- 界面使用内置 Roboto 字体和英文标签。字体 atlas 在启动时生成；中文字符显示与动态字体更新尚未加入。

## 验证入口

`editor_asset_editors` 覆盖各类资产预览、属性保存和独立历史，以及场景与资产原生窗口同时渲染、窗口内快捷键路由、缩放、资产最小化、主窗口整组最小化/恢复、关闭取消/丢弃/保存失败重试和窗口重建。`gui_docking` 补充两个 GUI context 的布局与 framebuffer 缩放隔离；`window_event_routing` 验证原生窗口事件路由。`window_ownership` 会短暂创建可见原生窗口，验证激活主窗口后的层级/焦点、非全局置顶、最小化/恢复、关闭隔离与重建。

`editor_multiselect` 使用真实编辑器输入验证 Outliner/Viewport Ctrl 增减选择、按下时的 Ctrl 状态、双目标描边、Details 混合值逐轴赋值、输入主对象已有数值、三个 gizmo 模式和整组 Undo/Redo，并检查父子选择、取消预览、无效批量提交、删除恢复及句柄重映射。独立入口为 `--exercise-multiselect`。`scene_management` 验证原子批量编辑/增删、最终层级校验及混合反射值；`transform_gizmo` 补充非均匀父级、剪切、跨零缩放和零缩放旋转的成组运算；`gui_input_and_data` 验证混合颜色逐通道编辑。

`object_placement` 覆盖分类、基本形状几何与放置计算；`gui_input_and_data` 覆盖复制载荷、跨面板预览/交付和取消；`scene_runtime_instance` 覆盖空场景动态注册、去重、保存重载和加载失败隔离。`editor_placement` 使用真实 GUI 输入依次放置八种对象，检查连续预览、八种取消路径、撤销重做、主方向光、图标拾取/隐藏、空文档 Save As 与重载，并验证缺失资源只禁用对应条目。可单独运行 `--exercise-placement OUTPUT.hasset`，截图随输出场景保存在同一目录。

`gui_docking` 验证布局保存恢复和纹理 ID；`gui_texture_rendering` 验证真实 RHI 离屏 sRGB 输出、GUI 采样和无效绑定拒绝。`scene_viewer_controls` 覆盖共享控制器的默认环绕模式、飞行模式的按键门槛、固定位置旋转、滚轮调速/推拉分流、速度与位移一致性、速度边界及输入中断恢复。`editor_acceptance` 使用真实控件位置产生 Platform 格式的输入事件，覆盖菜单打开 Sponza、右键移动门槛、松开右键停止、原地旋转、右键滚轮调速、普通滚轮推拉、模态输入隔离、视口隐藏与恢复、窗口缩放、场景重开、加载错误恢复和加载中退出。GPU 验收需要 D3D12 环境及挂载的 Sponza 资产。

文档验收通过 `--exercise-document OUTPUT.hasset` 编辑反射属性，验证 Enter 前实时生效、按属性合并历史、连续 Ctrl+Z/Redo、改回原值仍保留修改记录、重做分支截断、Render 已应用状态、异步保存期间的新修改、重载、无效/过期提交拒绝、只读挂载保存失败和独立视口相机。`--report` 输出验收结果及保存耗时；输出场景写到指定测试目录。

文档验收还使用真实键盘输入编辑 Position X、Rotation Z 和 Scale Y，检查度数到矩阵的转换、精确 Undo/Redo 及保存重载。

`--exercise-views OUTPUT.hasset` 验证初始视图按钮、相机创建与世代变化、父级坐标换算、不可逆父级拒绝、应用视角、预览失效清屏、返回原编辑器视角以及保存重开；同样纳入 `editor_acceptance`。

`transform_gizmo` 是无 GPU 的几何与变换回归，覆盖自由/轴向平移、带剪切父变换、局部旋转、剪切对象整体比例缩放、镜像、零缩放与恢复、视线退化及非有限输入。`--exercise-gizmo` 通过真实 GUI 事件验证三个工具栏按钮、实时拖拽、零轴恢复、历史合并、Esc 取消与重做分支保留，以及失焦后提交最后有效预览和撤销重做，同样纳入 `editor_acceptance`。

`tools/ComponentEditorBenchmark.py` 可重复运行 Debug/Release、Editor/Viewer、静止/移动相机组合，保留逐帧 CSV、日志、程序哈希和进程内存采样。Viewer 的流水线吞吐耗时与 Editor 的逐帧等待耗时应分别比较。本次组件化的测量条件、回退修正与结果见 [性能对比报告](ComponentEditorPerformance.md)。
