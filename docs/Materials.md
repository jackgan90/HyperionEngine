# 材质系统

材质描述表面使用的 shader、固定图形状态、动态状态、参数和渲染用途。它不拥有 mesh、相机、render target 或逻辑场景。一个 geometry 可以使用不同材质，同一材质实例也可以用于不同模型或自定义 primitive。五张 PBR 贴图仅是 glTF 适配器的约定，通用路径支持零张、多张贴图及只读 buffer。

## 模块与所有权

| 层 | 职责 |
| --- | --- |
| `Materials` | 仅依赖 Core/Math；CPU definition、instance、snapshot、参数 schema、semantic、状态和不可变资源源数据 |
| `Shaders` | 引擎自有 DXIL/SPIR-V/MSL 反射与编译缓存；DXC/SPIRV-Cross 留在私有 adapter |
| `Renderer` | 编译接口准备、provider、目标布局打包、资源协调、PSO/descriptor/constant slice 缓存和 Scene 桥接 |
| `RHI` | 通用 buffer/view/slice、sampler、binding layout/set、pipeline、draw packet 和能力查询 |
| `D3D12` | 原生状态转换、root signature、descriptor arena、上传和 fence 退休 |

`FMaterialDefinition` 是不可变配置；改变 shader、defines、pass 或固定状态需要新 definition。`FMaterialInstance` 由一个 Main owner 编辑，内容变化的 `Set`、`Clear`、`ReplaceDefinition` 产生新 revision，失败保留旧快照；设置相同值或清除不存在的覆盖不复制快照。Worker 私有 PBR 适配器可以在其唯一 owner 内临时构建实例，只发布 `Freeze()` 的不可变结果。跨线程和跨模型共享的是 `shared_ptr<const FMaterialSnapshot>`，Render 不读取可变实例。

texture/read-buffer source 拥有字节副本和不可复用的 identity/version；更改内容要创建新 source。GPU 资源在各 session/device 内共享，按源身份、编码、view 范围、数组次序和 sampler 值区分，不做跨导入副本的内容哈希去重。CPU Scene 直接依赖 Materials，仍无 Renderer/RHI 的直接或传递依赖。

## 声明、反射与按名称编辑

shader 参数物理名称与逻辑参数、semantic 分别定义。例如下面的 HLSL 使用任意命名：

```hlsl
cbuffer CameraData : register(b0) { float4x4 CameraMatrix; };
cbuffer SurfaceData : register(b1) { float4 ArtistColor; };
float4 VSMain(float3 InPosition : POSITION) : SV_Position
{
    return mul(CameraMatrix, float4(InPosition, 1));
}
float4 PSMain() : SV_Target0 { return ArtistColor; }
```

Main 的声明与写入可以使用独立的逻辑名字：

```cpp
const auto Semantics = GetStandardMaterialSemantics();
FMaterialDescription Description;
Description.Name = "Colored surface";
FMaterialPass Pass;
Pass.Vertex = {"Colored.hlsl", "VSMain"};
Pass.Pixel = {"Colored.hlsl", "PSMain"};
Description.Passes.push_back(Pass);
auto Camera = DeclareMaterialSemantic("Camera", "Engine.View.ViewProjection", *Semantics);
Camera.Targets = {"CameraData.CameraMatrix"};
Description.Parameters.push_back(Camera);
FMaterialParameterDeclaration Color;
Color.Name = "Tint";
Color.Type = FMaterialParameterType::Numeric(EMaterialScalar::Float, 4);
Color.Targets = {"SurfaceData.ArtistColor"};
Color.Default = FMaterialValue::Float(FVec4{1, 1, 1, 1});
Description.Parameters.push_back(Color);
auto Definition = std::make_shared<const FMaterialDefinition>(Description);
auto Instance = std::make_shared<FMaterialInstance>(Definition);
Instance->Set("Tint", FMaterialValue::Float(FVec4{1, .2f, .1f, 1}));
auto Surface = Session.GetResources().RequestMaterial(Instance->Freeze());
// Set a primitive's Surface to this lease, or a FSceneModel.Surface.Instance to Instance.
```

预声明参数可以在反射前写入。需要自动发现 shader 的未声明 uniform 时，调用 `PrepareMaterialDefinition`，等待其 Worker task；Main 用结果的 `Interface` 构造实例，再通过完整路径或唯一短名称 `Find/Set`。例如未显式映射的 `SurfaceData.ArtistColor` 会作为活跃参数导入。歧义短名称、拼写错误、类型不符、跨 schema 的旧 handle 都会拒绝。`Set` 返回 Active/Inactive；被优化掉的已声明参数不会与未知名称混为一谈。

反射保留目标实际的 offset、array/matrix stride、matrix major、结构树、resource shape、stage、space/register 和 stage signature。跨 stage 同名但不同接口不能强行合并；必要时使用 stage 限定的路径。编译 variant 的参数类型必须一致，修改 include、defines 或缓存格式会改变编译 key。普通数值写入不重编译 shader 或创建 PSO。

`FRenderMaterialDesc.Compiled` 显式选择一份不可变的预编译程序，必须与 Surface 的 definition 及设备 shader format 相符。同一 definition 的不同预编译对象分别保留；同一程序的数值 revision 仍共享静态资源。未传 Compiled 的自动编译路径单独按 definition 缓存，不受之前的显式程序影响。程序记录及其强持的编译结果随最后材质用户退休，不能只因 definition 相同而覆盖不同宏生成的程序。当前 session 按 Usage 使用该程序的 `Default` variant；准备接口可检查其他命名 variant，但不提供运行时非 Default variant 选择器。

DXIL 原生反射会将直接多维数组展平为总元素数，自动 schema 如实使用该布局；SPIR-V/MSL 保留各层数组维度。需要跨目标一致的逻辑形状时，显式声明嵌套数组 schema。准备阶段检查完整叶类型和总元素数，以 DXIL 原生 leaf stride 重建嵌套地址，保留实际 offset、extent 及矩阵/struct 内部布局。原生信息无法区分 `[2][3]` 与 `[3][2]`，维度顺序由作者负责，不能把兼容性检查当作原 HLSL 维度恢复。当前 DXC 对直接多维矩阵数组生成 SPIR-V 会报告缺少 MatrixStride decoration；因此这类数组的实际 GPU 验收限于 DXIL，SPIR-V/MSL 的多维测试覆盖 float/bool/struct。编译器错误正常向上报告，不绕过其验证或修改依赖代码。

纹理 sampled component type 与 structured-buffer element stride 同样进入接口约束。当前 RGBA8 路径拒绝 integer texture；structured view 的 stride 必须与 shader 反射值一致。RHI layout 的 `StructureByteStride=0` 表示尚未约束，但这种布局不能用于需要确定 stride 的真实 structured-buffer shader。当前 shader cache key 为 v7、reflection 为 v5；磁盘缓存沿用编译内容寻址，没有自动容量/TTL 淘汰，旧文件由缓存目录所有者管理。

## Semantic 与更新频率

`Engine.*`、`Pbr.*` 是保留命名空间；`ALBEDO_TEXTURE`、`NORMAL_TEXTURE` 等兼容别名会归一化到 PBR 语义。自定义语义使用自己的命名空间，并在创建 definition/session 之前注册、冻结 registry。语义定义包括类型、scope 和物理含义，不能仅靠名称猜测世界空间、单位或纹理编码。

| Scope | 输入及冻结边界 |
| --- | --- |
| Global | `SetGlobalParameters` 发布拥有值；修改时更新 scope revision |
| Frame | Main 的 `FreezeFrame`；自动加入 `Engine.Frame.Time/Index` |
| Scene | `SetSceneParameters`；冻结逻辑 Scene identity，direct primitive 使用 session 的默认场景 |
| View | `FRenderView` 的 Eye、ViewProjection 和 `Parameters`，由稳定 View identity 区分 |
| Pass | `FRenderView.PassParameters`，限定于 frame/family/view 的这次用途 |
| Material | instance snapshot identity/revision |
| Object | primitive 的 World/normal/orientation、`ObjectInputs`，以及发射 item 的稳定身份和实际内容 |
| Draw | `FRenderItem.DrawInputs`，限定于 frame/family/view、primitive 的 Scene/Slot/Generation、LocalItemId 是否存在及其值和当前 ordinal；`DrawParameters` 是按参数名覆盖 |

`GetProviders().Register` 只用于首次冻结前配置；provider 显式声明全部依赖 scope，只接收这些 scope 的 owned inputs。View×Object 派生矩阵同时依赖二者。`ObjectInputs`/`DrawInputs` 使用 semantic 名称；`ObjectParameters`、`SectionParameters`、`DrawParameters` 使用 schema 参数名，二者不混用。

参数解析顺序是 default → semantic provider → instance → Object → Draw。每次覆盖都检查类型、`OverridePolicy` 和 `OverrideScopes`。Engine semantic 默认 Locked，PBR semantic 默认允许手动设置。`Clear` 去除该层覆盖，恢复较低层值；不会把参数写成零。缺少活跃且 required 的值会产生明确错误，inactive 参数不触发缺失错误。

provider 返回空值仍保留全部声明依赖，保证原先采用 default 的参数在输入随后出现时重新求值。内置 provider 缓存只保存实际读取的同名值及弱输入身份，不延长无关 texture/buffer 的寿命；custom callback 仍比较全部声明 scope 的输入。同一组完整 scope key 只保留当前输入结果，内容变化替换旧项，过期 token 在 Collect 回收。默认预算为 4096 条、16 MiB 值树估算字节，超限淘汰最近最少访问项；估算包含输入、输出和 qualifiers，不包含外部共享资源载荷及全部分配器开销。`ProviderStatistics()` 提供 `CachedEntries/CachedValueBytes/Evictions`。float/vector/matrix helper 保留有限 float32 的原始位模式，包括正负零。

冻结输入采用 `FMaterialInputValues`：构造或整体替换时验证并排序，之后通过 `Get()` 只读访问；复制共享不可变值树。Global/Scene 写入相同内容时保留 scope revision/token。provider callback 仍返回 `optional<FMaterialValue>`，求值结果用 `FMaterialSharedValue` 共享；scope key 的 qualifiers 通过 `GetQualifiers/SetQualifiers` 访问。直接使用 Renderer 解析接口时，Values/Dependencies 表用 `Reset/Get/Set` 访问：每页 8 项的 copy-on-write 存储共享未变内容，前 4 页引用内联，更多参数使用扩展页，不限制材质参数数量。

标准块 `HyperionViewV1`（80 字节）、`HyperionObjectV1`（144）、`HyperionMaterialV1`（96）、`HyperionSceneV1`（48）见 `shaders/MaterialBlocks.hlsli`。标准块按完整版本化 ABI 校验，包括被优化掉的成员；用户自定义块按各目标反射打包，不要求使用标准名称。CPU 数值矩阵采用逻辑行序，打包器处理目标 major/stride，padding 清零。

相同有效 layout、成员映射、值和完整 scope dependency 可复用实际的 GPU buffer/offset。引擎准备路径另外按不可变程序及值身份复用未变块；scope 改变但最终数值相同时，也可安全复用原 slice。公共 `FMaterialConstantCache::Bind` 保留完整内容检查，接受同 key 下不同值并返回新 slice。相机变化只求值依赖 View 的参数；Frame/Pass/Draw 同样增量更新。混合 Object×View provider 在派生矩阵更新后求值。纯数值变化保留资源身份，不重新查询纹理 descriptor set 或 PSO；资源值或其依赖 owner 变化则失效。混合 cbuffer 中任一成员的有效数值变化时整块重新打包，不依赖标准块名称。

常量候选缓存默认最多 4096 块、16 MiB 对齐 slice extent；同一完整逻辑 key 的新值替换旧记录，超限按最近最少访问淘汰。跨 draw 准备快表最多 512 块，每个活跃 prepared draw 只保存当前块状态；它们不保留 scope token 或解析历史链。64 KiB 页面将 View/Pass/Frame/Draw 与长期输入分组。发布的常量区域不可覆写，淘汰只释放缓存引用，旧 frame/fence 继续持有旧 slice；空闲整页在无引用时回收，最多保留一页。`Resources.Statistics().Constants` 提供 `FullLookups/PreparedReuses/Evictions/CachedBlocks/CachedBytes/PreparedBlocks/PageBytes`。候选预算不限制外部保存帧、活跃 draw 或 GPU 在途页面，`PageBytes` 才是实际页面容量。

稳定 `LocalItemId` 的 primitive 持有有界的 CPU 参数解析缓存（最多 64 个 item/view 组合），键检查 snapshot、接口、usage、全部有效依赖的 identity/revision/内容，以及名称覆盖；稳定输入先检查，再构造派生矩阵/参数，World 按位比较以保留 shader 可观察的正负零差异；primitive 发布更新会替换缓存。Object scope 另按稳定 item 的实际 World、provider inputs 和名称覆盖维护有界 token，即使自定义发射器不改变 primitive revision，内容变化也会退休旧 token。RHI coordinator 用弱 CPU ownership 关联已准备的 draw packet，目标、镜像方向、geometry section 和材质不匹配时重新准备，动态状态和 scissor 每次应用。依赖 Draw/Frame/Pass 的值仍按相应频率失效。场景快照的 `FRenderItemList` 持有稳定 item 存储；裁剪和排序移动拥有句柄，保持透明及等深稳定次序。显式复制列表会深拷贝 item，避免修改独立排队的旧快照；自定义 `Collect` 仍向 `std::vector<FRenderItem>` 发射。

可分离的共享 engine 参数由 `FMaterialSharedParameters` 一次发布给同一 pass/参数掩码组，稳定 local `FResolvedMaterialParameters` 不随相机重复发布。批次兼容性、实例记录和 draw 常量都读取有效 shared/local 值；在实际 draw、需要重新打包记录或保守 fallback 时临时组合参数。View 与 Object 混合依赖、Draw 依赖、default/缺失输入、资源变化及实例成员中的共享值变化保留完整检查和增量 fallback。共享更新及 local 结果均不可变，旧 frame 保留自己的值与 GPU slice。

静态 collection 只缓存完整就绪且不超过 64 个 item 的原语输出；发布状态或资源就绪版本改变后重新准备。只有显式 `IsStaticCollection()` 的原语可使用这条路径；bounds、裁剪和透明排序仍按当前 view 计算。独立 view 状态最多保留 64 份及 120 个未使用 frame，场景发布仍会使 prepared view/plan 失效。

正常 session 自动安排缓存回收。独立使用 `FMaterialConstantCache` 时，调用方需在 scope 退休和 GPU 完成资源采集后调用 `Collect()`；返回 true 表示还有无 CPU cache owner 的页面被 packet/fence 保留，需要继续采集。`Bind()` 不扫描无关条目，仍活跃的 scope 不触发持续空闲轮询。

Object token 和参数解析缓存各自以 64 条为上限，插入压力下按最近访问帧淘汰，但保留本帧已访问的条目；本帧热集已满时，额外 item 直接求值而不加入缓存。这样重复扫描 65/128 项仍可复用已保留的 64 项，工作集更换后也能替换冷条目。完整复用与增量刷新都会更新 Object token 的访问帧。Provider 与常量候选使用独立访问顺序索引，O(1) 选择受害条目及更新顺序、O(log N) 定位并删除桶，不在每次逐出时遍历全表；同 key 替换、Collect 和 Clear 同步维护索引和统计。关闭资源服务后 `LivePages` 与 `PageBytes` 均为零，累计计数保留。活跃纹理、pipeline、descriptor 和 prepared draw 继续按租约及 fence 生命周期回收；本轮预算针对高频更新历史，不是整个资源系统的内存硬上限。

## Pass、能力与绘制边界

definition 可声明多个 `Usage`，`HasPass("Shadow")` 仅表示存在该 pass。`ResolveMaterialPass` 结合实际目标、sample count、depth/stencil 和是否调度该用途返回 Available/Unsupported/Incompatible 及原因。完整描述中的 A2C 标志不等于当前单采样窗口能够执行 A2C。

固定状态包括 raster、depth/stencil、blend、color mask、sample 等，动态 stencil reference/blend constants 位于 pass 描述中；pass 启用 `bAllowDynamicOverrides` 时，`FRenderItem.DynamicState` 可以覆盖它们。geometry 提供 vertex layout、stride、topology，session/graph 提供目标、viewport、load/clear；它们与 material 合成最终 pipeline。合法无 PS pass 支持深度用途，不能隐式向颜色目标写入。每个活跃资源必须有合法绑定或明确 fallback。

当前 native 执行是 D3D12 VS/PS、单颜色窗口目标、单采样，深度可选择 D32 或 D32S8。支持 Texture2D、固定资源数组、常规 sampler、只读 structured/raw buffer、多 cbuffer、VS texture 和非零 register space。实际 limits 通过 `FRHICapabilities` 查询，descriptor heaps 为显式固定容量，容量耗尽给出错误并回滚分配。comparison sampler、UAV、其他 stage、bindless、MRT、MSAA/resolve、任意离屏目标、完整阴影调度以及 Vulkan/Metal native 后端不在本次实现范围。SPIR-V/MSL 的编译与反射支持不代表已有对应 native 后端。

Main 冻结一份 frame，Render 在一个 task 中调用 `BuildViews(Graph, Views, Frame, Family)`，完成所有 view 的 collection/provider 解析后才提交 RHI 准备。View identity 和 Family 非零且在对应范围唯一，同一 frame/family 不允许重复接收。旧 frame 不得覆盖新 frame。单 view `Build` 委托同一路径。多 viewport 在同一图中具有独立的深度初始化域，graph 容量超限在 BeginFrame 前报告。

同一 primitive 可输出零到多个 owned item。可重复识别的 item 应填写 `LocalItemId`；缺省 ordinal 只在当前 collection 内有效，不跨 collection 复用 Object/Draw slice。即使 primitive revision 未变化，不同 World/Draw 内容也不会错误共享。`bClipSpace` 是 primitive 坐标契约，裁剪时不再套相机；自定义 shader 应采用对应的 WVP 语义。会在 shader 中改变几何边界的 pass 声明 `bRequiresConservativeBounds`；只有调用方明确提供覆盖位移的 bounds 并设置 `bConservativeBounds` 才启用 bounds 剔除。

## Scene 接入、就绪与生命周期

`FSceneModel.Surface` 选择共享 instance 或冻结 snapshot，`SectionSurfaces` 可替换指定 section 的材质。空 selection 继承默认材质。参数优先级为 legacy 三项 PBR override → model typed override → section typed override。材质变更不需要重新上传几何。

桥接器每次 `Flush` 为共享 instance 冻结一次，检测材质 revision 的变化，即使没有 `Scene.Update` 也会同步所有关联模型。所有待更新模型先准备和验证，再通过一次 `PublishGroups` admission 发布，成功后确认 Main 状态、版本与 receipts。Main 可确定的非法 section/schema/type 不产生部分更新；资源接口尚未准备时的验证会延迟到就绪阶段。异步失败有回执和组错误，可用合法的新 revision 恢复。

`IsInterfaceReady` 表示编译 schema 可用；`FRenderMaterial` 的 Ready 和 `FModel/Bridge.IsReady` 表示静态资源就绪，无需先有 View 或提交第一帧。动态 provider 缺失只使当前 draw preparation 失败。同一 model group 暂存全部 packets，全成或全败；独立 group 继续绘制。`FRenderBinding.GetLastDrawResult` 以及 `FModel/Bridge.GetDrawResults` 返回带 frame/family/view/usage/revision 的诊断，过时的 revision/frame 结果被丢弃。Viewer 显示 draw 错误但保留静态 readiness，使下一有效上下文能够恢复。

coordinator 权威持有布局、PSO、set、sampler、texture/read-buffer 和常量页；弱 scope tokens 控制缓存退休，draw/recorded list/fence 保留实际 GPU 引用。释放 CPU 租约只触发 RHI 0 采集。上传后失败、最后实例移除、停止出帧、失败 Present 均遵守同一 fence 协议。已经提交但尚未取得 fence 的上传保留到后续成功 drain；不能凭时间推测 GPU 完成。Close 前释放 CPU frame packets，再关闭 Scene/session，最后销毁 device/Tasks。

GPU cache 的 owner groups 按完整 shared ownership identity 建立有序索引，重复命中为 O(log U)，U 为活跃 owner group 数；过期 group 清理集中在 Collect，不在每个 draw 命中时扫描所有其他用户。没有固定数量上限的 native/cache 记录依赖活跃租约与 fence 完成退休，不能将“没有硬上限”直接等同于无限历史保留。批量发布已移除的旧 binding 再次清理时，Main admission 也校验 generation，不能释放新一代对象复用的槽位。

Triangle、ModelViewer、SceneViewer 已走同一通用材质路径；DebugUI 使用通用 RHI layout/set/CBV adapter 保留 overlay 顺序。glTF 的五种纹理角色、UV、mip、sRGB、mask/blend、镜像和双面行为保留，序列化字段、CLI、plugin ID 与 build target 未改。

## 验证入口

`material_contracts` 覆盖 CPU 约束；`shaders` 覆盖多目标反射和冷/热缓存；`material_bindings` 覆盖 schema、标准块、provider 和打包；`material_rendering` 使用真实 D3D12 像素、slice/offset、缓存计数和失败恢复验证。`scene_rendering` 增加共享实例传播、批次拒绝和覆盖优先级，`d3d12_frame_failure_recovery` 验证 sampler/set/page 的 GPU 保活与无帧退休。完整构建和命名检查命令见 [VisualStudio.md](VisualStudio.md)，本次日志和结果记录在 OpenSpec change 的 `implementation.md`。
