# 资产管线与静态 glTF 支持

原生资产工作流、工具、API、模式迁移、缓存和场景保存见 [NativeAssets.md](NativeAssets.md)。运行时 Assets 和 Viewer 只读取 .hasset；glTF/GLB 与场景 JSON 通过离线 FAssetImportService 转换。源模型示例由仓库脚本原创生成，沿用项目 MIT 许可证。

~~~powershell
./tools/Build.ps1
./out/build/debug/bin/hyperion_asset_tool.exe import "D:/Models/Scene.glb" out/content/custom.hasset
./out/build/debug/bin/hyperion_viewer.exe --model out/content/custom.hasset
./out/build/debug/bin/hyperion_viewer.exe --config experiments/Model.json --frames 180 --hidden --no-ui --capture out/captures/model.png --verify-model
~~~

Visual Studio 对应程序位于 out/build/vs2022/bin/<Configuration>。右键拖动旋转、滚轮缩放、方向键调整相机，Home 取景，Tab 切换面板。--verify-model 仍要求有限帧和截图，检查 GPU 已就绪及非背景覆盖。

## glTF 支持范围

| 项目 | 当前行为 |
| --- | --- |
| 容器 | glTF 2.0 JSON 和二进制 GLB |
| 依赖 | 外部相对文件、百分号编码 UTF-8 路径、base64 data URI、GLB BIN 和 image bufferView |
| 网格 | 多 mesh/primitive，索引/非索引 triangle list、strip、fan；统一转为三角形 |
| Accessor | offset、stride、interleaved、normalized、sparse；8/16/32 位无符号索引 |
| 属性 | POSITION、NORMAL、TANGENT、COLOR_0、TEXCOORD_0/1；缺失法线/切线按既有算法生成 |
| 节点 | TRS/matrix、层级和共享网格；default scene、首 scene 或顶层节点 |
| 图片 | PNG/JPEG 内存解码；渲染准备阶段生成完整 mip 链 |
| 材质 | metallic-roughness 的五类纹理、因子、UV 选择、sampler；KHR_materials_unlit |
| 颜色 | base color/emissive 按 sRGB，数据纹理线性；sRGB mip 在线性空间平均 |
| Alpha/面向 | Opaque/Mask/Blend，透明排序、双面和镜像绕序 |
| 持久化 | CPU 模型原生 .hasset，运行时不再解析源格式 |

动画、蒙皮、morph target、points/lines、UV2 及更高 UV 集会拒绝导入。唯一支持的 glTF 扩展是 KHR_materials_unlit；其他 required 扩展报错，其他 optional 扩展记录诊断并使用基础 fallback。没有 Draco、meshopt、BasisU/KTX2、KHR_texture_transform、扩展材质、IBL 或 glTF camera 导入。引擎场景阴影由独立 CSM 管线提供。

缺失切线的生成不是 MikkTSpace；透明排序以 primitive 中心为粒度，不能解决相交透明面。FBX/OBJ/COLLADA 尚未接入。PNG/JPEG 单张解码上限为 256 MiB、单边最多 16384；这不是整个进程的峰值内存配额。当前 D3D12 纹理描述符容量为 256，由同一设备上的模型和 GUI 共享。

## 适配器与渲染准备

AssetImport/Private/Adapters/GltfImport.cpp 封装锁定的 cgltf v1.15。根文件、外部 buffer 和图片通过 FAssetImportContext::Read 读取并跟踪内容指纹。cgltf 只解析内存，直接文件回调拒绝访问；stb 从内存解码。调用 cgltf 校验器前先检查 bufferView 物理范围、accessor/sparse 布局和节点路径深度，节点最多 256 层。

FAssetImportService 注册 FAssetImporter{Id, Version, Type, Extensions, Convert}，将语义转换集中在格式适配器。LoadAsync<T> 用于源格式工具/测试；ImportAsync 负责增量判断、稳定身份和原生依赖发布。新增外部格式仍需适配器；新增原生数据类型只需反射描述符和注册，不需要修改通用 Assets 读写分派。同步 LoadGltfPrimitive 兼容入口也属于 AssetImport。

GPU 上传、纹理 mip、材质适配与共享资源继续走 [RenderPrimitives.md](RenderPrimitives.md) 的 session/primitive 路径。RHI 0 创建和上传资源，上传 fence 完成后允许绘制；draw packet 保留资源到帧 fence 完成。没有持久化 GPU 句柄、独立材质/纹理资产或新增 importer 支持范围。通用材质边界见 [Materials.md](Materials.md)。

## 验证

async_gltf_import 保留源格式语义和取消/共享回归；native_asset_management、native_asset_publication 覆盖通用原生读写及发布。model_rendering 在拒绝源文件读取的后端上验证像素、上传和门控 IO；scene_viewer_controls 覆盖编辑/保存/重载。model_viewer_acceptance 使用 C++ 工具生成的原生 glTF/GLB fixtures，检查 UI、缺失资产和保存失败退出。

原始 2026-09-06 交付和独立审计的历史证据保留在 [ModelViewerReview.md](ModelViewerReview.md) 和 OpenSpec archive 中，不代表当前套件数量。当前变更证据见 [原生资产验证](../openspec/changes/complete-native-asset-pipeline/verification.md)。
