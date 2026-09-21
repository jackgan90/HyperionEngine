# 原生资产：导入、加载、编辑与保存

运行时只读取原生 .hasset。源 glTF/GLB 和场景 JSON 由独立 AssetImport 模块转换；Viewer 不链接 AssetImport，也不注册源格式 codec。现有实验配置仍是 JSON，model_source、scene_source、--model、--scene 保持原名，值改为原生资产路径。

模型 schema 3 引用独立材质并保存源节点/primitive ID，材质引用独立纹理。schema 2 可只读迁移，加载不写回。共享库、离线 mip、自定义 shader、通用参数和场景覆盖见 [SharedMaterialAssets.md](SharedMaterialAssets.md)。旧内嵌 model schema 1 必须先经 AssetTool 拆分升级。

天空资产、HDR/EXR 导入、浮点 cubemap 和场景天光引用见 [SkyLighting.md](SkyLighting.md)。天空同样通过 AssetTool 转为引用当前共享依赖的 `.hasset`；Viewer 不读取源 HDR/EXR。

## 构建和工具

从仓库根目录执行：

~~~powershell
./tools/Build.ps1
./out/build/debug/bin/hyperion_viewer.exe --config experiments/Model.json
./out/build/debug/bin/hyperion_viewer.exe --config experiments/Scene.json
./out/build/debug/bin/hyperion_viewer.exe --config experiments/Shadows.json

./out/build/debug/bin/hyperion_asset_tool.exe import ../HyperionAssets/.cache/Sources/Models/Showcase.gltf out/my-content/model.hasset
./out/build/debug/bin/hyperion_asset_tool.exe import ../HyperionAssets/.cache/Sources/Models/Showcase.glb out/my-content/scene.hasset --scene
./out/build/debug/bin/hyperion_asset_tool.exe import ../HyperionAssets/.cache/Sources/Scenes/Showcase.json out/my-content/showcase.hasset
./out/build/debug/bin/hyperion_asset_tool.exe inspect out/my-content/showcase.hasset
./out/build/debug/bin/hyperion_asset_tool.exe validate out/my-content/showcase.hasset
./out/build/debug/bin/hyperion_asset_tool.exe validate-library out/my-content
./out/build/debug/bin/hyperion_asset_tool.exe upgrade out/legacy.hasset out/upgraded.hasset
~~~

源文件与输出文件必须不同。--scene 将完整模型及其内部节点层级包装为一个场景 model 节点，并创建 directionalLight/environmentLight 节点及选择，不强制创建相机；Editor/SceneViewer 使用独立浏览视角自动取景。--name 设置模型名称或包装实例名称，--type 显式选择已注册类型，--force 跳过增量判断。inspect 和 validate 都验证根资产及依赖图，失败返回非零退出码。工具内置模型、材质、纹理、天空、场景和 catalog 类型；新增工具支持的资产类型需在工具中注册该类型及其源格式 importer。

普通 Viewer 构建直接消费已发布资产，不再导入样例。Engine 内置资源位于 `Content`；样例位于独立 HyperionAssets，通过 `/Game` 访问。`tools/PrepareContent.py` 显式恢复来源并调用 C++ AssetTool 发布。小型测试夹具仍生成到 `out/fixtures`。安装、挂载与来源重建见 [Content 与虚拟文件系统](ContentFileSystem.md)。

## 模块与线程

| 模块 | 职责 |
| --- | --- |
| Reflection | 稳定类型/字段 ID、类型注册、递归字段读写/遍历、校验及模式迁移 |
| Serialization | 与 C++ 布局无关的二进制编码、读取预算和 bulk 数据块 |
| AssetTypes | FAssetRef、头部、来源记录、catalog 等轻量反射数据，仅依赖 Reflection |
| Assets | 原生 envelope、CPU 对象缓存、引用解析、依赖图、异步保存 |
| AssetImport | importer 注册、源读取跟踪、glTF/场景 JSON 转换、增量发布 |
| Textures / Materials | 独立 CPU 纹理/材质反射记录与类型化参数 |
| Environment | 天空资产记录、全景转换、SH 投影和 GGX 预过滤；不依赖 RHI/Renderer |
| Scene | FModelAsset、FSceneManifest 和逻辑场景数据；不依赖 RHI/Renderer |
| Renderer | FSceneInstance/FModel、CPU 到 GPU 的准备、资源和绘制 |
| AssetTool | 组合工具所需类型和 importer；独立可执行程序 |

Main 仅发起请求、轮询、编辑及捕获快照；Worker 执行反射、转换、依赖遍历及编码；EDomain::Io 执行文件读取、原子替换和发布 lease 的获取。Worker 使用现有可恢复等待请求 IO，因此单 Worker 也可完成依赖加载。CPU ready、依赖图完整和 GPU ready 是不同状态。mip 生成属于离线导入；运行时直接共享已存 mip，GPU 上传/fence 与材质准备属于 Renderer，没有保存原生 GPU 对象或句柄。

## 调用

~~~cpp
FTaskSystem Tasks(3, 2);
FIOService IO(Tasks, LoadContentMounts("ContentMounts.json"));
FAssetService Assets(IO);
RegisterSceneAssetTypes(Assets.Types());

// 无类型入口根据资产头查找注册描述符。
auto Request = Assets.LoadAsync("/Game/Scenes/Showcase.hasset");
auto GraphRequest = Assets.LoadGraphAsync("/Game/Scenes/Showcase.hasset");

// 显式等待用于工具或 Worker；帧循环使用 Ready()/GetReady()。
auto Loaded = Request.Get(Tasks);
auto Scene = Loaded->As<FSceneManifest>(); // 检查真实 C++ 类型，不能随意 static_cast。
auto Graph = GraphRequest.Get(Tasks);      // Root、Assets、逐依赖 Failures。
auto Save = Assets.SaveAsync("out/saved-scene.hasset", Scene);
Save.Get(Tasks);

// AssetRegistry.h；实际扫描放在 IO 域。应用的 assets 插件在启动/换根时执行。
auto Discovery = DispatchAsync<FAssetDiscovery>(Tasks, {EDomain::Io}, [&]
{
    return DiscoverAssets(*IO.FileSystem(), "/Game");
}).Get(Tasks);
Assets.AddCatalog(BuildAssetCatalog(Discovery->Entries), "/Game"); // 同时报告 Discovery->Errors。
auto ById = Assets.LoadByIdAsync(Discovery->Entries.front().Header.Id);

Assets.Drain();
Tasks.Shutdown();
~~~

类型化 LoadAsync<T>/SaveAsync<T> 自动注册 T 的 RecordType；无类型入口需预先注册可能遇到的类型。FLoadedAsset 保留头部、描述符、不可变对象、实际路径及迁移诊断。FAssetRef 的 AssetId 是稳定身份，Path 是包路径或相对位置提示，TypeId 约束类型。保存和导入不固定 Revision；资产头的 Revision 仍用于内容校验。发现索引优先按 ID 定位，因此移动文件后重建索引即可继续解析。显式低层 revision 请求仍会校验。共享缓存不会绕过 ID、类型或显式 revision 约束。

依赖通过反射递归识别 FAssetRef，不按模型/场景名称分支。图遍历先检查活动路径以识别循环，再等待子资产；单个依赖失败不会丢弃其他已加载资产。默认图预算为 4096 个资产、32768 条边、128 层。对运行时场景，每个模型引用单独加载和报告错误。

## 缓存、保存与生命周期

完成的 CPU 缓存默认最多 64 项、256 MiB 估算保留字节；它使用访问次序淘汰，完成后自动修剪。原生对象的权重保守按存储字节的两倍估算，属于缓存预算而非进程硬内存配额。未完成任务单独持有，默认最多 256 个在途任务（包括完成清理）。ClearCache 释放已完成缓存；Invalidate(path) 使后续请求重新读取。它们不销毁其他消费者仍持有的不可变快照。FSceneInstance::Load 在重新加载场景前清理已完成缓存；后台文件监控尚未实现。

源转换缓存独立，完成项上限 16、128 MiB；ImportAsync 的增量判断始终重新校验源和发布结果，不会直接使用过期的转换缓存。两种服务均在完成和下一次请求时修剪内部任务列表。

同一服务内，同一目标路径的保存按请求接受顺序执行。其后的加载等待对应保存；保存前已返回的对象保持原有内容。SaveAsync<T> 在接收请求时复制传入值，调用者后续修改原对象不改变已接收快照。保存既有文件保留 AssetId，清除依赖的 revision 约束，内容变化产生新的 Revision，并移除旧导入来源记录，明确表示这是编辑后的原生内容。保存源格式扩展名会报错。

一个消费者的 Cancel 不取消共享生产者；服务析构取消并 drain 自有工作。显式 Drain 关闭外部入口，允许已接受的依赖图继续派生内部读取，并等待所有生产者和清理任务结束；各请求的错误仍由 Get/GetReady 观察。调用方必须先关闭消费者，再关闭 Assets/AssetImport，最后销毁 IO/Tasks。已进入系统的写入可能成功，取消无法撤销已完成替换。

挂载资产使用规范化 UTF-8 包路径，缓存、依赖图及写入顺序共享该身份。新发布内容和场景另存保留 `/Engine`、`/Game` 引用；旧相对引用仍可读取。原始导入文件和隔离工具输出可以显式使用本地路径。具体大小写、链接和只读契约见 [Content 与虚拟文件系统](ContentFileSystem.md)。

## 增量导入与发布

每个 importer 有稳定 ID 和版本。FAssetImportContext::Read 跟踪根文件、外部 buffer、图片等实际读取内容的 SHA-256。发布头保存 importer/version、选项、相对来源路径与指纹，以及来源到输出身份的映射。增量检查重新读取所有来源，验证原生依赖图完整性、当前记录版本和导入设置；内容、设置、importer 版本或模式变化都会重建。相同结果不重写该原生文件。

一个 AssetId 对应一个当前文件。生成依赖位于 Models、Materials、Textures、Skies 等可见目录，文件名由可读名称与稳定 ID 组成；已存在的 ID 优先沿用发现的路径。挂载发布使用包路径，隔离本地输出使用相对引用。已有原生包引用经完整图和身份校验后保留，并清除 revision 约束。

导入身份从现存文件及其可选 Import.OutputIds 重建，不需要 `.asset-library.hasset`。未找到目标的历史映射不使用。同一输出重导入保留根及存活产品 ID；显式新输出有独立根 ID。默认逻辑来源空间基于根 ID，源位置以相对路径记录；可用 `--source-root`/`--source-id` 指定跨根共享的逻辑来源空间。物理路径只用于本次读取，不能作为持久化来源 ID。相同原生纹理数据及颜色/格式/mip 解释可复用；不同可编辑材质来源保持独立身份。

一次发布中，如果多个来源要求同一个现存 AssetId 保存不同内容，导入会在写入前报告冲突，保留原生文件。当前不会自动拆分共享身份；需要统一这些来源的内容，或显式导入到独立库建立独立资源。纹理复用必须匹配实际暂存内容，不能复用已被本次导入更新的旧指纹。

增量检查、跨挂载原生引用以及 AssetTool 的 inspect/validate/export-json 都使用发现索引。文件只改名且 ID 不变时，有效依赖继续解析，来源未变的重导入保持零写入。通过 `--mounts` 提供完整内容根；未配置挂载的单文件工具默认扫描输入文件所在目录，引用该目录之外已改名的资源时应配置包含它们的内容挂载。

同一导入服务内，同一共享库按顺序发布；本地文件系统在 IO 域取得独占 Windows 文件 lease，其他活跃发布者会失败并返回明确错误。lease 文件关闭或进程退出时由系统删除。替代文件系统可覆盖 AcquireWriteLease；其默认实现仅提供当前进程、同一存储实例内的互斥。

完整图先转换、编码并暂存，重新检查输入后才写入当前文件；相同字节不重写。同步失败会恢复已修改文件并删除本批新建文件，回滚失败会附带诊断。调用方应在内容发布时使相关读取者静止；这里不承诺跨进程读者可见的多文件原子提交或断电恢复。活动 CPU/GPU 快照保留原有内容。历史由 Git/LFS 管理，不在工作树保留 revision 文件。

Catalog 是可重建的内存 ID/路径索引。`DiscoverAssets` 递归扫描原生元数据，忽略 `.git`、`.cache` 和发布状态；本地/挂载存储通过范围读取跳过 bulk。重复 ID 报错；损坏文件提供逐项诊断，不伪装为有效资产。实际加载仍执行完整内容校验。`validate-library ROOT` 验证全库；`migrate-library /Game EMPTY_STAGING_DIRECTORY` 用当前原生图生成隔离迁移候选，不修改输入。可选 `catalog` 命令仅用于显式诊断导出。

## 反射契约与版本迁移

~~~cpp
template<> const FRecordDescriptor& RecordType<FExample>()
{
    static const auto Type = []
    {
        auto Result = MakeRecord<FExample>("example.asset",
            {Member("name", &FExample::Name, {true, true, {"oldLabel"}}),
             Member("values", &FExample::Values),
             Member("optional", &FExample::Optional),
             Member("runtime", &FExample::Runtime, {false, false})}, 2);
        Result.Migrations.emplace(1, [](FArchiveNode::FObject& InFields)
        {
            // Explicit adjacent migration; a no-op declares compatible additive evolution.
        });
        return Result;
    }();
    return Type;
}
~~~

Member 选项依次为 required、persistent、读别名。默认值来自对象构造；迁移后仍缺少必需字段会失败。当前名和别名同时存在视为冲突；未知字段保留路径诊断并忽略。每个记录独立声明 version、MinimumVersion 和相邻迁移函数，缺少步骤或未来版本会失败，读操作不改写原文件。

支持 bool、有符号整数、完整 uint64、有限浮点、字符串、注册数值枚举、嵌套记录、vector、固定 array、optional、string-keyed map 和数值 bulk。枚举必须专门实现 RecordEnumValues<T>()，不接受未注册数值。类型注册检查 C++ 类型、模式、字段元数据及回调绑定身份冲突。TRecordCallback 保留 lambda 构造/调用方式，副本保持身份，替换成员读写、visitor、validator 或迁移回调会获得新身份；相同 callable 类型的不同捕获值也不能绕过冲突检查。所有读取先构造临时值并校验；ReadRecordFields 仅对可以 noexcept move assignment 的类型提交，其他类型用 ReadValue/ReadRecord 返回新值。

配置和 GUI 的 FTypeDescriptor 继续沿用，不因原生资产引入第二套同用途配置格式。这里没有 C++ 反射代码生成、任意指针对象图或对象内存转储。

## 二进制格式与限额

.hasset 的 HAST v1 envelope 包含：4 字节 magic、u32 容器版本、u64 payload 长度、64 字节十六进制 SHA-256 和 payload。payload 是 HYPA v2 archive，包含反射 header 和 object。头部 AssetId 使用随机 128 bit 标识；Revision 是根记录规范编码的 SHA-256。依赖表从反射字段自动生成，读取时与存储对象交叉核对；type/schema/revision 也必须与对象一致。

HYPA v2 以 little endian 写入 24 字节前缀：magic、u32 版本、u64 metadata 大小、u32 bulk 数量、u32 保留位。随后是每块 u64 offset/u64 size 的目录、metadata 和连续 bulk 数据。wire tag 固定为 bool=0、signed=1、double=2、string=3、bulk=4、array=5、object=6、null=7、unsigned=8。数值数组与图像像素存为带元素类型的数据块；不使用 variant 下标或原生结构体布局作为协议。

读取检查 magic、版本、完整性、范围、计数、重复 key、块索引、元素对齐、有限数值和多余数据。默认单文件 512 MiB（含 80 字节 HAST 头）、复杂度 100 万节点/64 层、保守累计分配预算 1 GiB。底层 EncodeAsset/DecodeAsset 可传入 FArchiveLimits；编码会先为 HAST 头保留字节，拒绝写出超过完整文件限额的内容。拥有原始字节的解码保留 bulk 视图直到类型构造，规范内容哈希也按块计算，不为哈希复制完整 bulk。公开 span 解码拥有必要副本，输入在调用后释放仍安全。预算不包含整个进程、图形驱动或任意自定义验证器的分配。

历史 HYPA v1 和裸 HYPA v2 容器可以只读解析，产生升级诊断和确定的临时身份；具体类型还须支持其模式版本，内嵌 model schema 1 要求工具升级；只有工具或显式保存才写入 HAST。Scene v1 的 path/TRS 会显式迁移成类型引用与矩阵，升级含源格式依赖的旧场景应使用 AssetTool，让依赖一并导入。

## 场景保存

Scene 面板的 Save edited scene 异步写出当前目录下 <原名>.edited.hasset，并显示保存中、成功或失败状态。重复保存同一文件保留身份。CLI 可指定路径：

~~~powershell
./out/build/debug/bin/hyperion_viewer.exe --scene /Game/Scenes/Showcase.hasset --frames 180 --hidden --save-scene out/edited/scene.hasset
~~~

FSceneInstance::Snapshot(destination) 保存 scene schema 7 的组件封装，保留对象/组件实例 ID、名称、完整仿射矩阵、可见性、几何选择和材质覆盖；新增实例获得独立 ID。自定义组件通过注册反射保存，未知组件阻止保存。它只保存仍在使用的模型引用，并按另存目标重新定位路径。相机、方向光、环境光、点光和聚光的节点 payload、层级及选择一起由 Scene 快照保存；相机镜头包含 FOV、near/far 和 focus distance。可选 initialView 保存显式初始浏览视图，导航不会修改它；插件不再补写独立 eye/target。运行时 Handle、GPU 资源、准备缓存、消息队列不进入文件。有资产关联的 FMaterialInstance/FMaterialSnapshot 和 section selection 会保存材质/纹理引用与类型化局部值；无原生关联的模型、材质或资源明确拒绝保存。详见 [SharedMaterialAssets.md](SharedMaterialAssets.md)。

## 验证与测量

源格式回归验证 accessor/sparse/stride/normalized、拓扑、纹理和节点语义。原生测试验证第三种数据类型、类型误用、模式迁移、损坏/伪造元数据、依赖循环、目录移动、catalog、取消、缓存和保存顺序。发布测试覆盖实际外部文件变化、去重、旧代际可读、失败保留旧根、并发顺序和本地发布 lease。模型/场景 GPU 测试的存储后端拒绝源格式读取；场景测试也比较编辑保存重载后的真实 GPU readback。

AssetTool 的每次运行输出 elapsed_ms、reads、read_bytes、writes、written_bytes 和 peak_resident_bytes。以下命令分别测量源 glTF 转换和原生 CPU 图加载，均包含校验：

~~~powershell
./out/build/release/bin/hyperion_asset_tool.exe measure-source ../HyperionAssets/.cache/Sources/Models/Showcase.gltf
./out/build/release/bin/hyperion_asset_tool.exe validate /Game/Models/Showcase.hasset
~~~

可运行 tools/MeasureAssets.py --tool <工具路径> --source <源模型路径> --output <结果目录> 自动进行每模式 2 次预热、7 次独立进程测量，保存原始日志和 Summary.json。

这两个测量不包括 GPU 上传；peak_resident_bytes 是整个短进程的峰值驻留集，不能当作缓存字节。首次运行与系统文件缓存也会影响耗时。实际 Debug/Release 结果和完整套件证据记录在当前 OpenSpec change 的 verification.md 中。

## 兼容入口

旧的 `FAssetReference` / `AssetReferenceType`、`LoadImageFile` 和 `LoadGltfPrimitive` 保留为兼容入口。新资产图使用包含类型、身份和版本的 `FAssetRef`；旧引用的任意字符串 ID 不能无损转换为该协议。`LoadGltfPrimitive` 已委托共享 glTF 转换核心，只适配旧 `FMesh` 返回类型。新源图像导入通过受预算约束的字节解码入口；同步 `LoadImageFile` 的 PNG/EXR 接受范围不与它们合并，以保留既有调用行为。

## 场景节点记录

当前 `hyperion.scene` 为 native schema v7，节点记录为 v3，使用带稳定实例 ID 的组件封装；plain JSON source v3 继续兼容，也可使用反射记录 JSON 表达初始视图和自定义组件。旧 native v1–v6 的模型、材质、矩阵、镜头和灯光状态通过显式迁移保留，新空场景不自动补节点。scene-json 和 native-scene-upgrade importer revision 7 保留整模型引用，不自动展开内部节点或 primitive；旧的显式展开文档保持其子节点和编辑状态。模型导入为场景不创建占位相机；发布策略进入缓存设置，避免复用旧展开或强制相机输出。旧版本读取不自动删除相机。运行时兼容读取不会修改磁盘文件。Snapshot 从当前组件及设置生成独立快照，包含 Transform、enabled、camera/light 状态、选择和 initialView，异步保存后续不会读取 live 节点。空 assets 的 camera/light/group 场景可保存。格式和示例见 [SceneComponents.md](SceneComponents.md)、[SceneManagement.md](SceneManagement.md) 与 [LocalLights.md](LocalLights.md)。
