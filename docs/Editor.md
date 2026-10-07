# 场景与资产编辑器

`hyperion_editor` 是独立的 Windows / D3D12 应用。提供 UE 风格的深色可停靠工作区：顶部菜单与工具栏、左侧 Place Object、中央 Viewport、右侧 Outliner / Details、底部 Content Browser 和状态栏。

原生标题栏默认使用黑色背景和浅色文字，由 Platform 封装 Windows DWM 设置。精确标题栏颜色需要 Windows 11；不支持时保留系统可用的外观，不影响编辑器启动。

## Log 窗口

引擎在关键操作边界记录插件启动/清理、内容根切换与恢复、导入及保存结果、场景依赖加载和刷新失败。日志包含相关路径、标识及失败原因；普通逐帧更新与重复状态轮询不产生记录。实际着色器编译的 warning/error 带有文件、入口、目标和宏定义；缓存命中不重播。D3D12 在读取设备统计时报告校验 Warning/Error/Corruption，同一设备的相同等级、消息 ID 和正文只记录一次，错误统计仍保留全部存储消息的计数。

Editor 启动不再自动打开命令行窗口。通过 **Window > Log** 开关日志面板；默认隐藏，首次打开时与底部 Content Browser 共用停靠节点。可拖动页签重新停靠或浮动，位置随现有布局保存；Reset Layout 恢复底部位置。升级旧布局时只补充 Log，不重置其他窗口。

面板显示本次进程从日志初始化起的输出，包括窗口创建前的启动日志和关闭面板期间的记录。Error 为红色、Warning 为黄色、Info 和 Debug 为白色。正文保留原始路径；多行消息按行显示，超长行按 UTF-8 边界分段。位于底部时跟随新输出，向上浏览时保留当前位置。正文按可见行从临时日志读取，不因显示缓存容量而删除历史。

结构化引擎日志保留等级；直接写入 stdout/stderr 的文字分别按 Info/Error 显示，并标记来源。原始标准流不含可靠的日志等级，其线程字段表示采集线程。尚未换行的短片段在换行或正常退出排空时记录。采集不包括其他进程、OS 调试流，以及入口初始化之前的输出。持久诊断仍写入 `out/logs/editor.log`，窗口不会混入之前运行的内容；临时会话文件正常退出时删除。

从脚本启动仍支持 stdout/stderr 重定向、现有参数和退出码。交互式致命错误提供原生提示；隐藏、重定向或有限帧运行不会等待错误弹窗。AssetTool 和 Automation CLI 保留命令行程序行为。附着 agent 可通过 `application.log.read` 分页读取同一份日志，见 [自动化能力](AutomationCapabilities.md)。

## 外部资产导入

使用 **File > Import Asset...** 打开非模态导入面板。Source、Conversion settings、Output、Asset properties 和 Import options and actions 各自可展开或折叠。选择源文件后自动准备属性；手工输入路径时，在离开 Source 输入框后准备。必须先通过 File > Open... 选择可写 Game 根。

- 模型：glTF/GLB，自动发布材质和图片依赖；可创建包含模型的场景。
- 贴图：PNG/JPG/JPEG，默认 sRGB，可选 Linear；生成 RGBA8 Texture2D 和完整 mip 链。
- 天空：2:1 HDR/EXR 全景图，可调整 radiance 面尺寸、specular 面尺寸和采样数，默认 256/64/256。尺寸不符合要求时显示错误。

不接受自有资产 JSON 或原生 `.hasset` 导入。已有原生资产通过对应资产编辑器打开、编辑和保存。

Output 的 **Save as** 显示自动计算的目标文件夹。Browse 选择 `/Game` 内的父目录，并记住上次选择；问号提示显示 `/Game` 对应的本地目录。所有本次生成的资产放入以来源文件名命名的文件夹，其他内容占用名称时使用数字后缀；相同来源和输出位置重复导入复用原文件夹。已有资产不自动移动。附属文件使用可读名称和数字序号，资产内部 ID 及引用仍由共享发布服务维护。

Advanced options 只提供 Force reimport，跳过增量检测；输出身份、依赖冲突和只读校验继续生效。相同输入、设置和有效输出保持零写入。Import 执行共享校验、准备快照检查与发布，结束后弹出成功或失败原因；索引刷新失败会提示文件已导入及相应警告。关闭面板不会取消已接受导入；导入期间换根返回 busy，正常退出排空后台工作。

**Asset properties** 在导入面板内显示转换结果；此版本没有 GPU 图像或三维渲染预览。不可修改的属性使用只读控件。

| 类型 | 可检查 | 可修改 |
| --- | --- | --- |
| 模型 | 节点层级、mesh section、材质槽、顶点/三角形数量、Bounds min/max | 根名称、节点名称及局部位置/旋转/缩放、section 名称及已有材质槽 |
| 贴图 | 尺寸、格式、编码、mip 数、像素字节数 | 名称；颜色编码在 Conversion settings 调整 |
| 天空 | 全景图宽度和高度 | 名称；烘焙参数在 Conversion settings 调整 |
| 模型生成的场景 | 节点与相机/灯光摘要、依赖 | 场景结构只读 |

节点变换保留原矩阵剪切；稳定 ID、几何/层级和生成的依赖保持只读。Undo / Redo / Reset to source 只修改属性草稿，不写磁盘，也不进入场景历史。源文件、输出或转换设置变化时自动重新准备；有未发布的属性修改时先确认是否重置。明确重选同一路径可重读外部更新或已修复的源文件；发布失败后重新准备一次，不会自动再次导入。

编辑过的未发布草稿参与换根和关闭保护。须先 Import，或明确丢弃；Save and Exit 不会暗中发布导入。准备/发布期间不可换根或修改同一草稿；正常退出排空后台工作。最多保留四份草稿，每份最多 64 步历史。

automation 使用 `asset.import.draft.prepare/get/edit/history/submit/discard` 与 `asset.import.drafts` 映射草稿操作；`asset.import` 单步入口及校验/任务查询保持兼容。GUI 和 automation 共享领域校验、属性编辑、历史与发布。原生文件选择器、目录偏好及窗口显隐属于呈现适配，见 [Automation 能力清单](AutomationCapabilities.md)。

## 天空光

天空光是场景组件。从 **Window > Place Object** 拖入 **Sky Light**，创建引用 Engine 默认 Cloudy 天空的 SkyAsset 天空光，一次 Undo/Redo。组件中的 **Priority** 为整数，默认 0、可为负数，越大越优先。有效启用的天空光中仅最高优先级生效，统一提供背景与环境照明；同优先级以持久对象 ID 字典序决定，最高候选的 Priority 显示 `[!]` 并可悬停查看原因。禁用、删除、撤销和修改父节点启用状态都会自动重新选择。复制保留 Priority，因此可能产生同优先级候选。

不再显示常驻的 Active/Ready 文本或激活按钮。Priority 的提示可查看当前生效或被覆盖的原因；Sky asset 属性仅在加载、上传或失败时附加状态，错误提示包含依赖或上传失败原因。资产就绪状态不参与优先级选择；高优先级资源失败时不会切换到低优先级天空光。Intensity 为 0 仍占用选择，关闭 Show background 仍保留 IBL。

Details 只显示当前 **Source** 适用的字段，两种模式共用 **Intensity**。Constant color 显示 **Color**；Sky asset 显示 **Sky asset** 选择器、**Tint**、**Yaw (degrees)** 和 **Show background**。选择器列出已索引的天空资产，也接受从 Content Browser 拖入的天空资产，拒绝其他类型。Tint 按通道乘以背景、漫反射 SH 与镜面 IBL，不影响直接光、局部光和自发光，修改 Tint、Yaw 或 Intensity 不会重新加载天空。隐藏字段保留原值，切回模式后恢复。Details 同时显示天空资产加载状态。详见 [天空与 IBL](SkyLighting.md)。

Model 和 Material 的 Asset Editor 临时预览默认显示内置 Cloudy 天空并使用其环境照明，无需选择 Game 目录；这不会修改资产或主场景。天空及纹理位于 Engine Content，Sky 类型资产仍预览它自身的天空。

## Editor preference 与抓帧

通过 **Edit > Editor preference** 打开偏好弹窗。首个选项 **Enable RenderDoc capture** 默认关闭，修改后立即保存到 应用 `Config/Preferences.ini`（见 [存储路径](Storage.md)），下次启动保留；`--editor-preferences <path>` 可指定独立的本地偏好文件。该文件与场景、布局及界面缩放配置分开，保存失败会在弹窗中显示错误并提供重试。

需要通过 `tools/Build.ps1 -RenderDoc` 或 `tools/GenerateSolution.ps1 -RenderDoc` 编入 RenderDoc 支持，并安装 RenderDoc。首次启用后重启编辑器，使 hooks 在图形设备创建之前加载。开启偏好后，Viewport 工具栏右侧显示相机形抓帧按钮；点击会捕获包含场景、GUI 和 Present 的完整帧，保存到 `out/captures`，并启动 RenderDoc 打开本次生成的 RDC。抓帧和自动打开由公共 Capture 服务管理。

关闭偏好立即隐藏按钮；插件 hooks 在退出前保留，不会运行中卸载。未编入支持、运行库不可用、尚未重启或显式传入 `--disable-plugin renderdoc` 时，按钮禁用，悬停提示及偏好弹窗显示原因，其他编辑功能仍可用。抓帧或打开失败也可在这些位置查看状态。

**Show RenderDoc HUD** 独立控制左上角 RenderDoc 叠加文字，默认关闭。修改后立即保存；运行库已加载时无需重启，下次启动也会恢复。隐藏 HUD 不影响抓帧、回放或 RenderDoc 快捷键；设置作用于当前进程的所有 RenderDoc 窗口。尚未加载运行库时也可预先保存，但不会因此启用抓帧插件。旧偏好文件缺少 `renderdoc_hud` 时默认隐藏；保存失败保留原设置，再次勾选可重试。

## 界面缩放

**Window > Application Scale** 统一调整文字、控件、间距、工具栏和状态栏，下一帧生效。默认 125%，支持 100% / 125% / 150% / 175% / 200% 预设、自定义数值及恢复默认。左右拖动 Custom 数值可实时调整，Ctrl+单击可直接输入倍率；鼠标停下后倍率保持不变。字体按目标字号重新生成，停靠分区比例、场景数据和相机状态保持不变。放大后面板可显示的内容减少，可使用滚动或调整停靠分区。

正常退出时偏好保存到 应用 `Config/UiScale.ini`，独立于场景和 `Layout.ini`。`--ui-scale 1.5` 覆盖启动倍率，`--ui-preferences <path>` 指定偏好文件；有效范围为 1–2。缺失或损坏的偏好回退到 125%。验收和 benchmark 运行不读写用户偏好。

Application Scale 不调整原生标题栏、场景镜头或渲染分辨率；framebuffer 像素密度仍独立处理。这不是自动的跨显示器 DPI 布局策略。

## 构建与打开 Sponza

使用统一的依赖和内容挂载。检出同级 HyperionAssets 并执行 `git lfs pull`；挂载与内容准备见 [ContentFileSystem.md](ContentFileSystem.md)。从引擎仓库根目录运行：

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

所有 `.hasset` 默认可见，无需内部资产开关；`.cache`、`.git` 和发布临时状态不展示、不扫描。资产类型来自文件头，扫描不会读取纹理/几何 bulk。Shader 保留文本文件，不出现在 Content Browser，也没有专用编辑器。外部导入入口见 File > Import Asset。Refresh 会同步更新当前 Game 资产的内存索引和引用选择列表。

切换根目录前验证新目录和资产索引。任意场景或资产页签有未保存修改时，可选择 **Save and switch**、**Discard changes** 或 **Cancel**；未命名场景先选择保存路径，保存完成前 `/Game` 仍指向旧目录。取消、保存失败或新目录无效都会保留当前文档。成功切换时结束旧扫描、加载和渲染工作，释放旧场景、选择、撤销历史与预览，再替换 `/Game` 并进入空文档；`/Engine` 映射保留。重复选择同一规范目录只更新最近使用顺序。

可选启动参数：

| 参数 | 用途 |
|---|---|
| `--asset-root <directory>` | 显式选择 Game 资源目录，优先于保存的根目录 |
| `--engine-content <directory>` | 指定引擎资源目录；开发构建默认使用仓库 Content |
| `--read-only` | 本次启动将 Game 根设为只读 |
| `--scene /Game/Scenes/Sponza.hasset` | 启动后直接打开指定原生场景 |
| `--layout out/tests/Editor/Layout.ini` | 显式布局文件；默认是应用 `State/Layout.ini` |
| `--frames 1200 --capture out/tests/Editor/Sponza.png` | 运行指定帧数并保存最后一帧 |
| `--benchmark out/tests/Editor/Frames.csv --benchmark-warmup 120 --benchmark-samples 300` | 资源就绪后预热并记录帧、场景、GUI 和渲染等待耗时 |
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

### 复制粘贴对象

场景 Viewport、Outliner 或 Details 获得键盘焦点且未编辑文本时，`Ctrl+C` 复制有序选择及完整子树，`Ctrl+V` 粘贴。父子同时选中只复制一次；副本选择保持原显式选择顺序和主选对象。名称使用场景内不重名的数字后缀，例如 `Light (1)`、`Light (2)`；其他编辑属性、局部矩阵和内部层级保持复制时的值，不添加位置偏移。

从 Place Object 拖放对象到 Viewport 成功后，新对象被选中，键盘焦点转到 Viewport，可直接按 `Ctrl+C`、`Ctrl+V`，无需额外点击。

副本放在复制时的原父级下，继承其当前世界状态；外部父级或引用已删除则整批拒绝。复制后编辑或删除被复制对象不改变快照。模型、纹理、材质和天空资产继续共享，对象材质覆盖独立。复制相机不更改场景默认相机或预览相机；灯光保留 Priority 和其他组件属性，复制方向光会叠加直接照明，复制天空光或符合阴影条件的方向光可能产生同优先级候选，并按持久对象 ID 字典序重新确定生效来源。

每次粘贴是一个 Undo/Redo 事务。Undo 恢复之前的选择，Redo 从历史恢复而不读取剪贴板。复制不改变 dirty、历史或 redo，保存仍需显式执行。

系统剪贴板决定当前可粘贴内容。在其他应用或 Editor 文本框复制文字、图片、文件后，旧对象不可再粘贴；文本框中的 Ctrl+C/Ctrl+V 始终编辑文字。Content Browser 和资产窗口不触发主场景粘贴；拖拽、gizmo、放置和模态操作期间快捷键暂停。长按不会重复创建对象。

当前类型化剪贴板 provider 支持 Windows，范围为同一 Editor 实例、同一打开的场景文档。关闭、重开、替换场景或切换内容根释放快照；同一文档保存不使其失效。快照最多 16,384 个节点、64 MiB 计入预算的编辑数据（共享不可变资源不计入）。未注册组件、未声明安全复制契约的扩展组件、无法完整读取的材质选择和剪贴板访问失败会明确报错，不会部分粘贴或回退到旧缓存。

### 放置对象

**Window > Place Object** 打开放置面板。支持名称搜索，以及 All、Basic、Shapes、Lights 分类；同一种对象可属于多个分类，All 只显示一次。首版提供 Cube、Sphere、Cylinder、Cone、Plane、Directional Light、Point Light、Spot Light 和 Sky Light。已有布局保留，**Window > Reset Layout** 恢复包含左侧放置面板的默认布局。

将条目或灯光缩略图拖入 Viewport：基本形状持续显示三维网格，光源显示图标。优先放到鼠标射线命中的可见模型表面，并按形状边界避免穿入表面；没有命中时先使用 Y=0 平面，再使用相机对焦平面。不自动旋转或吸附网格。CPU 查询沿用静态三角形限制，不复现材质透明裁剪或顶点变形；查询未就绪时不能提交。

在视口内松开创建并选中一个对象，记录为一次 Undo/Redo。Esc、失焦、视口外松开、隐藏视口、打开模态窗口、改变视口尺寸/缩放或切换文档会取消；移出视口但仍按住鼠标只隐藏预览，可以移回继续。预览不进入 Outliner、保存内容、阴影贴图投射或撤销历史。资源未就绪或失败的条目禁用并显示原因。

基本形状来自 `/Engine/Models/Primitives`，默认材质和依赖也属于 Engine 内容。尺寸为一场景单位，Plane 位于 XZ 平面并朝 +Y。无需先打开场景，可直接在 Untitled 文档放置，然后 **File > Save Scene As...**。空场景没有隐式光照，添加光源前正式网格可能呈黑色；拖拽预览使用独立的明暗着色。

Content Browser 中的 Model `.hasset` 同样可拖入 Viewport，使用相同落点、预览与取消规则。首次拖入会异步准备模型并显示状态，出现三维预览后松手才会创建对象；提前松手不会在加载完成后补建。纹理、材质、天空、场景和损坏文件不能作为模型放置，拖到 Details 的类型化资产选择器仍沿用原有规则。每次成功放置创建一个以文件名（不含扩展名）命名的根节点，保留模型内部实例变换、原始尺寸及材质；重复放置共享资产数据。加载缓存可保留到场景关闭，但取消的预览不会进入场景保存内容。

灯光图标始终面向相机，尺寸随界面缩放而保持屏幕大小，叠加显示在场景几何前方。单击图标可选择灯光；gizmo 优先。方向光和聚光灯另画方向箭头。**Viewport options > Show light icons** 控制已有灯光的图标显示，禁用的对象不显示图标。所有有效启用的方向光叠加照明；在开启 Cast shadows 且辐射非零的方向光中，以 **Priority** 最高者作为 CSM 和 contact shadows 的共同光源，各阴影方法仍遵守自己的开关。其余方向光不产生阴影。Priority 的同值处理、编辑、撤销和保存与天空光一致。`(?)` 提示以绿色显示当前阴影来源，以红色显示被哪盏方向光覆盖，并说明其他候选仍参与直接照明、可提高 Priority 争取阴影来源；不符合阴影候选条件的灯光显示排除原因。同最高 Priority 的 `[!]` 有独立悬停说明，指出多个符合阴影条件的方向光并列最高优先级，不展示对象 ID 或底层决胜规则。天空光图标位于节点原点，只用于选择，其位置和朝向不影响光照；天空光规则见[天空光](#天空光)。

Content Browser 模型预览按分部优先使用已就绪的原材质，保留颜色、纹理、遮罩和透明效果，并使用当前场景光照；屏幕空间效果沿用包含预览表面的视口深度。几何就绪但材质仍在编译或上传时，该分部暂用默认明暗材质，就绪后自动切换；已有缓存时可从首帧使用原材质。松手后，尚未就绪的分部继续保留临时预览，并在正式模型就绪时完成交接；交接期间每个分部只绘制一次，避免透明叠加或短暂消失。空场景无光照时，受光材质可能与正式模型一样呈黑色。Place Objects 的基本形状继续使用独立明暗预览。

注册表 `FObjectPlacementRegistry` 位于 Scene，工厂返回未加入场景的 CPU 节点；分类列表与创建逻辑独立。Editor 的 `FPlacementService` 将预设和模型引用转为拥有数据的候选，共享资源准备和文档提交；GUI 与 `IScenePlacement` 自动化入口使用同一实现。Gui 提供复制载荷的 `DragSource` / `DropTarget`，Renderer 提供 `FViewportPlacementSession`、射线落点计算和 `FTransientGeometry` 渲染贡献。Main 冻结预览快照：原材质分部合入普通非阴影视图，与场景物体统一选择 pass 和排序；默认明暗分部由 `IRenderFeature` 在 tone mapping 前绘制。两者均不创建持久场景或空间索引注册。灯光图标通过现有 GuiRenderer 纹理合成路径绘制。

### 选择与编辑

- 在 Outliner 或 Viewport 选中对象后按 **Del** 删除所有选中对象及其子对象；同时选中父子时只删除一次子树，删除后清空选择。**Ctrl+Z** 撤销删除会恢复全部子树、场景引用、原选择顺序与主对象；重做删除再次清空选择。文本输入、弹窗、放置和操纵拖拽期间不会触发对象删除，长按 Del 不连续删除。

- **左键单击模型**选中最近命中的模型对象，**单击空白处**清除选择。选择同步到 Outliner / Details，不修改场景或撤销历史。按下后移动超过 4 个逻辑像素视为拖动；gizmo 手柄优先，右键导航、失焦、对话框、视口或相机变化会取消本次点击。
- **Ctrl+左键**在 Outliner、模型和灯光图标之间增减选择；再次点击已选对象会取消该对象。最后加入的对象为主对象，拥有唯一的 gizmo；移除主对象后使用最后一个仍选中的对象。Ctrl 点击空白保留选择，普通点击替换整个选择。所有已选 Outliner 行高亮，搜索和折叠不清除选择。
- **Ctrl+A** 在 Viewport 或 Outliner 获得焦点时选中当前场景的全部节点，包括折叠、搜索排除、隐藏和停用节点，保留已有主对象。文本框中的 Ctrl+A 仍全选文字；Details、Content Browser、资产窗口、弹窗和导航/操纵/放置/层级拖拽期间不触发场景全选，长按不重复执行。
- Outliner 的 **Shift+左键**选择锚点至当前行的全部显示节点，包含两端并替换原选择；**Ctrl+Shift+左键**将区间追加到现有选择，不取消已选区间成员。普通或 Ctrl 点击建立锚点，连续 Shift 点击保留锚点，终点为主对象。区间包含滚动区域外的行，跳过折叠子节点和搜索排除的节点；无有效锚点时使用列表中的当前主对象，否则仅选当前行。过滤、场景/历史和外部选择变化会重置锚点。
- Viewport 的 **Shift+左键**与 Ctrl+左键相同，切换模型或灯光图标的选中状态；修饰键按鼠标按下时捕获，松开 Shift 后再松开鼠标仍执行原动作。Shift 点击空白或查询不可用时保留选择。上述选择操作均不改变场景 revision、dirty 或历史。
- 在视口中**按住鼠标右键**，使用 **WASD** 前后左右移动、**Q/E** 下降/上升；方向键与 PageUp/PageDown 为别名。单独按移动键不移动镜头，松开右键立即停止平移。
- **右键拖动**在当前位置调整镜头朝向，鼠标向右转头、向下低头；只旋转时世界位置、镜头参数及对焦距离保持不变。
- **按住右键滚动滚轮**调整 WASDQE 移动速度：向上加速、向下减速，每格乘以或除以 1.2。仅调速不会改变镜头位置、朝向或镜头参数；移动中调速立即影响后续平移。
- **不按右键滚动滚轮**沿当前镜头前后方向推拉，保持原有步幅与对焦距离调整，不改变选定的 WASDQE 速度。
- 视口工具栏的 **Viewport options**（滑杆图标）菜单显示 **Camera speed**，单位 **u/s**（场景单位/秒）。初始速度使用编辑器视角的 `max(1, FocusDistance)`，范围为 `0.01–100000 u/s`；失焦、打开对话框或重开场景保留选定速度，退出应用后不保存该设置。
- 复用 Runtime 的 `FSceneCameraController` 飞行模式。共享控制器也支持环绕模式。
- **F** 将浏览镜头对准当前选择。场景点击和 Outliner 选择共用同一操作；多选使用所有选中子树世界包围盒的合并中心，不受主对象和选择顺序影响。保留当前朝向与 FOV，按视口比例调整距离和临时近裁面；远裁距离按需扩大，保留已有范围，避免聚焦小对象后裁掉远处模型。Group 使用后代范围；灯光、相机和空 Group 使用世界位置与半边长 0.5 场景单位的稳定范围，不使用灯光照射半径。
- **F** 在 Viewport、Outliner 或 Details 获得焦点时生效；文本输入、弹窗、拖拽、右键导航和失焦期间停用。无选择时不移动；按住或配合修饰键不重复取景。隐藏或停用的选中模型仍使用可用几何范围，已有 SourceNode/SourcePrimitive 和隐藏 section 的边界口径保持一致。
- **Home** 在浏览视口将镜头对准全场景范围，并重新拟合临时近裁面、按需扩大远裁距离；先用 F 聚焦小物体再按 Home 仍能看到整个场景。主工具栏不再显示单独的 Frame Scene 按钮。
- 视口工具栏的 **Exposure** 调整曝光，**Visualizer** 下拉菜单提供 Lit 和已有六种 GBuffer 模式；GBuffer 模式仅支持 Deferred，不写回场景资产。
- Outliner 支持搜索和层级选择，一个完整模型实例对应一个模型对象，内部 primitive 不自动成为场景子对象。Details 展示反射属性，合法输入实时更新场景，无需 Apply/Revert；无效中间输入不写入场景。模型整体及 section 材质 override 仍属于实例，不改变共享资产；已有显式展开的场景继续兼容。
- Details 隐藏内部 Object ID；所有组件展开后的内容统一缩进。单选时，非必需组件标题栏右侧的 **×** 移除该组件，悬停提示组件名称，折叠时也可操作；Transform 等必需组件不显示移除按钮。移除支持 Undo/Redo。
- **Object enabled** 控制对象及其子树的实际启用状态；悬停帮助说明停用对象也会停用其子对象。自身勾选但因父对象停用而未生效时，Details 显示原因，多选时显示受影响对象数量。修改此开关支持 Undo/Redo 并随场景保存。
- 多选时 Details 只显示所有对象共有的组件。同一属性完全相等时显示数值，否则显示 **Multiple Values**；向量和展开的颜色通道独立判断。显式输入只修改该字段，输入主对象已有的数值也会应用到所有对象。Optional override 单独展示混合存在状态，启用时保留已有 override，并仅为缺失项创建默认值。模型 section 只有在模型资产和 primitive 身份对应时可批量编辑，不对应的集合只读。多选支持 Object enabled 和灯光 Priority；名称、增删组件和单对象相机操作仅在单选时提供。多选的 Source 混合时，隐藏依赖该字段的属性。
- **Ctrl+Z** 或 **Edit > Undo** 撤销，**Ctrl+Y / Ctrl+Shift+Z** 或 **Edit > Redo** 重做。一次连续输入、拖动或颜色选择器会话合并为一条历史，切换属性、结束输入或关闭选择器后开始新的记录。即使值改回原值，本次操作仍标记为修改，直到撤销回到保存点。连续撤销按操作逐项恢复；撤销后进行新编辑会清除重做分支。Inspector 文本输入也使用文档级撤销，其他文本框保留自身编辑行为。保存、切换对象或场景操作会结束当前编辑；**File > Save Scene / Save Scene As** 保存文档。标题 `*` 表示未保存修改，保存期间的新修改仍保持为脏。
- **Transform (Local)** 分为 Position、Rotation、Scale 三行，分别提供 X/Y/Z 输入。Rotation 使用度数（不显示 deg 后缀），绕固定 X、Y、Z 轴依次旋转；悬停查看具体约定。数值框支持单击输入和按住左键左右拖拽调节；悬停显示左右箭头，拖拽期间隐藏指针，松开恢复。一次拖拽作为一条撤销记录。输入数值即时生效，Enter 或失焦结束本次编辑，Ctrl+Z 恢复本次编辑前的值。原有剪切信息保留，Parent ID 不在属性面板中显示，已有父子关系保持不变。Add component 菜单支持添加注册的 CPU 组件，组件标题栏支持移除非必需组件，提交前验证完整场景。
- 多选 **Transform (Local)** 始终绝对赋值，包括在 Details 中拖动数值：例如 Position X 输入 3，会将各对象的局部 X 都设为 3，保留其余分量。父子同时选中时，各自局部值都按输入修改；这与 gizmo 的成组变换语义不同。批量修改先验证全部对象再一次发布，任一目标无效时整次拒绝；一次属性交互对应一条历史，Undo/Redo 恢复各对象各自的值，并保留当前选择。
- 灯光 **Color** 显示颜色预览块，点击打开色轮选择器，选色过程中实时更新场景，Close 或点击外部关闭，Ctrl+Z 撤销整次选色。展开 Color 可输入 0～255 的 R/G/B 分量（sRGB），实时生效。引擎保留线性 RGB 存储，亮度通过 Intensity 调整。
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

**Editor view** 是临时浏览视角，不出现在 Outliner。导航、F/Home 取景和曝光不使场景变脏，也不修改场景 revision 或撤销历史；普通 Save Scene 不保存浏览位置。打开场景时使用场景的可选 `initialView`，缺省时对可见模型取景，空场景使用稳定的默认视角；不隐式跟随 `defaultCamera`。

视口选项菜单的 **Set initial view** 将当前编辑器视角的位置、朝向与镜头参数写入场景设置。它支持 Undo/Redo，保存后下次打开采用该视图。它没有 Enabled、父对象或 Outliner 节点。导航不会自动更新这个预设。

Camera 组件用于用户明确创作的场景相机。在视口选项菜单点击 **Create camera from view** 创建可撤销的相机对象；它不会自动成为运行时默认相机。选择相机后，**Preview camera** 或视口的视角下拉框可切换为该相机的实时预览。此时导航与 F/Home 取景停用，修改 Transform/Camera 组件时直接看到效果。视口选项菜单的 **Return to editor view** 恢复进入预览前的独立浏览视角。

预览相机或其父对象禁用、相机删除、Camera 组件移除时，视口清空并显示不可用原因，不保留旧相机副本，也不切换到其他相机。**Apply editor view to camera** 把保留的编辑器视角应用到所选相机；有父对象时换算为局部变换，父变换不可逆则拒绝整次修改。该操作同时写入镜头参数，支持 Undo/Redo。当前版本不提供 Pilot、相机画中画或视锥 Gizmo。

## 选中描边

所有选中的模型显示不受场景遮挡影响的橙色外轮廓。视口选项 **Selection outline** 默认使用 **Union**，也可切换 **Per object**；**Smooth outlines (2x)** 提供可选抗锯齿质量。设置不修改文档。选中灯光的已有图标高亮，文件夹、相机及禁用/隐藏对象以原点菱形标记辅助识别；原点须位于镜头裁剪范围内。选择父节点不会自动选择其子节点。多目标效果对比、材质边界和 Renderer 接口见 [选中物体轮廓](SelectionOutlines.md)。

## 模块与帧顺序

Editor 私有 `FEditorInteractionFacts` 集中采集当前模态、过渡、手势、文本和视口事实，`FEditorInteractionPolicy` 分别决定导航、拾取、放置、Gizmo、层级手势、快捷键、文档和附加窗口的准入。各入口即时查询，能看到同帧稍早发生的状态变化；不缓存成一个全局 busy。普通 popup 阻断快捷键但自身不构成文档 busy；等待层级点击/拖拽判定会阻断导航，只有已经开始的层级拖拽阻断 Gizmo；保存关闭继续阻断附加窗口。各入口保留自己的取消、焦点恢复和事务收尾，SceneEditing 的 document/revision/idle 校验仍为最终修改依据。

放置目录用可选分类 ID 表达过滤：无值表示全部，有值精确匹配开放字符串类别。面板的 **All** 只是无过滤按钮的标签，真实类别可以拥有同样文字，控件身份分别定义。关键词搜索和多类别对象的唯一返回、注册顺序保持一致。

Editor 插件保留功能生命周期与 typed service 注册。私有 `FEditorViewport` 拥有浏览相机、视口目标、尺寸和区域，执行初始化、resize、取景和导航重置。`FEditorAcceptanceDriver` 是不暴露场景状态的私有边界：只有 BUILD_TESTING 启用且请求验收时，才创建 `FEditorAcceptanceHarness`，由它拥有场景方法、类型化状态、各场景的输入与观测上下文、控件快照、夹具判断和完成/超时检查。场景通过 `TransitionTo` 显式跳转，输入阶段、等待预算与一次性标志各有职责。生产 GUI 提供带类型的控件观察，帧路径使用明确的执行策略及截图/选择钩子；实际 GUI 弹出位置由生产代码自身持有。禁用测试的构建不包含场景实现或状态，收到验收参数时给出受控不可用诊断；普通有限帧运行、截图及 report 仍可用。`editor_acceptance_boundary` 检查生产头文件依赖和实际编译源选择。

组合交互报告使用布尔 `interaction_verified`，与退出判断共享 `IsInteractionComplete()`；测试关闭或未启用组合交互时为 false。旧的数值字段 `exercise_step` 已移除，其他已有报告键、类型和 CLI 参数保持现有契约。新增或维护用例时遵守 [Editor 验收用例维护](EditorAcceptance.md)，不要通过共享等待计数或状态序号推断动作、路由、截图和完成结果。

私有 `FEditorDocumentTransition` 区分文档转换目标与推进阶段，独立拥有请求、保存后继续、失败恢复、丢弃和取消规则。宿主提供当前 dirty、保存和待编辑快照，通过语义操作推进；GUI 和输入消费者只读取查询，不直接修改待换根、待打开或关闭状态。窗口关闭可以与待换根决策重叠，丢弃按关闭、换根、打开的顺序选择目标。弹窗请求与可见性单独建模，标题栏关闭和 Cancel 共用取消操作；取消仅撤销后续换根/退出意图，已接收的保存仍完成到原文档。

文件保存、场景加载、窗口动作和 root prepare/commit 仍由 Editor 宿主执行；`ContentRootService` 保留最终 dirty/busy/generation 检查与旧内容退休。`application.close.request/status` 与 GUI 共用这些关闭规则，状态投影仍为 idle/saving/failed/closing；接受关闭请求不表示进程已经退出。`editor_document_transitions` 验证无 GUI 的规则推进，真实保存、换根和关闭由 GUI 与附着自动化回归覆盖。

放置通过私有 `FPlacementService` 聚合场景、模型加载/上传、预览材质和图标加载/上传的准备结果。GUI、automation 轮询与最终提交共用 Ready/Pending/Failed 准入规则，错误信息和显示文案独立；CPU 加载完成不代表 GPU 已就绪。原材质尚未就绪的模型分部仍使用临时预览材质。`editor_placement_preparation` 验证阶段聚合、空错误和前缀碰撞，真实交互由 `editor_placement`、`editor_model_placement` 覆盖。

纹理编码和引用编辑使用 Runtime/AssetEditing 的 `FAssetEditWorkflow`，与 automation 共用 snapshot、busy admission、文档/资产身份及 generation 校验和单次历史提交。Worker 只准备拥有数据的快照，Main 提交文档；关闭或退出前 Drain 汇合任务，销毁路径不提交准备结果。资产窗口继续管理预览和页签，保存仍显式执行。

基础资产文档支持由 AssetEditing 的 `SupportsAssetDocument` 根据规范反射类型身份判断，Editor workspace 与独立 automation 共用 model/material/texture/sky 规则。此查询不决定预览能力或字段权限，Editor 仍保留加载/失败页签及自身预览诊断，scene 文档继续使用场景工作流。

资产属性控件通过 AssetEditing 的成员策略取得编辑路由，校验、规范化与预览影响由共享文档领域处理。资产、节点和图元改名保留当前预览；变换、材质分配、实际材质值/引用及编码数据变化触发预览更新。自动化节点改名使用相同规则。相等提交仍保留文档事务，连续交互的 Undo/Redo/Cancel 按整个交互累计的影响处理；纹理编码历史恢复准确 mip 数据并共享未改变的 mip0 存储。

预览形状和纹理通道的语义身份由 Runtime/AssetEditing 的 `AssetPreviewOptions` 定义，显式关联稳定数值、显示标签和模型路径或 RGBA 分量。Editor 内部保存类型化身份，GUI 位置只用于选择；GUI 与 automation 继续经过同一预览设置服务。天空预览用命名成员关联 Radiance、Specular、BRDF 的资产引用和缓存纹理；格式标签按 `ETextureFormat` 显式映射。重复选择不重建预览，这些设置不修改资产 generation、dirty 或历史。

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

Main 构建 UI 并路由相机输入，更新 Scene 后冻结场景帧；Render 将场景绘制到与视口物理尺寸一致的保留纹理，再构建 GUI 合成 pass。GUI 的采样读取显式声明在 RenderGraph 中，纹理源及生命周期随帧保留。此版本逐帧等待 Render/RHI 完成，再处理下一次 UI/场景变更，优先简化所有权；公共 FFramePipeline 仍供独立使用者使用。

场景色调映射写入 RGBA8 纹理的 sRGB RTV，GUI 通过 UNORM SRV 读取显示编码值，现有 GUI shader 解码后再写入 sRGB backbuffer。D3D12 为 RGBA8 颜色纹理提供 linear / sRGB 两个 RTV；其他颜色格式仍只允许 linear 视图。

## 当前边界

- 一个主场景窗口和一个按需创建的原生资产窗口，各自内部可停靠和浮动；未启用跨原生窗口的 ImGui multi-viewport，也不支持将任意面板拖出为新的原生窗口。
- 一个场景视口，默认 Deferred / reversed-Z；此版本不开放 offscreen 深度调试预览。
- Details 支持单选/多选反射编辑、保存与撤销重做，并提供从编辑器视角创建相机的入口；可通过 FGuiPanelEvent 订阅绘制扩展面板；内置文档面板仍属于同一个 Editor 插件。PRS 支持成组操纵，Del 支持删除选中子树，Place Object 支持内置形状、三种灯光和天空光；反射的类型化资产引用使用按类型筛选的选择器；尚无 Play、框选或自定义轴心。任意资产的模型替换和实例创建使用 Runtime 或导入工具。
- 界面使用内置 Roboto 字体和英文标签。字体 atlas 在启动时生成；中文字符显示与动态字体更新尚未加入。

## 验证入口

`editor_asset_preview_contracts` 验证形状/通道固定映射、重排及改名后的 GUI 选择映射、天空产品对应关系、格式标签和通道转换的固定像素。`automation_asset_preview` 使用真实附着 MCP/JSONL 比较预览 schema 基线，逐项切换形状/通道，并检查非法值、类型不支持、重复设置、就绪状态和资产历史隔离。

`editor_asset_editors` 覆盖各类资产预览、属性保存和独立历史，以及场景与资产原生窗口同时渲染、窗口内快捷键路由、缩放、资产最小化、主窗口整组最小化/恢复、关闭取消/丢弃/保存失败重试和窗口重建。`gui_docking` 补充两个 GUI context 的布局与 framebuffer 缩放隔离；`window_event_routing` 验证原生窗口事件路由。`window_ownership` 会短暂创建可见原生窗口，验证激活主窗口后的层级/焦点、非全局置顶、最小化/恢复、关闭隔离与重建。

`editor_multiselect` 使用真实编辑器输入验证 Outliner/Viewport Ctrl 增减选择、按下时的 Ctrl 状态、双目标描边、Details 混合值逐轴赋值、输入主对象已有数值、三个 gizmo 模式和整组 Undo/Redo，并检查父子选择、取消预览、无效批量提交、删除恢复及句柄重映射。独立入口为 `--exercise-multiselect`。`scene_management` 验证原子批量编辑/增删、最终层级校验及混合反射值；`transform_gizmo` 补充非均匀父级、剪切、跨零缩放和零缩放旋转的成组运算；`gui_input_and_data` 验证混合颜色逐通道编辑。

`editor_selection_shortcuts` 使用 `--exercise-selection-shortcuts` 验证双面板全选、超过 128 个节点、连续/反向范围、Ctrl+Shift 追加、过滤和折叠、Shift 按下时捕获、文本/焦点/弹窗/导航占用、重复键及取消层级拖拽。`editor_state` 补充行序、过期锚点、句柄 generation 和大范围验证；`automation_scene` 与 `automation_selection_framing` 验证 select_all 的目录、schema、共享选择、原子拒绝和实际 CLI/MCP 调用。

`object_placement` 覆盖分类、基本形状几何与放置计算；`gui_input_and_data` 覆盖复制载荷、跨面板预览/交付和取消；`scene_runtime_instance` 覆盖空场景动态注册、去重、保存重载和加载失败隔离。`editor_placement` 使用真实 GUI 输入依次放置九种对象，检查连续预览、八种取消路径、撤销重做、方向光阴影来源与生效天空光、图标拾取/隐藏、空文档 Save As 与重载，并验证缺失资源只禁用对应条目。可单独运行 `--exercise-placement OUTPUT.hasset`，截图随输出场景保存在同一目录。

`gui_docking` 验证布局保存恢复和纹理 ID；`gui_texture_rendering` 验证真实 RHI 离屏 sRGB 输出、GUI 采样和无效绑定拒绝。`scene_navigation` 覆盖共享控制器的默认环绕模式、飞行模式的按键门槛、固定位置旋转、滚轮调速/推拉分流、速度与位移一致性、速度边界及输入中断恢复。`editor_acceptance` 使用真实控件位置产生 Platform 格式的输入事件，覆盖菜单打开 Sponza、右键移动门槛、松开右键停止、原地旋转、右键滚轮调速、普通滚轮推拉、模态输入隔离、视口隐藏与恢复、窗口缩放、场景重开、加载错误恢复和加载中退出。GPU 验收需要 D3D12 环境及挂载的 Sponza 资产。

文档验收通过 `--exercise-document OUTPUT.hasset` 编辑反射属性，验证 Enter 前实时生效、按属性合并历史、连续 Ctrl+Z/Redo、改回原值仍保留修改记录、重做分支截断、Render 已应用状态、异步保存期间的新修改、重载、无效/过期提交拒绝、只读挂载保存失败和独立视口相机。`--report` 输出验收结果及保存耗时；输出场景写到指定测试目录。

文档验收还使用真实键盘输入编辑 Position X、Rotation Z 和 Scale Y，检查度数到矩阵的转换、精确 Undo/Redo 及保存重载。

`--exercise-views OUTPUT.hasset` 验证初始视图按钮、相机创建与世代变化、父级坐标换算、不可逆父级拒绝、应用视角、预览失效清屏、返回原编辑器视角以及保存重开；同样纳入 `editor_acceptance`。

`transform_gizmo` 是无 GPU 的几何与变换回归，覆盖自由/轴向平移、带剪切父变换、局部旋转、剪切对象整体比例缩放、镜像、零缩放与恢复、视线退化及非有限输入。`--exercise-gizmo` 通过真实 GUI 事件验证三个工具栏按钮、实时拖拽、零轴恢复、历史合并、Esc 取消与重做分支保留，以及失焦后提交最后有效预览和撤销重做，同样纳入 `editor_acceptance`。

`tools/ComponentEditorBenchmark.py` 比较 Debug/Release 与静止/移动 Editor 工作负载，保留 CSV、日志、程序哈希和进程内存采样。共享 CSV 和其他对照工具见 [渲染诊断](RenderDiagnostics.md)。

## Agent 附着

Editor 默认允许同一 Windows 用户通过 CLI/MCP 附着；`--disable-plugin automation-local` 可关闭。场景查询、组件与层级编辑、选择、剪贴板、放置、Undo/Redo 和显式保存使用 Runtime/SceneEditing 的同一个文档实例；agent 修改进入 GUI 历史，保存更新同一 dirty/save point。资产页签通过 `IAssetWorkspace` 共享实际草稿、历史与 busy 状态，内容根查询、设置和清空使用 `FContentRootService`，共同执行 dirty/busy 检查和旧文档失效。用法见 [应用附着](AutomationConnections.md)，操作范围见 [自动化能力](AutomationCapabilities.md)。

## 结构编辑和渲染诊断

场景默认相机和初始视图通过相机操作编辑；方向光和天空光在各自 Details 组件中编辑，照明与阴影来源由 Priority 推导。Details 不再显示 Hierarchy。修改父节点只在 Outliner 内拖拽：将已选节点行拖到目标父节点行，固定保持世界变换；拖到常驻的 **Scene** 行解除父节点。可先在 Viewport 点选模型或灯光，选择会同步到 Outliner，再从已选行起拖。Viewport 不提供 Re-parent 拖拽，保留点选与 Gizmo 操作。多选时拖动任一已选行会带上完整选择；父子同时选中只移动最外层节点，保留内部层级与主选中对象。一次放下对应一次 Undo/Redo，保存会记录新的 parent/local；局部变换数值可能变化。

Outliner 表格的首行是当前场景容器，名称取场景文件名（不含扩展名），未保存场景显示 **Untitled Scene**，Type 为 **Scene**。已有根对象在其下展示，默认展开；点击箭头折叠或展开，进入或改变非空搜索时展开一次。空场景和无匹配的搜索仍显示 Scene 行，拖拽开始或结束不增加临时行。它是场景文档的展示项，不创建真实节点、不修改场景文件，也不参与对象计数、选择、范围选择、全选、复制、删除或变换；折叠和搜索不改变选择或历史。

普通树和搜索结果都支持落点，悬停折叠节点会展开，靠近列表边缘会滚动。绿色边框表示可放下，红色边框和提示说明非法目标；Esc、失焦、右键或放到其他区域取消操作。自己、后代、失效节点和不可逆父变换被拒绝，整组不会部分修改。保持世界变换不改变 Enabled 的父级继承规则，挂到禁用父节点仍会隐藏。既有单节点 duplicate 与保留子节点删除仍提供 SceneEditing 和 Automation 能力，尚无专用 GUI 入口；场景对象的 Ctrl+C/Ctrl+V 使用上述完整子树剪贴板流程。

Edit > Render settings 打开渲染设置窗口。Reversed Z 修改后在场景及三维资产预览的下一渲染帧生效，Save render settings 独立保存下次启动值。视口工具栏的信息与统计按钮分别显示左上角只读状态和右上角 profiling HUD；Stats... 选择统计分类及采集选项。方向光 Details 的 Override Shadow settings 启用该灯光自己的 Directional / Contact shadows 参数，随场景保存并支持撤销/重做。视口选项继续提供剔除、冻结、合批、包围盒与光影响范围。字段、持久化、实时深度和 Automation 接口见 [渲染诊断](RenderDiagnostics.md)。

Pipeline、GBuffer 与 Visualizer 使用稳定选项身份，改变显示顺序不会改变保存值或画面含义；修改 VSync 等其他字段保留当前选择。切到 Forward 时，GBuffer visualizer 显示 Lit 并禁用选择，原 Deferred 选择仍保留，切回 Deferred 后恢复。

主视口使用完整的已提交渲染设置。3D Asset Editor 预览保持独立的 Deferred / Compact / Lit 和各页签曝光，只共享现有的深度约定与资产窗口 VSync；后来打开或恢复的预览遵守相同边界。预览设置不写入场景或资产历史。
