# Instance 批次渲染

`FRenderSession` 默认在 Render 线程将兼容的可见 render item 合成 instance 批次；RHI 0 创建实际 draw packet 和常量数据切片。Scene 模块不依赖这条渲染路径。Scene Viewer 的 **Instance batching** 复选框可实时切换，`--no-instance-batching` 强制使用普通绘制，适用于同一个可执行文件的 A/B 对照。CLI 强制关闭时，界面显示关闭状态及对应选项，替代无法生效的复选框。

## Shader 宏契约

引擎为普通版传入 `HYP_ENABLE_INSTANCE=0`，为可选的 `Instance` permutation 传入 `HYP_ENABLE_INSTANCE=1`。当前没有引入 shader metadata system，也不按文件名或源码字符串判断支持情况。shader 应采用可被编译参数覆盖的默认值：

```hlsl
#ifndef HYP_ENABLE_INSTANCE
#define HYP_ENABLE_INSTANCE 0
#endif

struct FInstance
{
	column_major float4x4 World;
	float4 Tint;
};
cbuffer ObjectData : register(b1)
{
#if HYP_ENABLE_INSTANCE
	FInstance Records[128];
#else
	column_major float4x4 World;
	float4 Tint;
#endif
};
```

使用 `#if HYP_ENABLE_INSTANCE` 选择路径。instance 顶点入口接收 `uint InInstanceId : SV_InstanceID`，据此索引 `Records`；像素阶段需要实例参数时，通过 `nointerpolation uint` 传递索引。普通版直接读取 `World/Tint`。不需要实例 ID vertex stream，native start-instance 固定为 0。

材质 pass 声明数组与逻辑参数之间的映射：

```cpp
Pass.InstanceArrays = {{"ObjectData", "Records"}};
Pass.bAllowBatchReordering = true;
```

这份声明描述数据布局；是否存在可用的 instance permutation 仍由编译和反射决定。每个声明的 block 必须包含一个从 offset 0 开始的固定结构体数组，stride 为 16 的倍数，完整数组不超过 64 KiB。编译器将 `Records[i].World` 映射为普通逻辑路径 `ObjectData.World`，保留 parameter schema、semantic provider、作用域和 override 优先级。记录内部支持数值、矩阵、嵌套结构与固定数组，不局限于 Model 类型。

实例版必须反射出系统 instance-ID 输入与有效数组，且其活跃逻辑输入必须由普通 pass 提供。未声明数组、shader 忽略宏、源码将宏固定为 0、布局无效或可选编译失败时，保留有效的普通版。`FCompiledMaterialDefinition::FindInstancePass()` 查询结果，`InstanceDiagnostics` 保留被拒绝的具体原因。反射不替代 shader 作者对索引和访问语义的责任。

Model 的 Object/Surface 数值使用实例数组，View/Scene 数据仍为共享常量。Model、Triangle、GUI 均具有 0/1 路径；GUI 当前保持有序普通绘制。

## 兼容性与独占绘制

`IRenderBatchStrategy` 提供 `Evaluate` 和 `CanCombine`，`FInstanceBatchStrategy` 是首个实现。通过 `FRenderSession::GetBatchSystem().Register()` 在 Main 上、第一次 Build 前注册额外策略。较晚注册的策略优先；协调器为每个源 item 选择一个策略，保证一次计划中的源索引恰好覆盖一次。后续新增执行方式时可扩充计划 payload，并复用这一调度与独占协议。

兼容性比较以下实际输入，不要求材质实例或 Model 对象相同：

- 共享几何资源身份、geometry/section 索引范围、vertex attributes、stride 和 topology。
- 编译 shader、常量物理布局、有效固定与动态状态、镜像后的绕序、目标格式。
- texture 来源/版本/编码，buffer 来源/版本/视图范围/stride，完整 sampler 值和资源数组顺序。
- 非实例 block 的有效数值。实例 block 内的不同数值不阻止合批。

哈希只定位候选，全量比较决定兼容。仅显式允许重排、启用 depth test/write、无 blend/stencil 的 opaque/masked pass 可使用当前策略。其他 item 构成排序屏障；不会跨越 transparent、overlay 或其他有序工作。每个 view/usage 独立规划，不跨 view 合成 draw。

## 缓存与生命周期

稳定 item 身份为 Scene/Slot/Generation/LocalItemId。匿名 item 可在当前帧合批，但不跨帧复用数据。原语 revision 不是唯一失效依据：实际 emitted item、最终参数、成员顺序、可见集合、布局和资源都参与判断。

系统缓存兼容性描述、完整的静态计划和不可变的紧凑实例记录。成员和数值不变时复用 CPU 数据及 GPU slice；剔除、成员排序或实例值改变时只重新填充受影响的 chunk。共享 View 常量变化不要求重新上传 Object/Surface 实例记录。当前采用紧凑可见列表，没有持久 slot 间接索引。

默认兼容性条目上限 4096；静态计划最多 16 个 view/usage，合计不超过 4096 个输入条目。实例 chunk 最多 512 个，记录及保留数值树按保守估算受 16 MiB 预算限制。GPU instance slice 缓存另有 512 条目/16 MiB 上限。缓存条目不保留 native geometry/texture lease；旧 packet/list/fence 独立保留其引用的 native 页面。`MaterialConstantStats::InstanceBlocks/InstanceBytes` 单独报告实例缓存，原有 `CachedBlocks/CachedBytes` 保持原语义。

发布后的常量字节不被覆盖。删除对象后，过期 owner 触发资源收集；CPU 缓存在下一次规划时清理。关闭 session 时先在 Render 清空批次缓存，再关闭 Scene 和 RHI 资源。失败 Present、取消录制和旧帧仍按已有 fence 退休规则处理。

## 提交与失败处理

Render 在剔除和材质求值后创建计划。RHI 预检所有源 item，再准备批次或普通 draw，最后统一发布。某个模型组的 section 失败时，该组所有成员均不绘制；如果成员已进入其他暂存批次，会重新压紧剩余成员。批次专属准备错误可以回退为单独绘制。原始 item 仍各自获得 frame/family/view/usage/revision 对应的报告。

材质就绪阶段只准备必需的普通变体资源。可选 Instance 版的 native layout/binding 在批次准备路径中按需创建；超出设备绑定限制或实例版 pipeline 准备失败不会使有效普通材质整体失败。已知设备限制可在规划时回退，其他 native 准备错误由批次路径捕获并回退。

`FDrawPacket::InstanceCount` 默认为 1。布局保留完整 shader extent、instance stride/capacity；native 校验 shader 数组形状与布局匹配，并验证实际提交数量所需的切片大小、256 字节切片地址对齐、每阶段常量数量和 root signature 限制。超出容量的组拆成多个 draw，最后只有一个成员时使用普通版。

## 验证和性能

`instance_batching` 覆盖 DXIL/SPIR-V/MSL 反射、类型打包、真实 GPU 图像、容量/短切片拒绝、策略独占、资源/状态拆批、缓存预算、成员变化、多视图、整组失败修复与旧帧保留。`scene_moving_camera` 用同一 Viewer 分别开启/关闭合批，检查图像和可见数量一致、draw 显著减少、上传量有界。

Viewer CSV 保留 `frame/frame_ms/scene_draws`，新增可见 item、实例/普通 draw、失败数量、chunk 复用/重建、打包/上传字节、GPU slice 复用以及规划/准备耗时。末尾的 `fallback_disabled/shader/device/ordering/singleton/preparation` 六列分别记录对应原因；界面显示非零原因及数量。回退计数反映规划和准备阶段事件，应使用 `single_draws` 判断最终普通 draw 数。多 view family 的批次事件与耗时累计，缓存存量取最后一次观察值。性能结果及复现命令见 [InstanceBatchPerformance.md](InstanceBatchPerformance.md)。
