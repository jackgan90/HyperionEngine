## 1. 材质 CPU 契约和模块边界

用户已授权实施并要求不做 Git commit。按组依赖顺序推进，在实现并验证后勾选任务；发现 design 中范围外的大量必需开发时暂停并由用户裁决。计划审计记录保留为实施前的固定快照证据。

- [x] 1.1 新建 Runtime/Materials target、Public/Private 结构和声明类型；仅依赖 Core/Math，Scene 声明直接依赖 Materials，更新边界检查覆盖传递渲染依赖。（D1，M01）
- [x] 1.2 实现不可变 definition、pass/state 规范化、单 Main owner instance、identity/revision 和只读 snapshot；验证重复 pass/define、错误 state 组合与失败编辑不发布。（D1-D3，M01）
- [x] 1.3 实现 typed schema/value、完整路径、带 schema version 的 handle、默认值/instance/draw override、definition replacement 校验。（D4-D5，M01/M03）
- [x] 1.4 实现 semantic registry、保留命名空间/别名/物理约定、Manual/Semantic/Locked/AllowOverride 与 clear 规则，增加 CPU material_contracts 测试。（D5，M01/M03）
- [x] 1.5 实现 CPU 描述、HasPass 与上下文可用性结果、队列和输入/bounds 要求；验证 A2C 声明不等于单采样执行、未提供阴影 pass 不报告支持。（D2-D3，M01/M06）
- [x] 1.6 实现拥有不可变 mip bytes 的 CPU texture source、read-buffer source/view、sampler/数组值及 identity/version，验证构造范围/编码/溢出和调用者存储释放；不依赖 Scene/RHI。（D1，M01/M05）

## 2. Shader 编译、反射与接口布局

依赖 1.3 的参数路径约定；公共 Shader reflection 不依赖 Materials。

- [x] 2.1 扩展 Compile options 和 artifact reflection 类型树、resource shape/stage/location 及 VS/PS signature；保留现有 Compile 便捷入口。（D4，M02）
- [x] 2.2 在私有 DXC adapter 实现 DXIL 成员/资源/signature 反射，包含 bool、数组、struct、非方阵和 matrix major；原生类型不出 Public。（D4，M02）
- [x] 2.3 扩展 SPIRV-Cross 反射、MSL 中间产物及生成资源映射；对查询支持的每个非零 space 应用版本化绑定映射，验证区间/碰撞。（D4，M02）
- [x] 2.4 实现 defines 规范化、重复拒绝、cache schema/mapping key 和从未剥离中间产物重建反射；扩展冷/热 cache、修改 include/options、损坏缓存回归。（D4，M02）
- [x] 2.5 实现 program 跨 stage 接口合并/冲突、stage 限定名称、schema-to-target 映射、未声明活跃参数导入和 Inactive 参数状态；补充歧义/拼写错误测试。（D4，M02/M03）
- [x] 2.6 实现 Renderer 的异步 PrepareMaterialDefinition 到 Materials-owned FPreparedMaterialInterface 的交接；Main 接受版本化 schema 后创建/迁移实例，验证 InterfaceNotReady、跨 variant 类型冲突、失败/取消/重试及旧 handle。（D4，M01/M02/M03）

## 3. 通用 RHI 资源绑定

依赖 2.1，native layout 构造依赖 2.5 的合并规则。

- [x] 3.1 定义 buffer usage/view/slice、sampler、binding layout/set 和实际 limits；添加所有新 payload 的 device identity 契约并更新 RHI fake devices。（D3/D7，M05/M08）
- [x] 3.2 实现 D3D12 只读 structured/raw buffer view 与常规 sampler 创建；验证范围、stride/对齐、参数值及 rollback；comparison sampler/SampleCmp 明确标记未启用并拒绝。（D3/D7，M05/M06）
- [x] 3.3 扩展现有 descriptor 管理为固定容量 resource/sampler heaps 的连续范围分配，区分 source descriptors 与 shader-visible table；测试部分分配失败和受控容量耗尽。（D7，M08）
- [x] 3.4 从 layout 生成 root CBV/table 及 immutable binding sets，处理 stage visibility、space、数组、非连续 registers 和 root budget；套用新资源保活链。（D7，M05/M08）
- [x] 3.5 扩展 buffer 页分配与未发布区域写入接口，验证逻辑范围、对齐和 published slice 不可覆写；为 coordinator 增加 slice/page 权威 ownership。（D6-D8，M04/M08）
- [x] 3.6 替换 packet 专用字段及 native recording 分支，接入布局/slot/device/upload/CBV/geometry 验证；VS 纹理上传最终状态支持实际 shader stages。（D7，M05/M08）

## 4. 图形状态、目标和有限 graph 适配

依赖 3.4/3.6。

- [x] 4.1 扩展 RHI raster/depth/stencil/blend/sample 描述及动态 stencil ref/blend constants；Renderer 使用显式穷尽转换和统一状态规范化结果。（D2-D3，M06）
- [x] 4.2 将 vertex layout/topology 归入 geometry，解析 pass 的输入/输出及 target signature；实现合法无 PS 路径，拒绝首版外 stage/target/resource 能力。（D2-D4，M05/M06）
- [x] 4.3 增加 session/swapchain 的 D32 或 D32S8 选择，保持单采样单颜色目标；映射真实 stencil state 并保留默认 D32 行为。（D3/D10，M06）
- [x] 4.4 扩展 FPassCommands viewport/target/depth-stencil 信息及 Compile 的独立初始化验证；支持同 frame 多 view 的明确边界，保持 Present/取消协议。（D6/D10，M04/M06/M08）
- [x] 4.5 更新场景队列与兼容 pass 聚合，混合 depth on/off draws 不按 draw 拆 pass；Opaque/Masked 共 bucket 保持等深交错次序，保留透明中心深度稳定排序、linear/sRGB 边界和 context 容量失败。（D2/D10，M07/M09）

## 5. 参数提供者、打包和实际 GPU 共享

依赖 1、2、3、4.4。

- [x] 5.1 增加 owned frame/context、view/pass/family 身份、scene provider snapshot 和 direct primitive 默认 scene；实现单 Render task 的 BuildViews 及唯一 pass 命名，单 view Build 委托它；接入 ViewerFrame 的 Main 参数冻结。（D5-D6，M03/M04）
- [x] 5.2 实现标准 Engine/Pbr semantic providers 和初始化期扩展注册，保留当前默认光照方向/颜色；验证完整优先级、缺失输入、Locked 与 clear。（D5，M03）
- [x] 5.3 定义带版本的 View/Object/Material 标准 HLSL block 和 CPU schema，按活跃成员反射校验 ABI；实现目标感知的 struct/array/matrix/bool 打包及 padding 清零。（D4/D6，M02/M04）
- [x] 5.4 实现按 layout/mapping/device/完整 scope dependencies 缓存的参数计划和 slice 复用；混合 block 与派生矩阵覆盖 view/object/material 变化。（D6，M04）
- [x] 5.5 接入持久与帧 constant 页分配、revision 新 slice、整页回收/有界空闲页，增加 provider/pack/upload/reuse 统计。（D6-D8，M04/M08）
- [x] 5.6 增加 material_bindings 测试，验证双 View、失活字段、不同 ABI 不误共享、相同 Material/View 的真实 buffer+offset 共享。（D6/D11，M03/M04）
- [x] 5.7 为零到多 item collection 定义 owned Object/Draw 参数与可选稳定 LocalItemId，默认 ordinal 限定在 collection 内；实现含生效内容签名的 key，覆盖同 primitive 同 revision 的不同 World/Draw 数据及发射顺序变化。（D6，M04）

## 6. 独立材质资源和完整发布

依赖 2-5。

- [x] 6.1 拆分 coordinator 的 geometry/material/texture/read-buffer 记录和公开租约；消费 1.6 的独立 CPU source 输入，保留批次 builder，资源 key 使用拥有身份的不可变 source/version/representation。（D1/D8，M05/M07）
- [x] 6.2 接入 material program/layout/PSO 结构化缓存，覆盖 state、input、target、mirrored winding，排除普通值和动态纹理/sampler；实现无用户缓存退休。（D8，M07/M08）
- [x] 6.3 分离 InterfaceReady、ResourcesReady 和 DrawPreparation/LastDrawResult；兼容 IsReady 在首次 Build 前就绪，group 暂存 packets 全成或全败，frame-context 错误不永久污染静态资源；保留 pending 静态替换的组策略。（D8，M07/M08）
- [x] 6.4 把新 native 类型加入现有 Process/CanRelease/Collect/Close；补齐 upload 已提交后 descriptor/material 失败、共享取消、迟到 revision 的清理/错误路径。（D7-D8，M08）
- [x] 6.5 实现 binding-set cache 的完整 layout/resource generation/view/array-order/sampler key、动态 CBV 分离及可退休 ownership，添加 descriptor 创建/复制/复用计数。（D7-D8，M07/M08）

## 7. Scene 和 primitive 的材质入口

依赖 1、5、6，保留现有 Scene/Render 消息和 generation 协议。

- [x] 7.1 给 CPU Scene 增加 model/section material selection 和 typed overrides；实现优先级、legacy 三项 PBR override 兼容，以及异步资产后 section/schema 验证。（D9，M01/M07）
- [x] 7.2 扩展 bridge 材质 revision 订阅，将 FModel 的 owned update 准备与 Publish 分离，在 Flush 冻结共享实例一次并全量验证后合并为关联 Render batch；admission 成功才确认版本/receipts，处理添加/复制/移除和实例解除订阅。（D8-D9，M01/M08/M09）
- [x] 7.3 将 primitive material 状态改为不可变通用 snapshot/lease 和局部参数覆盖，验证换定义、原子批次、旧帧保活与错误恢复。（D8-D9，M01/M08）
- [x] 7.4 将 bClipSpace 转为 primitive coordinate/bounds contract，更新 pre-collection/group/item 保守剔除及 material-switch invalidation，增加 clip-space/位移 bounds 回归。（D9，M09）

## 8. 现有调用方迁移

依赖 5-7。

- [x] 8.1 在 Renderer 私有 PBR adapter 将 FModelMaterial 转为 builtin definition/instances/semantic 资源角色，复用现有 mip/UV/颜色空间准备，保持 CPU 序列化字段不变。（D9，M09）
- [x] 8.2 拆分 Model.hlsl 的 View/Object/Material 常量与 Scene 光照输入，用通用 binder 替换 SceneDraws 的 FModelConstants，保持现有 PBR 输出数值。（D9，M04/M09）
- [x] 8.3 迁移 Triangle 为零纹理材质、geometry input、clip-space 参数；通过新通用 pipeline/bindings 渲染。（D9，M05/M09）
- [x] 8.4 接通 ModelViewer/SceneViewer 的共享桥接入口和 readiness/error，保留镜像/多实例/全场景透明及现有控件行为。（D9，M07/M09）
- [x] 8.5 适配 DebugUI pipeline、常量和字体纹理绑定，保留 overlay 剪裁和顺序；迁移所有直接 RHI 测试/fault injection/fake producers。（D7/D9，M08/M09）
- [x] 8.6 全仓确认移除通用路径的 bMaterialLayout、MaterialTextures[5]、512-byte 模型布局及临时兼容分支；核对旧 CLI、配置、plugin ID 和 build targets 未变化。（D9，M09）

## 9. 专项与回归验收

依赖相关实现；以下为交付门槛，不是计划阶段已执行结果。

- [x] 9.1 新增 material_rendering D3D12 fixture，验证零/一/八纹理、固定资源数组、多 cbuffer、非零 space、VS texture、只读 buffer 与动态 sampler 的真实像素。（M02/M05）
- [x] 9.2 同一图内 BuildViews 双 viewport/view 验证相机隔离、唯一 pass 名、共享冻结边界、对象矩阵、共享 slice identity/offset 和 pack/upload 计数；覆盖混合 cbuffer、camera-only 变更、同 primitive 不同 item/发射次序。（M04）
- [x] 9.3 用像素/readback 验证 blend/depth/write/cull/mirror/stencil state；对 A2C/MSAA/MRT/未支持类型和非法输入做明确失败断言。（M06）
- [x] 9.4 扩展 render_resources/scene_rendering 检查跨不同模型的 material/PSO/binding-set 复用、definition/state/layout/target/view/array-order key 分离；预热后仅改 Object 常量不创建/复制 descriptor，资源变化只替换受影响 set 且不重复 geometry upload。（M07）
- [x] 9.5 扩展现有 fence-gated lifecycle 测试，覆盖新 descriptor/sampler/pages 的失败 Present、上传后分配失败、替换/移除、heap 耗尽和无新帧退休；验证 direct RHI lifetime。（M08）
- [x] 9.6 执行 triangle/GUI/model/scene 既有 GPU 和集成验收，保持 PBR/mask/blend/UV/sRGB/mips/镜像/剔除/交互行为及零 D3D12 validation errors。（M09）
- [x] 9.7 增加复杂常量 GPU fixture：嵌套含 padding struct、固定数组、bool/int/uint、两种 major 的非对称非方阵及矩阵数组，通过独立 CPU 算术 oracle 和像素/readback 验证真实 pack/upload/bind；覆盖冷/热 Shader cache。（M02/M04）
- [x] 9.8 验证依赖 View/Object 的材质可在零帧条件达到 ResourcesReady、首次 Build 成功；一个 section 缺 provider 时仅该 model group 整组失败，后续兼容上下文恢复，旧 revision/frame 诊断不覆盖新结果。（M08）

## 10. 文档和最终验证

- [x] 10.1 新增 docs/Materials.md，更新 SourceLayout/RenderPrimitives/AssetPipeline 中材质、线程、共享和能力说明；记录首版限制和示例的 name/semantic 用法，不改历史归档。（D1-D11）
- [x] 10.2 执行适用的 style、boundaries、Ninja naming 和 VS Debug/Release 全量测试（命令见 design D11/docs/VisualStudio.md）；保留原始日志到 out，记录实际结果。（M01-M09）
- [x] 10.3 执行 `openspec validate add-extensible-material-system --strict`、`openspec validate --all --strict` 和最终 diff 检查；记录验收证据与未完成项，归档/提交按后续授权处理。
