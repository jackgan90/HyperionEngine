# 独立材质与纹理资产

原生模型 schema 2 保存几何、节点和 `MaterialSlots` 引用。材质 `hyperion.materialasset` 与纹理 `hyperion.textureasset` 是可单独加载、编辑、保存的反射记录，可由不同模型共享。运行时不读取 glTF、源图片或资产 JSON；HLSL 仍由现有 ShaderCompiler 编译，编译后的 shader 资产不在本轮范围。

## 运行样例

~~~powershell
./tools/Build.ps1
./out/build/debug/bin/hyperion_viewer.exe --scene out/content/Scenes/SharedAssets.hasset
./out/build/debug/bin/hyperion_asset_tool.exe validate out/content/Scenes/SharedAssets.hasset
~~~

[SharedAssets.json](../assets/Scenes/SharedAssets.json) 引用 [SharedQuad.json](../assets/Models/SharedQuad.json) 和 [SharedPanel.json](../assets/Models/SharedPanel.json)。两个模型引用同一个 [SharedColor.json](../assets/Materials/SharedColor.json)，后者引用 [White.json](../assets/Textures/White.json)。左侧实例保存局部绿色 Tint，右侧保持资产默认红色。

该材质的 shader 路径、入口 `AssetVertex/AssetPixel` 和 `ASSET_GAIN=1` 均来自资产。增加这类材质只需编写 HLSL 和资产数据；Renderer 不按该材质名称或固定 PBR 字段分派。[SharedAsset.hlsl](../shaders/SharedAsset.hlsl) 使用标准 View/Object block 与自定义 Surface.Tint。

## 数据与编辑接口

| 记录 | 持久化内容 |
| --- | --- |
| FTextureAsset | 名称、RGBA8 mip 链、Linear/Srgb 编码 |
| FMaterialAsset | 名称/版本、pass 用途、shader 路径/入口/defines、固定/动态状态、参数声明和值 |
| FMaterialAssetValue | 类型、数值 words、结构/数组元素、sampler 或 FAssetRef 纹理引用 |
| FSceneMaterialAsset | 材质 Reference、Material 作用域 Values、Object 作用域 Overrides |
| FSceneSectionMaterial | primitive section 编号及独立材质选择 |

数值支持 bool/int/uint/float、向量、矩阵、结构和数组。参数的 target、semantic、作用域和 override policy 沿用通用 Materials 校验。ReadBuffer、render target 和无原生关联的程序纹理不能持久化；保存会明确报错。纹理 sampler、UV 选择属于材质，颜色编码属于纹理资产。

创建新材质可用 `PersistMaterialDescription` 转换已有通用描述，使用 `PersistMaterialValue` 写入类型化值。运行时通过 `LoadSceneMaterialSelection` 异步解析引用，完成后在 Main 发布给场景：

~~~cpp
FSceneMaterialAsset Stored;
Stored.Reference = MaterialReference;
auto Request = LoadSceneMaterialSelection(Assets, Tasks, Session.GetResources(),
    Stored, ContainingScenePath);

// 帧循环等 Ready 后，在 Main 取结果并编辑。
auto Model = *Scene.Find(Handle);
Model.Surface = *Request.GetReady();
Model.Surface.Instance = std::make_shared<FMaterialInstance>(Model.Surface.Snapshot);
Model.Surface.Snapshot.reset();
Model.Surface.Instance->Set("Tint", FMaterialValue::Float(FVec4{0, 1, 0, 1}));
Scene.Update(Handle, Model);

auto Saved = std::make_shared<const FSceneManifest>(Scene.Snapshot(Destination));
Assets.SaveAsync(Destination, Saved);
~~~

分段选择使用 `Model.SectionSurfaces[PrimitiveIndex]`。数值修改产生新快照，保留其资产关联，不修改其他实例。Snapshot 保存与资产基准不同的 Material 值，并保留 Object 覆盖；Save As 会重定位所有模型、材质、纹理引用。原生资产内容本身可经 `Assets.LoadAsync<FMaterialAsset>`、复制编辑、`SaveAsync` 保存。现有引用若固定旧 revision，更新资产后必须显式更新引用或重新导入父图；不会静默跳到最新版本。

完整模型加载使用 `LoadNativeModel(Assets, Tasks, Path, Cancellation, &Resources)`。它在 Worker 上解析图并准备当前依赖版本的默认材质，再将 `FSceneModelData` 交给 FSceneModel/FModel。只读取 FModelAsset 得到的是几何和引用，不能直接作为已解析的渲染模型使用。CPU/procedural 源数据用 FModelSource，工具侧通过 SplitModelSource 生成独立记录。

## 离线发布与共享库

~~~powershell
./out/build/debug/bin/hyperion_asset_tool.exe import assets/Models/Showcase.gltf out/library/models/A.hasset --library out/library
./out/build/debug/bin/hyperion_asset_tool.exe import assets/Scenes/SharedAssets.json out/library/scenes/Shared.hasset --library out/library
./out/build/debug/bin/hyperion_asset_tool.exe export-json out/library/models/A.hasset out/edit/Model.json
./out/build/debug/bin/hyperion_asset_tool.exe import out/edit/Model.json out/library/models/Edited.hasset --library out/library
./out/build/debug/bin/hyperion_asset_tool.exe upgrade out/legacy-model.hasset out/library/models/Upgraded.hasset --library out/library
~~~

`--library` 默认为输出父目录。不同根必须使用同一库才能稳定复用导入身份。库的 `.asset-library.hasset` 保存来源键到 ID 的映射，`.assets/<id>-<revision>.hasset` 保存不可变代际。发布以库为单位排队，并取得库与根文件的 IO lease；先完成依赖及索引，再原子替换根。失败不破坏旧根图；已写出的孤立代际保留，不自动回收。

glTF 材质按来源模型和 material index 保持独立可编辑身份，不因数值相等合并。外部图片按规范化绝对来源路径加颜色/mip 设置共享；嵌入图片按来源模型和 image index 保持身份。BaseColor/Emissive 的 sRGB 与 Normal/MetallicRoughness/Occlusion 的线性解释生成不同纹理资产；不同 sampler 使用同一纹理数据。当前路径规范化不折叠符号链接或 Windows 大小写别名。

glTF 和旧内嵌模型仅在 AssetImport 中拆分。mip 在离线生成，sRGB 的 RGB 分量在线性空间平均，alpha 普通平均；奇数尺寸边缘参与过滤。运行时 TextureSource 直接持有已加载纹理的 mip，不重新生成或复制像素。旧 model schema 1 的运行时加载会要求显式 upgrade；升级保留根 ID，并将内嵌字段转换为材质/纹理依赖。

## JSON 来源格式

`export-json` 输出与二进制记录相同的通用反射树，格式为 `{type, version, fields}`，引用路径相对输出 JSON 重定位。import 支持当前模型、材质、纹理和场景 JSON；原有 scene schema_version=1 简写也保留。

数值 bulk 使用 `{"$bulk":"f32","data":[...]}` 等标签。材质数值的 `words` 是 u32 位模式，建议使用 C++ 类型化 API 生成；例如 float 1 的 word 为 1065353216。sampler/状态枚举取已注册的数值，未知枚举、重复 key、越界整数、错误类型和超预算数据会拒绝。源码 JSON 引用其他源码时，应清空旧原生 ID/revision 并使用源码路径，导入器负责生成固定原生引用。

## 缓存与验证

每个渲染服务共享材质准备缓存，使用不可变材质对象及解析后的纹理对象作为身份。材质/编译程序元数据最多 1024 项，纹理元数据最多 4096 项，均为弱引用；活动快照和帧持有所需数据。失效或淘汰只影响复用，不改变活动资源寿命。GPU 继续使用现有 source、definition、descriptor、pipeline 缓存和 fence 退休。

`FRenderResourceStats::AssetMaterials` 提供 Requests、MaterialPreparations、TextureSources、MaterialCacheHits、TextureCacheHits 和条目数；配合 GeometryUploads、Materials.TextureUploads/PipelinesCreated 可区分 CPU 准备与 GPU 创建。数值编辑与几何身份分离；新依赖版本可以复用同一几何资源，旧快照继续显示旧材质。模型就绪要求 Render 已接收当前版本，避免把旧材质的就绪状态用于新版本。

`shared_asset_rendering` 在一个 Worker、拒绝源资产读取的文件系统及真实 D3D12 下验证：

- 两个模型资产：2 次几何上传，1 次材质准备，1 个纹理源，1 次纹理上传。
- 单实例数值修改：几何/纹理/管线创建增量均为 0，另一实例不变。
- Save As 后清除 CPU 缓存重载：像素完全一致。
- 分段材质：sampler 变体复用纹理；相同 128 灰度字节的 sRGB/线性版本读回约 0.502/0.737；类型化纹理/sampler 覆盖保存重载一致。
- 材质依赖更新：旧红色和新蓝色版本同时显示，几何只上传一次；缺失依赖和错误 revision 报错。

`material_asset_contracts` 与 `shared_asset_publication` 覆盖通用记录、非法数据、mip、外部/嵌入图片、角色变体、并发根、共享库 lease、旧代际和旧模型升级。完整构建和回归结果见 [本轮验证](../openspec/changes/archive/2026-09-12-add-shared-material-texture-assets/verification.md)。上述计数来自小型确定性夹具，不代表大型场景帧率提升。

## 审核后补充的边界

- 包含尚未发布或加载失败的材质选择时，SceneInstance 的 Snapshot 明确报错，避免保存时丢失源文件中的材质引用和覆盖值。移除该实例后可保存其余场景。
- 模型等待初次加载时，已经接受的整模型/分段材质编辑和清空操作优先于稍后到达的初始加载结果；未编辑的分段继续采用来源中的选择。
- LoadNativeModel 不传 RenderResourceService 时仍可用于渲染。FModel 从当前已解析的材质数据取得声明快照，沿用通用材质的 Worker shader 编译流程；不同依赖版本的实例仍共享未变几何。
- 一个场景可以同时引用同一材质/纹理 ID 的多个固定 revision。离线发布保留各代际；同一来源转换中互相矛盾的命名产品仍报错。
