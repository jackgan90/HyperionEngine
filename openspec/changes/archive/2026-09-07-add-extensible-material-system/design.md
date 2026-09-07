## Context

审查基线：`1847be673f72f9877e62fb2d80556e0f8a10ccc9`。本 change 的当前阶段仅产出计划；未经后续实现指令，不执行 tasks。

当前主路径：Triangle 直接构造 `FRenderResourceDesc`；两个 viewer 经 `FSceneRenderBridge -> FModel -> RequestModel -> PrepareModelResources`；Render 收集并排序 snapshot，RHI 0 的 `SceneDraws.cpp` 物化 packet，D3D12 录制并通过现有 fence 集合保活。`FRenderMaterialDesc`/`FDrawPacket`/root signature 都内置模型五纹理布局。`ModelConstants` 每 draw 512 字节且混装 View、Object、Material 数据。`ShaderCompiler.cpp` 不反射 DXIL，SPIR-V 仅有资源级反射。`RenderGraph` 仅管理单个 swapchain color target 及可选深度，D3D12 固定单采样。

适用规范以根 AGENTS.md 和当前 `docs/CodingStyle.md` 为准（包括 boolean 的小写 `b` 前缀）；`openspec/config.yaml` 中旧的 uppercase-first boolean 描述不得覆盖当前规范。本任务不顺手修改该全局配置。

## Goals / Non-Goals

**Goals:**

- 材质脱离模型身份和固定 PBR 参数布局，支持参数名和 semantic，跨对象/模型共享以及局部覆盖。
- 在现有 VS/PS 绘制路径上实现完整的常用 raster/depth/stencil/blend 状态和通用只读图形资源绑定；关闭深度的带纹理材质是实际可执行路径。
- 明确数据来源、布局、身份、版本、线程、更新时机和 GPU 完成条件；真正共享兼容 uniform buffer，不仅共享 CPU 计算。
- 保持现有模型、场景、GUI、剔除、透明顺序和失败清理行为；提供可定位的验收及统计。

**Non-Goals:**

- 不增加阴影贴图生成/采样流程、完整灯光/聚簇系统、MSAA/resolve、MRT、通用离屏 RenderGraph、UAV/compute、bindless、GPU instancing、骨骼动画、材质图/编辑器、文件监听热重载或新原生后端。
- 不改变 `.hasset`/glTF/场景清单序列化协议，不创建新的磁盘材质格式；首版定义和实例由引擎 CPU API 构造，已加载 CPU 资源作为输入。
- 不重写 Tasks、资产 IO、Scene/BVH 算法或底层提交/Present 恢复。针对材质依赖和新资源类型扩展现有协调器属于范围内。
- 首版不承诺执行所有可描述硬件特性。下表以外的必需状态/类型在编译或 pass 解析时返回 `Unsupported`，没有隐式降级。更广能力的 enum/标签可扩展，但没有后端实现不得报告 Enabled。

## Decisions

### D1. CPU 材质模块和依赖

新增 `Source/Runtime/Materials` / `hyperion_materials`，Public 为 `Hyperion/Materials`，Private 按定义验证、参数、semantic 分拆。公开类型仅依赖 Core/Math（必要的标准库）；不依赖 Scene、Shaders 编译器、RHI、Renderer、插件或 native API。源文件遵守 PascalCase。

`FMaterialDefinition` 保存 Shader 源引用/入口/defines、CPU 材质状态、参数声明和用途 pass。`FMaterialInstance` 是单 Main owner 的编辑对象；构造出的 `shared_ptr<const FMaterialSnapshot>` 是跨线程可读值，不含 GPU handle。身份使用进程内单调 ID 加显式 revision，定义不可原地修改；定义变更构造新版本。定义默认值 -> 实例覆盖，首版不引入任意继承链。

资源参数的值为 Materials 定义的不可变 CPU 输入：`FMaterialTextureSource` 持有 ID/version、RGBA8 编码、每 mip 的 width/height 和拥有的 bytes；`FMaterialReadBufferSource` 持有 ID/version、拥有的 bytes，view 值单独记录 offset/extent 及 Structured stride 或 Raw 模式。构造时复制调用者 span，或接收不可变共享存储；禁止借用可变容器/裸指针。资源数组是这些值的固定长度数组，sampler 是 CPU 值描述。源构造验证尺寸、mip 链、字节大小、溢出与 view 范围，Renderer Worker 转换为 RHI upload/view 描述并以 source 身份/version/表示查缓存。源引用由 material snapshot、accepted preparation、native record 分阶段保活；source 生命周期和 GPU 完成各有自己的引用，不用模型内纹理索引或路径作公共参数值。程序化、自定义 Shader 和 glTF adapter 都走此输入契约，不新增磁盘 loader。

Materials 的作者状态描述与 RHI 完整 pipeline 描述分别定义；Renderer 的集中适配器用穷尽 enum 转换将前者映射为后者，并组合顶点/目标信息。禁止数字强转、未定义默认映射或 RHI 反向依赖 Materials。这里有少量显式转换成本，换取 Scene 的 CPU 材质引用不产生传递 RHI 依赖。共享编码/查询规则集中在 Materials 的规范化函数，Renderer 不维护第二份独立“有效属性”。不为本次状态类型新增泛化 Core 工具或另一个通用图形框架。

Scene 可以 PUBLIC 依赖 Materials；Renderer 依赖 Materials/Shaders/RHI/Scene；RHI 保持不依赖 Materials。`CheckBoundaries.py` 将 Materials 纳入数据模块限制，并检查 Scene/Materials 的渲染依赖闭包，防止仅检查直接 include 遗漏。第三方反射继续留在 Shaders/Private/Adapters。

### D2. 材质 pass、状态和可查询事实

定义拥有名称唯一的用途 pass，每个 pass 明确 shader program、参数到 shader 目标的映射、state 和输入要求。首版 scene scheduler 每次选择一个用途（默认 `Forward`），不会自动执行材质中的所有 pass。支持显式选择另一个自定义用途用于验证变体；ShadowCaster/DepthOnly 名称可以描述，但默认 renderer 不调度它们，也不伪造阴影功能。

材质状态包含 fill（solid/wireframe）、cull（none/front/back）、front face、depth bias/clamp/slope、depth clip、depth test/write/compare、stencil 前后面 compare/fail/depth-fail/pass 和 read/write mask、blend 独立 RGB/alpha factor/op、color write mask、alpha-to-coverage 请求和 sample mask。StencilRef、BlendConstants 是动态状态；初始来自 pass 默认，可由声明允许的 draw override 覆盖。Shader 入口和 static defines 的改变属于编译结果变更。参数/纹理/sampler 值改变不重编译 Shader。

最终 PSO = 材质 pass 有效静态状态 + 编译程序 + binding layout + 几何 vertex layout/topology + target signature。VB/IB、draw range、viewport/scissor、attachments 不由材质拥有。几何声明 vertex attributes、stride 和 triangle/line/point list topology；pass 声明所需输入，验证缺失、类型和 offset/range，保留 VS 已优化掉的可选属性。首版不增加 patch/tessellation/geometry/mesh shader。

`FMaterialDescription` 在 CPU 定义/快照就绪后可查询，不等待 GPU 上传。查询分为声明（如某 pass 请求 A2C）、实现结构（有无用途 pass、semantic 和资源需求）和上下文解析结果（Available/Unsupported/Incompatible + 原因及有效状态）。必须带 pass/variant 上下文查询最终 state；不得复制一个容易与 state 冲突的全局 `bAlphaToCoverage`。Shadow 查询分别表述 HasPass 和 CanExecute；当前 builtin PBR 不声明未经实现的 ShadowCaster。

Surface 分类/Queue 明确为 Opaque、Masked、Transparent、Overlay；Opaque 与 Masked 共用同一个不透明排序 bucket，保持原始注册/发射相对顺序（不会强制将所有 Masked 移到 Opaque 后面）；Transparent 为全场景中心投影深度降序，Overlay 为稳定顺序。Alpha clip 是 Shader 契约，不由 RHI 推测。分类是调度声明，blend state 是真实管线状态，不互相自动覆盖；自定义 blend 仍可显式选队列。数值变化不能私自改变队列；队列变化通过完整材质快照发布。

### D3. 首版实际支持矩阵

| 能力 | 实现与验证 | 范围边界 |
| --- | --- | --- |
| Shader | VS/PS，可选 PS 仅允许 color write mask 全零且无颜色输出需求；VS/PS 独立 entry/defines | 其他阶段显式 Unsupported |
| 常量 | bool/int32/uint32/float32、1-4 分量向量、2-4 行列 float 矩阵、固定数组、递归结构体 | half/double、运行时数组、不透明/指针类型拒绝 |
| 资源 | 2D RGBA8 linear/sRGB texture、mips、固定长度独立资源数组；独立 sampler；只读 structured/raw buffer view | Cube/3D/texture-array、UAV、可写资源、运行时 descriptor 数组拒绝 |
| sampler | U/V/W address、min/mag/mip、LOD bias/range、anisotropy（查询上限）、border color | comparison sampler/SampleCmp 不启用并显式拒绝；不新增 sampled depth 格式 |
| 目标 | 一个现有 swapchain color target、linear/sRGB view、1 sample、可选 depth/stencil | 多目标、离屏、MSAA/resolve 拒绝 |
| Depth/stencil | 保留 D32_FLOAT 默认目标；按 session 配置可创建 D32_FLOAT_S8X24_UINT 目标用于 stencil 验收；二者每帧单一选择 | 不引入任意 attachment 图 |
| Bind limits | Query 暴露每阶段资源/CBV/sampler、空间、buffer range/alignment 和 heap 容量；至少验收 8 张纹理、2 个非零 space、多个 cbuffer | 超限在录制前失败；不会按模型固定 5 |

MSAA/A2C 的请求可查询且 SampleCount=1 时返回不可执行原因。Stencil 的可选 swapchain 深度格式和图中单一 depth/stencil 初始化是材质状态落地所需的有界改造，不包含新 render target 系统。D32 与 D32S8 不在同帧切换；stencil 测试 fixture 显式选择 D32S8，现有 viewer 默认 D32。

### D4. 参数接口、反射和 Shader 编译

新增规范化 `FShaderReflection`（归 Shaders）：资源 name/kind/dimension/count/stage/register/space，完整 cbuffer 类型树、offset/size/array stride/matrix stride/major，VS 输入与 PS 输出。DXIL 通过 DXC reflection；SPIR-V/MSL 编译中间产物由 SPIRV-Cross 反射，并保留实际目标布局和 MSL 资源位置映射。MSL 仅保持编译/元数据可用，不承诺 Metal 执行。

先反射再生成可缓存的 artifact；缓存包含带 schema version 和校验的字节码/元数据，或在缓存读取时从保留的未剥离中间产物重建。采用后者：DXIL cache 保留 reflection container，SPIR-V/MSL cache 保留原始 SPIR-V，每次读取用相同规范化逻辑生成元数据及最终 bytes。元数据版本、有序规范化 defines、toolchain、entry、stage、target、编译/绑定映射选项与 source-root 内容进入 key；绑定/材质 manifest 进入单独 material compilation key。定义不在 Shader 源根时也不能遗漏其版本。重复 define 名称拒绝。

非零 register space 不沿用目前仅 space0 的 shift。首版支持明确查询范围内的 spaces（至少 0、1、2），每个受支持 space 都传入 b/t/s/u 非重叠 shift；register 区间按固定映射版本限制并验证，拒绝越界造成的碰撞。HLSL register -> target binding 的映射留在 artifact，不能把 SPIR-V offset/slot 当作 DXIL offset/slot。VS/PS 的相同物理位置仅在类型/布局/来源一致时合并；阶段不相交且布局不同则保留 stage-specific binding，交叠阶段冲突拒绝。

`FMaterialParameterSchema` 包含稳定的作者参数 ID、完整路径、类型、默认值、可选 semantic、来源/覆盖规则。参数目标可以是 `Vertex:Block.Member` / `Pixel:Block.Member` / 资源名，单一不歧义名称可作便捷别名；多目标映射共享一个逻辑参数。无显式 schema 的活跃 Shader 参数自动生成为同路径手动参数，仍可按 uniform name 写入；有歧义必须使用限定路径。schema 声明但在当前 variant 被优化掉的参数保留为 Inactive，setter 可更新逻辑值并报告 Inactive；拼错且未声明的参数报 UnknownParameter。必需活跃资源必须绑定，不要求供应已优化消除的资源。

CPU schema 的交接采用显式两阶段流程：Main 向 Renderer `PrepareMaterialDefinition` 提交 authored definition、用途/有限静态变体集合和所选 Shader target，得到独立异步结果；Worker 用 Shaders 编译/反射并转换为 Materials 的纯 CPU `FPreparedMaterialInterface`（schema、目标映射标识、诊断），以原定义 version 发布；Main 读取 Ready 接口后构造绑定该 schema 的实例。该步骤不创建 GPU 资源，也不需要 frame/view/geometry。Materials 不持有 compiler 或 artifact 的 native/vendor 类型。显式 schema 的定义可以预先构造实例，但首次渲染前仍需按上述接口验证；反射补齐若新增槽，须构造新的 schema/version 后在 Main 验证并迁移按逻辑 ID 保存的已知值，原实例不会被 Worker 就地扩展。未知名称在接口未就绪时返回 InterfaceNotReady，不能先猜 offset；失败/取消结果稳定且可重试。不同 variant 的逻辑 schema 按声明 ID/限定路径合并，类型冲突拒绝，每个 variant 保留自己的 Active/Inactive 和字节映射。定义/variant 变化重新准备接口后显式换版，旧 handles 失效。

Name 与 semantic setter 最终写同一逻辑槽。CPU 类型存储不等同 Shader 字节布局；bool 显式编码为 32 位，matrix 按目标 major/stride 写入，数组/struct 递归按 offset 打包、padding 清零。setter 在变更前检查类型/形状/有限浮点/资源类别；失败不递增 revision。handle 含 schema identity/version，换定义后旧 handle 拒绝。definition 默认值不足的活跃手动常量视为未初始化并拒绝绘制，不使用未初始化内存。

### D5. Semantic 契约与优先级

注册表存稳定字符串名称、类型、作用域、单位/空间/方向/颜色编码和默认必需性。`Engine.*`、`Pbr.*` 为保留命名空间，别名（如 ALBEDO_TEXTURE -> Pbr.BaseColorTexture）归一到同一 ID；扩展使用自有命名空间，重复注册定义冲突失败。同一 semantic 可映射到多个同型目标；同一声明向多个逻辑参数的有歧义写入拒绝，提供限定参数路径解决。

引擎语义首批包括 Frame.Time/Index、View.CameraPosition/ViewProjection、Object.World/Normal/OrientationSign，以及 Scene.MainDirectionalLightDirection/Color。主光方向约定为世界空间从表面指向光源的单位向量，内置默认值沿用当前 Model.hlsl 的光照方向/颜色；无通用灯光发现系统。PBR roles 包括 BaseColor/MetallicRoughness/Normal/Occlusion/Emissive texture 和 factor、UV set/normal scale 等，材质提供其值。normal texture 的切线空间约定、base/emissive sRGB 与其它 linear 在 builtin PBR schema 中明确，不强制任意自定义 texture 采用该 role。

每个参数 Source 为 Manual 或 Semantic，另有独立 Locked/AllowOverride 和 allowed scopes。解析顺序：定义默认 -> semantic provider（若有）-> instance override -> draw override；后两项只在 AllowOverride 且参数声明允许该 override scope 时生效。必需 provider 缺失失败，可选项使用显式默认值。Locked 参数按名写入也拒绝。清除 override 恢复 provider/default 值，不依赖 setter 调用先后。Engine 参数默认 Semantic/Locked，PBR 参数默认 Manual/AllowOverride 并允许实例/对象覆盖。

providers 由 Renderer 的只读 binding context 供值，不在 Materials 内拉取 Scene、Main 或 device。Global 指 session 范围而非进程可变单例；provider 注册在 session 初始化阶段完成，结构变更采用新 provider registry version。扩展 provider 消费 frame-owned 输入值和依赖 key，只在快照准备阶段求值，无 RHI 回调或跨队列等待。

### D6. 作用域、共享及多视图输入

`FRenderFrameContext` 带 session identity、frame serial/time、已冻结 Global/Scene provider 数据；`FRenderView` 增加 View identity/revision 和目标签名引用；pass selection 带 Pass identity。scene bridge 将 logical Scene identity 和 scene 参数快照送入已有 session（仍保留一条活动逻辑 scene attachment 的现有限制）。直接 triangle/primitive 使用 session 默认 scene。Material key 使用独立 material instance identity/revision。所有输入是 owned 值或不可变引用。

一个 primitive 可输出多个 item，不能只用 primitive identity/revision 当作 Object 数据 key。收集结果包含 owned `ObjectParameters`、`DrawParameters` 和可选稳定 `LocalItemId`。未提供稳定 ID 时由 collector 分配本次 collection 的发射 ordinal，并加入 Frame/View/Pass/collection identity，禁用跨 collection 复用；有稳定 ID 时 key 包含 primitive scene/slot/generation + LocalItemId + 生效 Object 参数内容签名，hash 命中后比较规范化值，保证相同 ID/revision 但不同 World/override 不误共享。Draw scope 总包含 frame/view/pass/collection identity、primitive/LocalItemId-or-ordinal 和有效 Draw 内容。不同 item 若要共享 Object storage，必须通过显式相同 Object identity 且完全一致的有效内容；内置单 item primitive 使用稳定 ID 0。标准 Object/Draw 输入在 Collect 后冻结再求缓存 key，不能从 Main 对象回读。

collector 还为每个 item 写入不可变 RenderGroup identity（render scene identity + 现有单调 group ID），用于静态组 readiness 和逐 view 的 packet 暂存/错误结果归属；不能通过材质相同或几何资源相同推断同组。组内无可见 item 时不物化 draw，已收集的可见 item 按该 view 原子成功/失败；其它 group 不受其失败影响。

保留单 view `FRenderSession::Build`，它委托给接受 frame context 和 view 数组的 `BuildViews`。一组 view 必须在同一次 Render task 调用中连续收集/冻结；期间不泵控制消息，RHI 等待只等待 RHI，Main 新提交的 Render 更新在该 task 结束后才处理。所有 view 的 CPU collection 完成后再物化 native packets，从而共享同一个 primitive/material 发布边界。调用者不能将相同 view family 拆成多次独立 Build 并声称是原子同帧；便捷 Build 的默认 family 只有一个 view。图 pass 名采用 session/frame/family/view/usage/segment 身份，重复 view ID 或重复 family admission 拒绝，避免当前 Scene 0 名称在多次 Build 中冲突。不引入多窗口或多 scene 并发模型。

不同 view 按数组顺序绘制到同一目标的各 viewport/scissor，每个 view 首次 depth 使用显式清理（允许清理共享 depth 全面，因为已完成前一 view 的颜色绘制不回访）；未要求 load 保留另一 view 的 depth。旧单 view 默认 viewport 不变。ViewerFrame 在 Main 更新插件后冻结 frame/provider context，并在既有单 Render task 内传给 BuildViews；时间/FrameIndex 由 session frame producer 生成，View 内容改变时更新其 revision，不把纯 culling/debug 开关变化算入 uniform block revision。

Material 定义可以声明多个标准 block（Global/Frame/Scene/View/Pass/Material/Object）或任意混合 block；规范布局由带版本的 HLSL include 和 CPU schema 共同定义并由真实反射校验。标准 View block 使用 CameraPosition/ViewProjection，Object block 使用 World/Normal/OrientationSign；Material block 使用 PBR 因子与采样相关值。标准 block ABI 固定，未使用成员不允许改变 block 总布局；跨 stage 合并用规范 schema 补足失活成员并校验所有活跃成员的位置。

GPU slice 的兼容 key 为 device + target ABI/layout signature + block logical mapping + 完整依赖 identity/revision 集合。仅相同 type/size 不足以共享；相同 semantic 在不同 offset 的 block 不能共享 bytes。Manual 参数的 key 包含生效 override 版本。同一个 shader block 混合了 view/object/material 时按三者共同打包；不声称能自动拆分既有 cbuffer。派生 WorldViewProjection 显式依赖 World 和 ViewProjection 两者。

RHI 0 使用页式 constant storage：CreateBuffer 批量创建 upload-memory 页；追加写入 API 仅对 coordinator 未发布的 slice 开放。按 capability 对齐分配，保留 logical size 和 padded extent，原生 CBV 范围不得越界。已发布 slice 不原地修改，新 revision 分配新 slice；引用归零且 GPU 已完成后回收，空闲页保留有界上限（默认一个页，额外空闲页在 collection 释放）。首版页中已使用空间在整页无活跃 slice 后重置，避免复杂空洞压缩。稳定 Material/Scene/Global slice 跨帧保留；View/Object 仅在依赖不变时复用；Frame/Draw 使用帧身份使数据自然更新。

提供可测试统计（各 scope 的 provider 求值、打包次数、上传 bytes、slice 复用、descriptor/PSO 创建及回收），不增加 GUI 面板。用同 buffer identity + offset 及计数断言证明 GPU 共享；不以 draw 数减少作为验收。

### D7. RHI binding layout 与 D3D12 descriptor 管理

`FResourceBindingLayout` 声明物理位置、kind/count/stage、buffer requirements；`FResourceBindingSet` 是不可变设备资源，持有 texture/buffer/sampler/slice 引用。布局身份独立于具体资源值，跨 stage 合并规则遵循 D4。所有活跃 slot 必须完整绑定；optional 参数在 material 层补齐显式 fallback 资源（或被 variant 优化消除），后端不读取未写 descriptor。

资源 binding-set cache key 为 device + layout/group identity/version + 按槽排序的 resource source/native generation + view kind/encoding/mip/offset/extent/stride + sampler 规范化值。不能只按 material instance identity，也不能忽略数组元素、view 或 sampler。与 Object/Frame CBV 分离的资源组不含 frame serial，因此同材质跨对象/帧复用同一 native binding set 和 descriptor range；资源来源为 view/pass 时 key 包含实际资源身份。动态 CBV slot-to-slice 参数列表在 packet 中独立缓存/保活，不使 texture table 重建。修改一个 texture/sampler 产生新 set，旧 set 保留到最后 CPU/GPU 使用结束；缓存的权威 ownership 可退休，不能永久保留所有历史 sets。统计与测试明确断言稳定多帧 descriptor 创建/复制次数不增长，更新时增长一次且旧范围在 fence gate 前不复用。

`FDrawPacket` 使用 pipeline、geometry、binding sets、dynamic offsets/state；移除 Constants/Texture/MaterialConstants/MaterialTextures 等专用字段。动态 CBV slice 与只读资源集合分开，避免 per-object 常量变化复制所有纹理 descriptor。D3D12 使用 layout 规划的 root CBV 和资源/sampler descriptor tables；root signature budget 计算、range overlap 和 register 限制在 CreateLayout 时验证。descriptor tables 按 layout 合并范围而非每 semantic 一个 root parameter。

将现有 texture heap 管理扩为 device-owned CBV/SRV/UAV heap（首版只分配只读 views）及 sampler heap，提供 contiguous range 分配/回收。heap 创建时固定容量（查询暴露），不在绘制中换 heap或自动增长；容量耗尽返回明确错误且回收所有部分分配。已有 texture 的 source descriptor 与 binding table copy 明确区分，shader-visible range 由不可变 binding set 保活。必要的 CPU descriptor 存储也由相同 native resource 管理，不将 texture 自身 Slot 当作完整 binding table。

资源/布局创建及 descriptor 分配在 RHI 0；indexed RHI recorder 仅读取已完成 binding set 并录制。sets 和 constant slices 由 packet -> recorded list -> fenced submission 保活，已发布范围在 GPU 完成前不可覆写。direct RHI client 保持原有共享 payload/device-state 生命周期；managed sets/layout/samplers/pages 的权威引用在 coordinator 中，最后销毁走 RHI 0。

只读 buffer view 验证 byte offset、extent、structured stride 或 raw 对齐；buffer 创建使用明确 usage，不能用 vertex buffer handle 冒充未声明用途的 SRV。RGBA8 纹理上传沿用现有异步 fence；VS texture sampling 要将最终资源状态从 pixel-only 改为与实际 stage 兼容的 shader-resource 状态。第一版资源只读，上传完成后处于可被所有所需 graphics stage 读取的状态，不需要通用状态图。

Draw validation 在录制前验证 device、layout compatibility、完整 slot、type/count/stage、upload readiness、CBV 对齐/extent、geometry/index range 及 target signature。异常仍沿用 join all admitted recorders -> CancelFrame 的协议，不能释放已提交失败 Present 的资源。

### D8. 资源服务、发布和缓存

保留一个 session/device 的 `FRenderResourceCoordinator`，扩展内部记录种类为 Geometry、Texture/ReadBuffer、CompiledMaterial、BindingSet/ConstantPage。公开几何与材质使用独立 thread-neutral lease。`FRenderResourceDesc` 可保留为批次 builder，但不再以整个模型记录拥有所有材质/PSO；section 指定几何范围和默认 CPU material snapshot，准备时拆成可共享的独立记录。

资源 key 使用拥有身份的不可变 source token + version + subresource + representation（texture encoding/mips/view）；禁止裸地址脱离 owner 复用，不能用 source path 唯一标识。程序/layout/PSO 按结构化内容 key 在同 device 共享；PSO key 包含 program bytes/options、layout、vertex layout、topology type、完整静态 state、target format/sample、mirrored winding，不含普通参数值、动态 sampler 或 texture identity。查找 hash 后校验结构内容；每类缓存没有活跃租约或 admitted/GPU 引用时可退休，不把历史 revision 永久缓存。

就绪状态分为帧无关资源就绪和本次绘制准备，禁止以 Frame/View/Object uniform 或最终 draw bindings 作为进入 Collect 的前置条件：

| 阶段 | 输入与责任 | 发布 |
| --- | --- | --- |
| InterfacePreparing/Ready | Worker 编译/反射和 CPU schema 验证；无 frame/geometry/device 资源需求 | D4 的独立 CPU 接口结果 |
| ResourcesPreparing/Uploading/Ready | 已选 snapshot 的程序接口、geometry、静态 resource source/view 上传及可预制资源组；无 View/Object slice 依赖 | model/binding 的 ResourcesReady；旧 IsReady 兼容表示此阶段 |
| DrawPreparation | 资源就绪的 group 进入 Render collection，冻结 view/pass/item 参数；RHI 0 基于真实 target/input 创建或查询 PSO、分配 slices、补齐动态资源组 | 一次 frame/view/usage/group 的完整 packets 或带 revision 的 draw diagnostic |

geometry/material/静态 texture 未全部 ResourcesReady 时初始整组不绘制，但必须在不提交任何 frame 的情况下也能到达 ResourcesReady。第一次可绘制 frame 负责动态绑定，不能等这次绑定完成才允许第一次 Collect。新 snapshot 批次在已知 schema 错误时整体拒绝；异步静态准备失败发布所选 revision 的 ResourcesFailed。选中 pending 静态替换后 affected model group 暂不绘制，旧 frozen frames 保留旧租约。仅常量值更新不进入等待 frame bindings 的资源 pending 状态。

DrawPreparation 对同 group 先暂存所有 packets，全部成功后才加入 graph；部分失败释放未发布 CPU 引用，经 coordinator 正常退休资源，保留其它成功 group。dynamic provider 缺失、target/PSO 不兼容或帧分配错误发布 thread-neutral `LastDrawResult`（含 frame/view/usage/selected revision），不将共享静态 ResourcesReady 记录永久置 Failed；下次兼容 context 可重试并以成功结果清除该上下文错误。model/viewer 的错误显示合并静态资源错误与最新适用 draw diagnostic，但 Ready/IsReady 查询不依赖未来 frame。旧 revision 或更早 frame 的结果不能覆盖更新版本的状态。无新帧时静态失败、资源退休仍由现有进度任务完成。

以上阶段无旧/新参数与 descriptor 混装，无自动回退；后续有效更新可恢复。material-only 替换不再次请求 geometry upload。PSO 不预先阻塞与其尚未知的 viewport/target 配置相关的首次 collection。

状态/错误发布扩展现有 primitive results，按上述阶段区分 geometry/material 的静态 Ready 与本次 DrawResult。相同 material instance 由多个 primitive 共享时，Main 显式提交新 snapshot；Scene bridge 在 Flush 比较订阅实例 revision，将受影响模型一次合并发布，确保同一 frame boundary 的共享用户看见一致版本。直接 primitive client 同样通过显式 batch 提交，不引入 Render 对 Main 对象的读取。更换 definition 必须重新验证旧 overrides；不适配者整次更新拒绝，禁止按 offset 套用。

取消一个使用者不取消其他共享 production；stale revision/generation 完成不回写当前 binding。新增 records 加入现有 Process/CanRelease/Collect/Close 路径和失败退休，包括在纹理上传提交后发生后续 material/descriptor 失败的情况。上传批次保活与 partial native ownership 在任何异常中都不能丢失。使用原有无新帧 cleanup 机制，不增加 Main/Render 回调依赖。

### D9. Scene、PBR 和插件迁移

Scene 的 `FSceneModel` 添加 CPU material selection：默认替换（可选）、按 asset primitive/section index 的替换 map、通用 typed overrides。不是按 node occurrence 定义新的资产格式。优先级为 section explicit material > model default replacement > imported default；先选 definition，再应用 model overrides，再应用 section overrides。现有 BaseColor/Metallic/Roughness 兼容入口转换成 PBR semantic override；同一参数又经新 API 明确覆盖时新 API 优先。不可用参数/类型在 bridge 的定义解析时失败，不静默忽略。异步资产未就绪时保存选择，asset 到达后验证 section index，失败保留可移除状态。磁盘 FModelMaterial/清单格式不变。

`FMaterialInstance` 共享引用只在 Main 上读写；Scene snapshots/bridge outgoing messages 携带冻结 CPU material snapshot。Scene GetChanges/bridge Flush 增加 material revision 订阅检查，不能仅依赖已有 Scene.Update 是否发生；一个 material revision 在该次 Flush 冻结一次，影响的多个模型更新作为一个关联 batch 交 Render。复制一个 model 默认共享材质实例，创建新 material instance 则实现独立编辑。primitive 的 draw overrides 不写回共享实例。

桥接批次通过有界扩展现有 FModel 完成：将 SetState 中的验证/生成 owned primitive updates 与最终 Publish 分离；bridge 为已有 attachments 收集更新，全部验证后单次调用 FRenderSceneClient::Update，并在 admission 成功后推进已发布 material revision/receipt。任何成员验证失败不确认其共享 material revision，也不发布半批；后续有效编辑可以恢复。new attachment 的 CreateBatch 携带本次冻结材质版本，remove/replace 按已有 generation 顺序处理；不为该行为重写 Tasks 或引入通用事务引擎。

Renderer 私有 PBR adapter 消费 glTF FModelMaterial，创建 builtin 定义/实例、五种 role 的纹理和 UV/sampler；五张是该 preset 的接口，不是系统限制。`PrepareModel` 的几何准备和 mip/颜色空间转换可复用。Model.hlsl 拆成 View/Object/Material 常量块，光照方向/颜色来自默认 Scene provider，保持现有数值、tone mapping、alpha mask、alpha、double-sided 和镜像方向行为。初版各已命名 block 对应实际反射结构，PBR 手写 C++ memcpy 布局退出通用 SceneDraws。

Triangle 使用零纹理简单定义；几何输入移到 geometry 描述，TransformMatrix 由 clip-space object 参数供值。将 `bClipSpace` 从材质移到 primitive 的显式坐标契约（World/Clip）和 bounds policy；Clip 默认不做 world BVH 拒绝，但以其真实 clip transform 做可证明的 item 级测试。自定义位移 pass 声明需要 conservative bounds，primitive 未提供时整条 pre-collection/group/item 路径保守保留；材质切换需使相关 group bounds 失效，避免旧 BVH 提前剔除。

两个 viewer 继续通过 Scene/Bridge；scene-viewer UI 行为不新增材质编辑面板。DebugUI 仅迁移 native layout/packet 构建为通用 bindings，其 GUI overlay pass、剪裁和顺序不变。全仓检索所有 FPipelineDesc/FDrawPacket 生产者（包括 native fault injection 和 fake devices），同步替换，最终没有 backend/Renderer 的 `bMaterialLayout` 分支或固定 MaterialTextures[5] 通用字段。

### D10. 调度和有限 graph 改动

Renderer 在 Render 上冻结材质、选择 pass、验证声明并得出队列/目标需求；RHI 0 只物化已冻结计划和准备 native 资源。保留全场景 Transparent 稳定中心深度排序及相交透明限制。PBR 镜像 determinant sign 影响最终 front face 和 normal/tangent 参数；PSO key 必须包含有效 winding。

兼容 target/view 的 scene draws 在同一 graph pass 内允许切换 pipeline、depth test/write、blend、cull、stencil ref；不再仅因某个 draw 不测深度而创建新 pass。一个兼容段只要有 draw 使用 depth/stencil 就绑定对应 attachment，并在该 view 的首次需要时初始化；不使用它的 draw 由自身 pipeline 禁用。仅因 target view（linear/sRGB）、viewport/view 或外部显式用途边界切段，保留队列顺序而非为合并重排。图的实际 pass 数超 context 上限继续明确拒绝，不承诺任意复杂图。

FPassCommands 添加目标签名、viewport 和有限 depth/stencil load/clear 信息；Compile 对 depth 与 stencil 内容分别跟踪初始化（stencil 清零），禁止无 stencil 格式时使用/清理 stencil。color-content 及 Present transition 规则不变。先 clear 后 masked/transparent 绘制，不能因为材质切换重复 clear。单采样下 A2C 请求在 material resolve 失败；不写 false 后继续绘制。

### D11. 验收设计与证据

| ID | 必须验证的结果 | 层级/入口 |
| --- | --- | --- |
| M01 | CPU 定义/实例跨模型共享，更新隔离，陈旧 handle/type/缺失参数明确失败，Scene 不链接 RHI | 新 material_contracts + dependency_boundaries |
| M02 | DXIL/SPIR-V/MSL 来源反射，struct/array/matrix/bool、非零 space、stage 合并/冲突、inactive 参数、冷/热 cache 一致 | 现有 shaders（shader_tests）扩展 |
| M03 | 任意 uniform 名与 semantic 映射相同槽、provider/override/clear 顺序、Locked、缺失/冲突语义 | 新 material_bindings CPU 测试 |
| M04 | 同 View/不同 Object 共用 View buffer+offset；相同 material 共用 Material slice；双 View/family 隔离；同 primitive 多 item 不串值；混合 block/派生矩阵完整失效；复杂类型真实 GPU 打包 | material_bindings + 新 material_rendering GPU fixture |
| M05 | 0/1/8 张纹理、固定资源数组、多个 cbuffer、VS texture、structured/raw SRV、动态 sampler，带纹理但无 depth | material_rendering + rhi_backend_contracts |
| M06 | compare/write、前后 cull/mirror、blend RGB/alpha/write mask、stencil 两面/ref/mask 的像素或 GPU readback；非法 A2C/MSAA/MRT/资源类型明确失败 | material_rendering + d3d12_device_ownership |
| M07 | 参数/纹理/材质替换不增加 geometry uploads；不同模型的相同 PSO/资源 binding set 复用；稳定帧不重复分配/复制 descriptors；布局/state/target/view 不同不误命中 | render_resources + scene_rendering |
| M08 | freeze 后更新、迟到结果、上传/descriptor 分配失败、heap 耗尽、record/Present 失败；无新帧退休；关闭后 lease 存活 | render_primitives/render_resources/d3d12_frame_failure_recovery |
| M09 | 三插件/GUI 画面与交互回归、PBR alpha/UV/sRGB/mips、共享实例、clip/位移保守 bounds 和全场景透明排序 | 现有 GPU/集成 suite |

材料测试 fixtures 使用临时 HLSL/CPU 几何和现有真实 D3D12 窗口/readback harness，生成物放 out。M04 使用两个 viewport 的同一图、两个已知相机值和像素断言；同时检查 native slice identity。另一个 shader 读取嵌套 struct、固定数组、bool/int/uint、row-major 与 column-major 非方阵/数组的不同分量，将校验结果编码为各像素区块并 readback，期望值由独立 CPU 数值表达式计算，不复用待测 packer 生成 oracle；同时验证冷/热缓存路径。多 item fixture 从同一 primitive 输出不同 World/Draw 参数并调整发射顺序，逐项检查结果和缓存隔离。M06 stencil fixture 选择 D32S8；A2C/comparison sampler 只验证拒绝/有效状态查询，绝不将单采样或 RGBA8 常规采样当对应效果证明。能力超限测试用受控小 heap，不依赖显存耗尽。定义换版测试验证旧 name handle 不可复用。

实现最终验证：`python tools/CheckStyle.py`、`python tools/CheckBoundaries.py`、Ninja compile database 的 `--naming --build-dir out/build/debug`、VS Debug/Release `GenerateSolution.ps1 -Test -Configuration ...`，以及 change/all strict validation。GPU/desktop 不可用时记录阻塞，不能把 fake-device 通过当像素验收通过。计划阶段仅执行 OpenSpec 和文档一致性检查，不运行实现测试。

## Risks / Trade-offs

- 多个资源类别打破原模型打包的默认生命周期假设 → 在一个现有 coordinator 内扩展记录，M08 使用 fence gate 验证 partial failure 和无帧退休。
- Shader 优化和跨目标 ABI 差异 → 真实目标反射 + canonical block 活跃成员校验；不能只用 C++ sizeof 或一套 SPIR-V 布局猜测 DXIL。
- 任意自定义 cbuffer 无法自动按频率拆分 → 反射打包保持兼容，完整依赖 key 避免错误复用；共享标准块作为明确优化路径。
- 固定 descriptor heaps 有容量上限 → Query 报告、创建前预算和完整回滚；本任务不实现 bindless 或 heap paging。
- 有界 constant page 仍可能被长寿命 slice 保留 → 独立统计、整页回收和空闲页上限；不在此任务实现通用 GPU 内存碎片整理。
- 材质声明与 backend 可用性不同 → pass resolve 返回原因并在提交前失败，默认 builtin 只选首版可执行能力。

## Migration Plan

按 tasks 顺序先扩展反射及 RHI，再实现 Materials/Renderer，最后迁移所有调用方并删除临时兼容分支。每阶段保持现有 build/test target 名称；允许短期适配器保持编译，但最终验收不允许旧模型专用执行路径。整个变更不修改持久化协议，回退可以整体回退本 change 的源码/文档修改，无用户资产迁移步骤。计划审计完成不等于授权实施、归档或提交。

## Open Questions

首版能力、数据来源、更新策略、模块依赖、共享/缓存、迁移和验证路径按上述决策执行；计划审计发现的具体缺口记录到 `plan-review.md` 并修订后复审。不得以“实现时再决定”保留会影响可执行性的架构选择。

如果已有基线直接证据证明必需修复涉及完整阴影、MSAA/离屏图、资产持久化迁移、Tasks/提交模型重写或其它明显超出材质范围的大量工作，暂停该依赖方向并请求用户决定；与本次验收无关的既有问题记录范围外，不纳入 tasks。
