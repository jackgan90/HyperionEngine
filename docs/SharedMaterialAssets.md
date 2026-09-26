# 独立材质与纹理资产

当前 Content 归属、目录选择及外部资产重建见 [Content 与虚拟文件系统](ContentFileSystem.md)。

原生模型 schema 3 保存几何、带稳定 ID 的源节点/primitive 和 `MaterialSlots` 引用；旧 schema 2 可兼容读取。材质 `hyperion.materialasset` 与纹理 `hyperion.textureasset` 是可单独加载、编辑、保存的反射记录，可由不同模型共享。运行时不读取 glTF、源图片或资产 JSON；HLSL 仍由现有 ShaderCompiler 编译，编译后的 shader 资产不在本轮范围。

## 运行样例

~~~powershell
./tools/Build.ps1
./out/build/debug/bin/hyperion_editor.exe --asset-root ../HyperionAssets --scene /Game/Scenes/SharedAssets.hasset
./out/build/debug/bin/hyperion_asset_tool.exe --asset-root ../HyperionAssets validate /Game/Scenes/SharedAssets.hasset
~~~

`Scenes/SharedAssets.hasset` 引用 `Models/SharedQuad.hasset` 和 `Models/SharedPanel.hasset`；两个模型共享 `Materials/IndependentAuthoredShader.hasset`，后者引用 `Textures/SharedWhite.hasset`。这些自创资产直接由原生文件保存，旧 JSON 配方不再参与导入和来源重建。左侧实例保存局部绿色 Tint，右侧保持资产默认红色。

该材质的 shader 路径、入口 `AssetVertex/AssetPixel` 和 `ASSET_GAIN=1` 均来自资产。增加这类材质只需编写 HLSL 和资产数据；Renderer 不按该材质名称或固定 PBR 字段分派。`/Game/Shaders/SharedAsset.hlsl` 使用标准 View/Object block 与自定义 Surface.Tint。

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

分段选择使用 `Model.SectionSurfaces[PrimitiveIndex]`。数值修改产生新快照，保留其资产关联，不修改其他实例。Snapshot 保存与资产基准不同的 Material 值，并保留 Object 覆盖；Save As 会重定位所有模型、材质、纹理引用。原生资产内容本身可经 `Assets.LoadAsync<FMaterialAsset>`、复制编辑、`SaveAsync` 保存。作者引用跟随稳定 ID 的当前内容，重新加载场景后即可读取共享资产的修改，不需要重新导入父图；已持有的不可变快照保持原有内容。显式低层 revision 请求仍严格校验。

完整模型加载使用 `LoadNativeModel(Assets, Tasks, Path, Cancellation, &Resources)`。它在 Worker 上解析图并准备当前依赖版本的默认材质，再将 `FSceneModelData` 交给 FSceneModel/FModel。只读取 FModelAsset 得到的是几何和引用，不能直接作为已解析的渲染模型使用。CPU/procedural 源数据用 FModelSource，工具侧通过 SplitModelSource 生成独立记录。

## 离线发布与共享库

~~~powershell
./out/build/debug/bin/hyperion_asset_tool.exe import ../HyperionAssets/.cache/Sources/Models/Showcase.gltf out/library/models/A.hasset --library out/library
./out/build/debug/bin/hyperion_asset_tool.exe --asset-root ../HyperionAssets validate /Game/Scenes/SharedAssets.hasset
./out/build/debug/bin/hyperion_asset_tool.exe export-json out/library/models/A.hasset out/edit/Model.json
~~~

`--library` 默认为输出父目录。不同根使用同一库可共享资源；现存文件的可选导入映射用于恢复身份，不需要管理 hasset。新生成依赖直接写入该目录，不再创建 Models/Materials/Textures 等类型子目录；已有共享资产保持其路径，每个 ID 保留一个当前文件。发布以库为单位排队，取得库与根的 IO lease，完成全部转换后再更新当前文件；同步写入失败时恢复旧文件并移除新文件。历史由 Git/LFS 保存。

glTF 材质按来源模型和 material index 保持独立可编辑身份，不因数值相等合并。外部图片按逻辑来源身份加颜色/mip 设置共享；嵌入图片按来源模型和 image index 保持身份。新生成纹理还可按不含显示名称的原生数据指纹复用，颜色、维度、格式和完整 mip 都参与判断。BaseColor/Emissive 的 sRGB 与 Normal/MetallicRoughness/Occlusion 的线性解释生成不同纹理资产；不同 sampler 使用同一纹理数据。当前路径规范化不折叠符号链接或 Windows 大小写别名。

glTF 在 AssetImport 中拆分为模型、材质和纹理。mip 在离线生成，sRGB 的 RGB 分量在线性空间平均，alpha 普通平均；奇数尺寸边缘参与过滤。运行时 TextureSource 直接持有已加载纹理的 mip，不重新生成或复制像素。旧内嵌 model schema 1 不受运行时支持，应从原始 glTF/GLB 重新导入。Asset Import 不处理原生资产升级。

## JSON 诊断导出

`export-json` 输出与二进制记录相同的通用反射树 `{type, version, fields}`，用于检查字段和依赖；数值 bulk 带有 `f32`、`u32` 等类型标签。该导出不是可重新导入的源格式。模型、材质、纹理、天空和场景的旧 JSON 导入均已移除。

现有原生材质完整保存 Pass、Shader 引用、参数定义、默认值和实际值。通过 Editor 或共享资产编辑服务修改已有资产；当前参数编辑入口不提供从零定义任意 Shader Pass 和参数结构的完整材质创作流程。
