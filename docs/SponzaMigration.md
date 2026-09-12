# Sponza 场景迁移

2026-09-12。Scene Viewer 的 `experiments/Scene.json` 现在直接打开 Khronos glTF Sample Assets 的 Sponza。源模型、纹理、材质和节点变换原样保留；本次没有实现新的 glTF 特性，也没有修改光照 shader。

## 启动与交付内容

从仓库根目录运行：

```powershell
./tools/Build.ps1 -Preset debug -Target hyperion_viewer
./out/build/debug/bin/hyperion_viewer.exe --config experiments/Scene.json
```

Release 将两处 `debug` 换成 `release`。现有 Scene Viewer 入口保持不变；通用 Viewer 不带参数时仍使用 Triangle 实验。默认关闭调试面板以展示完整构图，按 Tab 打开；不会自动旋转镜头或播放场景动画。Home 仍为 Fit 全场景，重新启动恢复默认镜头。

| 文件 | 作用 |
| --- | --- |
| [Scene.json](../experiments/Scene.json) | Sponza 默认入口、1440×728 窗口、曝光 2.5、灰色背景和面板默认关闭 |
| [Sponza.json](../assets/Scenes/Sponza.json) | 单个 Sponza 实例及默认 eye/target/near/far |
| [Sponza 资产](../assets/Models/Sponza/Sponza.gltf) | 上游 glTF、bin 和全部 69 张图片 |
| [Source.json](../assets/Models/Sponza/Source.json) | 固定上游 commit、文件大小和 SHA-256 |
| [NativeContent.cmake](../cmake/NativeContent.cmake) | 构建时经现有 C++ AssetTool 导入并加入共享 catalog |
| [SceneViewerControls.cpp](../Source/Plugins/SceneViewer/Private/SceneViewerControls.cpp) | 固定镜头的视场角调整，投影、CSM 相机描述与 Fit 使用同一个值 |

原 Showcase 源场景及生成资产仍保留，可显式通过 `--scene out/content/Scenes/Showcase.hasset` 打开。原生 Sponza 为 `out/content/Scenes/Sponza.hasset`，依赖位于 `out/content/.assets`；运行时只读取原生资产，不联网、不解析源 glTF。构建需要源码目录中的完整资产，但不需要重新下载。

## 来源与资产核对

来源为用户指定的 [Khronos Sponza 页面](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/Sponza/README.md)，固定版本 `90d7ede14c7e280af263824604b427a1ca02cb66` 的 `Models/Sponza/glTF`，不使用另一个版本的 Sponza。完整保留 [上游说明](../assets/Models/Sponza/UpstreamREADME.md) 与 [上游许可说明](../assets/Models/Sponza/License.txt)；这些第三方文件不按引擎命名规范重命名。

直接核对该版本的 glTF 及图片：

| 内容 | 核对结果 |
| --- | --- |
| 层级/几何 | 1 个节点、1 个 mesh、103 个 indexed TRIANGLES 图元、262,267 个三角形 |
| 变换 | 原节点约 0.008 的统一缩放原样保留；场景实例为 identity |
| 材质 | 25 个 metallic-roughness 材质，22 个 OPAQUE、3 个 MASK，3 个双面材质 |
| 纹理 | 69 张：65 张 JPEG、4 张 PNG；68 张为 1024×1024，1 张为 4×4；无缩小或重编码源图片 |
| 顶点属性 | 全部有 POSITION/NORMAL/TEXCOORD_0；102 个图元已有 TANGENT；第 50 个图元（从 0 开始）无切线且其材质无 normalTexture，使用已有生成路径 |
| 采样 | Repeat、线性放大、三线性 mip 缩小；现有 importer 离线生成完整 mip 链，按用途区分 sRGB/linear |
| 外部文件 | glTF + bin + 69 张图片，共 71 个文件、52,686,624 字节，逐文件 SHA-256 已核对 |
| 未包含 | animations、skins、cameras、morph targets、extensionsUsed、extensionsRequired 均不存在；无灯光实体 |

保留 baseColorFactor、G 通道粗糙度/B 通道金属度、法线贴图、Alpha cutoff 和双面状态。源文件没有 occlusionTexture 或 emissiveTexture；不存在因为 importer 不支持而丢弃这些纹理的情况。现有白色 fallback 和默认 factor 继续生效。

## 默认镜头与参考图匹配

原图尺寸为 2491×1259，默认窗口为接近相同比例的 1440×728。通过参考图与本项目真实 D3D12 截图的纹理对应点，向原始三角网格投射射线获得 3D 点，再拟合透视相机；最终拟合受已有 world-up、零 roll 相机约束。没有增加任何相机导入特性。

- Eye：`(-10.521811, 1.233887, 0.461733)`。
- Target：`(2.304462, 3.170765, -0.395713)`；world up：`(0,1,0)`。
- 垂直 FOV：`0.729224966` 弧度，约 `41.7815°`；near/far：`0.05 / 100`。
- 在缩放至 1440×728 的参考图上，314 个有效纹理对应点的重投影 RMS 为 `1.337` 像素。这是这些几何对应点的拟合误差，不是整幅图像的颜色误差，也不等于拿到了上游未公开的相机矩阵。
- 原始拟合结果与对应点图：`out/SponzaMigration/CameraFitWorldUp.json`、`CameraFit.json`、`CameraMatches.jpg`。图像工具只安装在 `out/SponzaMigration/PythonPackages`，不属于引擎依赖，构建和运行均不需要它。

默认视场角是 SceneViewer 插件的固定值，因此显式打开其他场景时也使用它；Fit 同步使用该视场角。此项是应用默认镜头调整，没有新增 FOV 序列化或 glTF camera 支持。

## 缺失能力清单

**这个 Sponza 文件没有触发不支持的必需特性；没有为成功导入而删除源内容。** 以下区分本场景重现受限之处与引擎的一般导入边界，不能把源文件根本没有的数据记成“迁移丢失”。

| 能力 | 当前实现与本次影响 | 后续开发边界 |
| --- | --- | --- |
| 截图外部灯光 / 局部光 | 上游 README 明确说截图中的灯光不属于模型。本项目模型着色目前只使用一个方向光与常量环境项，不能重现参考中庭院内的局部补光与相应阴影 | 后续另行设计点光/聚光灯、场景灯光数据及阴影；不能从本 glTF 还原缺失的灯光位置、强度或类型 |
| glTF `KHR_lights_punctual` 导入 | 未实现；可选扩展会产生忽略诊断，作为 required 则导入失败。本资产没有该扩展，未发生灯光丢失 | 后续统一安排 importer 与场景/渲染支持 |
| glTF 相机导入 / 可持久化完整镜头参数 | 当前加载节点 transform/mesh/children，未消费 camera；场景清单保存 eye/target/near/far，Scene Viewer 使用固定视场角。本资产没有相机，无法直接读取参考截图的原始相机矩阵 | 本次仅调整已有应用镜头默认值，没有增加相机 importer 或场景 schema 字段 |
| 环境贴图 IBL、GI/间接光 | 当前环境项是常量近似，没有漫反射环境卷积、预过滤镜面环境和 BRDF LUT，也没有多次反弹照明 | 本次保留近似光照，用现有曝光参数改善可见性；后续单独安排 |
| 动画、蒙皮、morph targets | 静态 importer 对动画/skin 与 morph 明确拒绝；该 Sponza 不含这些数据 | 仅记录通用能力缺口，本次没有裁剪动画或冻结动态数据 |
| 其他 required 扩展 | 当前白名单仅 `KHR_materials_unlit`；其他可选扩展有忽略诊断。该 Sponza 未声明扩展 | Draco、纹理变换等扩展支持不在本次工作范围内 |

源码依据：[GltfImport.cpp](../Source/Runtime/AssetImport/Private/Adapters/GltfImport.cpp) 的 `CheckExtensions` / `LoadNodes`、[GltfPrimitives.cpp](../Source/Runtime/AssetImport/Private/Adapters/GltfPrimitives.cpp) 的 `ConvertPrimitive`、[GltfMaterials.cpp](../Source/Runtime/AssetImport/Private/Adapters/GltfMaterials.cpp) 的 `Binding` / `LoadMaterials`、[SceneManifest.cpp](../Source/Runtime/Scene/Private/SceneManifest.cpp)、[ViewerShadows.cpp](../Source/Applications/Viewer/Private/ViewerShadows.cpp)。

## 光照与技术差异

参考 README 没有给出截图的渲染器版本、灯光、曝光、色调映射或相机矩阵。因此下面关于 Khronos 的公式对比以其公开的 **Sample Renderer 当前实现** 为对象；不能据此断言 README 的历史截图必然由同一套公式生成。

| 项目 | Hyperion 当前行为 | 差异与影响 |
| --- | --- | --- |
| 直接光 BRDF | GGX NDF、Schlick Fresnel、F0=0.04 与 baseColor 按金属度混合；Smith 可见性采用独立的 G1(V)×G1(L) | [Khronos BRDF](https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/main/source/Renderer/shaders/brdf.glsl) 的 `V_GGX` 使用 height-correlated joint GGX；两者并非完全相同，尤其在粗糙表面的掠射角区域 |
| 环境光 | `(baseColor*(1-metallic) + F0*(0.7-0.4*roughness))*ambient*occlusion` | [Khronos IBL](https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/main/source/Renderer/shaders/ibl.glsl) 使用环境纹理、粗糙度 mip、GGX LUT 和多重散射补偿；本项目的金属灯具、旗帜与阴影中的表面会不同 |
| 光源与阴影 | 方向为 normalize(-0.45,0.8,0.65)，颜色 (3,2.85,2.7)，ambient=(0.22,0.25,0.3)；4 级 2048 CSM | 沿用引擎默认方向光与阴影算法；截图外部补光与其阴影参数未知，不进行匹配或补实现 |
| 曝光 / 输出 | 现有曝光设为 2.5；Reinhard `HDR/(1+HDR)`，sRGB RTV 执行最终编码 | 参考截图没有公布曝光或输出曲线，无法保证亮度、反差及高光一致；不改光照公式 |
| 渲染路径 | 默认 deferred、compact GBuffer、Reversed-Z，材质参数先编码后在 lighting pass 读取 | 存在中间参数量化；与其他渲染器的 forward 或高精度路径不能保证逐像素一致 |
| 粗糙度与切线 | roughness 下限 0.045；已有切线直接使用，缺省切线由现有算法生成 | 上游已有的 MikkTSpace 切线没有重建。唯一缺省切线图元不使用法线贴图，本次没有增加 MikkTSpace 生成器 |
| 贴图与筛选 | JPEG/PNG 解码成 RGBA8 并生成完整 mip；没有源资产减配 | GPU 纹理不是压缩 BC/KTX 格式。普通 mip 筛选并非专门的 alpha coverage 保持算法，远处叶片细节可能与参考不同 |
| 背景 | 配置中的统一灰色 clear color | 不把背景色当作天空照明或环境贴图；参考天空、室内补光和间接照明并未被重建 |

本地公式见 [SurfaceLighting.hlsli](../shaders/Lighting/SurfaceLighting.hlsli)、[Tonemap.hlsl](../shaders/Common/Tonemap.hlsl)、[ModelMaterial.hlsli](../shaders/Material/ModelMaterial.hlsli) 和 [GBuffer.hlsli](../shaders/Deferred/GBuffer.hlsli)。

## 验证

验证日志和截图放在 `out/SponzaMigration`；本机 NVIDIA GeForce RTX 5080，D3D12 debug layer 开启。

| 检查 | 结果 | 证据 |
| --- | --- | --- |
| Debug / Release 构建 | 两者通过，正常内容构建包含 Sponza 与 catalog | `BuildDebug.log`、`BuildRelease.log` |
| 原生资产依赖图 | 97 个资产全部通过 validate | `Validate.log` |
| 重复增量导入 | Up to date；written_assets=0、writes=0 | `Incremental.log` |
| 默认场景真实 GPU 运行 | Debug / Release 各 360 帧，1/1 models ready、0 failed、79 个可见 scene draws；validation errors=0 | `FinalDebug.log`、`FinalRelease.log` |
| 构建一致性 | Debug / Release 截图逐像素相同 | `SponzaDebug.png`、`SponzaRelease.png` |
| 静止镜头 / 剔除一致性 | BVH 第 360 帧与 Linear 第 420 帧截图逐像素相同，镜头不自动运动 | `FinalLinear.log`、`SponzaLinear.png` |
| 已有 CSM 的实际效果 | 关闭阴影后有 657,299 / 1,048,320 个像素变化；无验证错误 | `FinalNoShadows.log`、`ImageValidation.json` |
| 相关回归 | Debug 5/5（112.10 秒）、Release 5/5（42.25 秒） | `TestsDebug.log`、`TestsRelease.log` |
| 代码检查 | 430 个自有源码路径/格式；410 个模块源码、28 个模块边界；改动的 1 个 C++ 编译单元语义命名均通过 | `Style.log`、`Boundaries.log`、`Naming.log` |
| 变更检查 | git diff --check 及新增自有文件空白检查通过；71 个源文件哈希一致 | `AssetAudit.json`、`Source.json` |

回归范围为 `scene_management`、`scene_viewer_controls`、`async_gltf_import`、`scene_viewer_acceptance` 及自动依赖的 `model_fixtures`。其中真实场景验收覆盖 514 实例、三种剔除模式的相同画面、GUI、失败隔离；交互测试覆盖 Fit 与编辑保存重载。

原生图读取的文件总量约 394 MB，主要来自原样解码的 RGBA8 完整 mip 链。它明显大于约 53 MB 的压缩源资产，这是现有纹理存储格式的结果；本次未引入纹理压缩、流式加载或内存策略改造。

![Sponza 默认镜头实拍](../out/SponzaMigration/SponzaRelease.png)
