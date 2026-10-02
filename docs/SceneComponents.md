# 场景组件与编辑契约

编辑状态由 Main 上的 `FSceneNode` / `FSceneComponents` 持有。对象有稳定的文档 ID、名称、启用状态和组件实例；组件有实例 ID、类型 ID 和值状态。没有 ECS、隐式 Tick 或 UObject 生命周期。运行期 `FSceneHandle` 的场景身份、槽位和代次不能作为保存身份。

## 组合与扩展

`FSceneTransform` 必需且唯一，保存父对象 ID 和局部仿射矩阵。Camera、Model、Light 是可以组合的能力；`GetKind()` 只用于概括显示，能力查询使用 `Has()` 或类型访问器。内置组件每种最多一个，自定义类型可声明多实例和组件依赖。`Node.Camera()` / `Node.Local()` 等兼容接口直接访问组件存储。

新增组件在所属模块定义值类型、反射和验证，并在加载场景前注册。例如：

```cpp
struct FMovementComponent
{
	float Speed = 1;
	bool bEnabled = true;
	bool operator==(const FMovementComponent&) const = default;
};

template<> const FRecordDescriptor& RecordType<FMovementComponent>()
{
	static const auto Type = MakeRecord<FMovementComponent>(
	    "example.movement",
	    {Member("speed", &FMovementComponent::Speed, Inspect("Speed", 0)),
	     Member("enabled", &FMovementComponent::bEnabled, Inspect("Enabled"))});
	return Type;
}

// 在所属模块的初始化入口执行一次。
SceneComponentRegistry().Register(MakeSceneComponent<FMovementComponent>("Movement"));
```

`FRecordMemberOptions::Inspector` 控制顶层属性显示，`FPropertyPresentation` 提供标签、分组、只读、范围、枚举选项、引用类型标记、容器大小限制，以及控件类型、单位和提示。`bPersistent` 独立控制持久化。嵌套记录默认显示序列化字段，可覆盖呈现元数据。支持标量、枚举、可选值、记录、序列、已有 map 项和完整矩阵；`Vector3` 控件为反射的 `FVec3` 提供紧凑的 X/Y/Z 行，`Color3` 将线性 RGB 显示为 sRGB 色块、色轮和 0～255 的 R/G/B 分量。引用标记不自动生成资产选择器；map 键新增、通用资产选择器、Gizmo 尚未实现。

`Components.Add(instanceId, typeId)` 添加实例。自定义组件无需增加 Editor 面板文件或修改持久化 DTO；影响渲染或其他子系统的行为仍由该子系统实现。多实例组件通过实例 ID 查询，类型访问器只表示该类型的首个槽位。

反射编解码和值验证也用于 Worker 上的资产管线，应只依赖传入的 CPU 值，不能访问活动 Scene 或 Render 对象。涉及父子关系、组件依赖和派生世界变换的约束由 Main 上的场景事务统一检查。

## 编辑与线程

### 存储值与显示布局

当字段的存储形式不适合直接编辑时，可在组件的 `FRecordDescriptor::DisplayLayout` 上指定 `FRecordDisplayLayout`。`MakeRecordDisplayLayout<TSource, TDisplay>(Project, Apply)` 将组件投影为另一个反射值类型，通用 Inspector 根据该显示类型生成控件。`Project` 只读取源值；`Apply` 接收源候选副本、编辑后的显示值和最初的显示值，按需写回。它们应是纯 CPU 转换，不能访问活动场景或 GUI。显示值先经过反射校验，再转换、验证源组件并提交原有场景事务；持久化仍使用源描述的成员。显示布局不参与资产格式，也不要求新增独立 Editor 面板文件。不支持递归或嵌套的显示布局投影。

Transform 的映射位于 Scene 模块的 `SceneTransform.cpp`，显示为 **Transform (Local)** 下的 Position、Rotation、Scale 三行；父对象 ID 在 Inspector 中隐藏，层级数据仍然保留。Rotation 的 X/Y/Z 是绕对应固定轴的右手欧拉角，单位为度，顺序为先 X、再 Y、最后 Z；列向量矩阵为 `Rz * Ry * Rx`。它表示相对于父坐标系的局部变换。控件标明各轴，悬停可查看约定。

权威状态仍是完整仿射矩阵。映射将线性部分分解为旋转和上三角伸缩，保留未显示的剪切系数；仅打开或不改 TRS 时不重建矩阵；仅修改 Position 精确保留原线性部分。Rotation/Scale 编辑保留剪切系数并重建线性部分。负缩放使用确定的表示（非奇异反射归于 X），零缩放使用确定的补全基；欧拉角、负缩放和万向锁附近存在等价表示，实时提交后显示值可能规范化，但几何变换保留。输入有限值与最终场景约束都需要通过验证。

Sponza 在场景中是一个完整模型实例，103 个 primitive 的顶点已经位于共用模型坐标系的不同位置，约 `0.008` 的源节点缩放保留在模型资产中。Inspector 的 Transform 表示整个模型实例的摆放；单位 Transform 不表示模型内部没有缩放。该显示映射不会重定位原点或把 pivot 移到每块几何中心。

### 事务与渲染诊断

```text
Main: component → draft → detached candidate → validated scene transaction
                                                  ↓
Main: Save snapshot                         render publication
                                                  ↓
Render: primitives → copied diagnostics → Main read-only diagnostics service
```

输入缓冲允许暂时无效。Inspector 每帧从当前组件投影显示值，在控件报告变化时把 `FRecordDraft` 解码到对象副本，检查只读字段、范围和组件验证，再经 `FScene::EditNode` 检查完整对象、父子关系和派生变换。句柄或预期 revision 过期时拒绝写入，失败不改变权威状态。调用方必须先操作副本再提交；组件验证负责所有运行期约束，呈现范围不能代替完整验证。

Renderer bridge 把组件发布为派生状态。Main UI 不读取 `IRenderPrimitive` 或 GPU 对象。`GetComponentDiagnostics()` 返回只读副本，包含对象/组件身份、发布 token、预期 primitive revision、已应用矩阵/可见性及最近 draw 的 frame/view/pass。状态可能落后；`bApplied` 表示版本是否追上，最近 draw 不能解释为本帧一定会绘制。诊断不持久化。Scene 继续独立于 Renderer / RHI。

### 组件变更事实

`FScene::GetChanges()` 返回拥有自身值的变更副本。每条 `FSceneChange::ComponentChanges` 按 `(TypeId, InstanceId)` 排序，同一身份只出现一次；两个 ID 都是 owned string。`ESceneComponentChangeFlags` 的 `Added`、`Modified`、`Removed` 表示自上次有效确认以来已经发生的变更，`HasComponentChange` 查询其中的标志。`Node` 仍表示该句柄最新的完整状态，删除项没有 `Node`，组件事实不携带已删除的组件值。

差异比较发生在 Scene 共享事务的提交准备阶段：以权威旧值和最终候选值配对 live 已注册组件，通过类型描述的 `Equal` 判断同身份的值变化。空 optional 槽位与 opaque 未知封装不产生 typed facts。仅 opaque 封装变化仍沿用 Metadata 变更与原有保存拒绝规则。组件实例重命名产生旧身份 `Removed` 和新身份 `Added`；在同一个实例 ID 下替换类型产生两个不同类型的事实。等值重排、空槽位以及候选内部增删后恢复原值不新增 revision 或事实。

未确认的多次已提交事务按完整场景句柄（含 generation）合并标志。因此先新增后删除保留 `Added | Removed`，先修改再恢复原值仍保留 `Modified`；它们是发生记录，不是相对上次确认状态的净差异，也不是按操作排序的日志。每条变更的 revision 更新为最新事务；`Acknowledge` 只移除 revision 不晚于确认值的整条记录，不会按标志拆分较新的记录。槽位重用产生新 generation，旧删除记录和新新增记录保持分离。调用方已保留的 `GetChanges()` 副本在后续编辑、删除、Clear 或确认后仍有效。

初次 `BeginSynchronization()` 为所有当前 live 组件加入 `Added`，保留已有未确认标志，并发布无组件事实的 settings 项；它不增加场景 revision。结束同步后可重新开始。重设父级与保留子节点删除同样经过共享准备：实际 Parent/Local 变化产生 Transform 组件 `Modified`，只有继承 World 或 effective-enabled 变化的后代不产生局部组件事实。

现有 `ESceneChangeMask` 继续表达消费者兼容效果。Scene 私有适配在本次差异涉及 Model、Camera 或 Light 时比较原有语义值；只重命名唯一内置组件的实例 ID 不触发这些效果，也不额外弄脏查询索引或渲染资源。查询失效只使用当前事务的效果，未确认的旧 mask 不重新制造失效。所有新增 `Get`/`Equal` 回调、事实分配与合并均在首次权威写入前完成；准备失败保持 Scene 值、revision、待确认变更和查询状态不变。GUI 与 automation 继续使用相同的 SceneEditing 服务及其既有历史契约，本事实 API 不新增操作 ID 或持久化格式。

## 灯光阴影属性

Directional Light 记录版本 3 增加整数 `priority`（默认 0）；版本 2 增加可选 `shadowSettings`，包含独立的 `directional` 和 `contact` CPU 参数组。Details 的 **Override Shadow settings** 创建或清除该值，组内编辑沿用组件验证、事务、撤销/重做和原生场景保存。版本 1 读取后保持无覆盖值，不在加载时修改资产；渲染继续采用兼容的会话默认设置。Priority 选出的阴影方向光使用自己的参数，不会把上一盏灯的值写入默认配置。`castShadows` 同时约束两类阴影。

这些类型位于 Scene，不依赖 Renderer 或 RHI；Renderer 从不可变场景发布中解析 Priority 推导出的阴影来源及其设置。当前 Point / Spot Light 不暴露阴影字段，未来可扩展相应方法组。Details 不再附加渲染诊断伪组件，运行时 primitive 诊断只通过只读服务查询。

## 模型层级与共享

模型 schema 3 保存源节点和 primitive ID。schema 2 只在读取内存中补出确定 ID，schema 1 内嵌材质不由当前 importer 拆分，需从原始 glTF/GLB 重新导入。缺少源 ID 时生成 `node-N` / `primitive-N`，保证同一源排列及兼容重建稳定，不承诺自动合并上游重排或重新拓扑后的编辑。

默认 Model 组件引用完整模型；导入模型为场景和原生场景兼容加载均不自动展开模型节点或 primitive。场景只持有实例摆放和实例属性，模型内部层级与变换留在资产中。渲染使用 `对象世界变换 × 模型内部节点变换`。组件保留整体和 section 材质 override，不增加场景拓扑，也不改写共享模型或材质。

模型实例共享 `FSceneModelData` 和模型 GPU 资源，一个 `FModel` 可以对应多个 render primitives；draw 数仍随合批、视图和 pass 改变。内部 primitive 可通过 `render.component_diagnostics` 查询，不要求与 Outliner 对象一一对应。render handle 或 draw 序号不属于保存身份。

已有展开场景的 `SourceNode` / `SourcePrimitive` 选择和子对象编辑继续兼容，兼容读取不会自动合并或丢弃这些状态。`ExpandSceneModels` 仅作为显式 CPU 编辑操作保留，当前没有 Editor 拆分按钮；后续独立部件功能必须由用户主动触发。Sponza 的整体引用迁移须核对旧子树与源模型，存在无法表达的子对象修改时拒绝合并。

## 文档与迁移

原生场景 schema 8、节点 schema 3 使用 `{id, type, state}` 组件封装；场景设置保存默认相机和可选 `initialView`，灯光来源由组件 Priority 推导。Model 组件的序列化类型 ID 继续使用 `hyperion.staticmesh`，持久化适配保留引用及材质选择，排除解析后的资源。旧字段只在读取时迁移，加载不会写回或改变拓扑。未知类型保留原始封装并标记不可用，保存明确拒绝，避免静默遗漏插件数据。

Editor 在合法输入变化时实时提交，并将同一控件的连续输入或同一颜色弹窗会话合并为一条撤销记录，保留最初状态与最新状态。即使值改回原值，记录和修改标记仍保留。Ctrl+Z 连续逐条撤销，Ctrl+Y / Ctrl+Shift+Z 重做；新编辑截断重做分支。Undo/Redo 重新验证，并清除当前控件的活动输入，防止旧值再次写回。切换对象、保存和其他文档操作结束当前编辑，不再需要 Apply/Revert。Save 冻结状态并使用 AssetService 原子写入，完成只标记捕获时的保存点，后续编辑仍为脏。失败保留脏状态，打开其他文档和退出检查未保存修改。

独立 `FSceneCameraView` 复用导航控制器，经 `FSceneViewRequest::CameraOverride` 冻结给 Render。导航、曝光和 F/Home 取景不修改场景相机或撤销历史；编辑 Camera 组件属于文档修改。

`export-envelope` 导出原生记录 JSON 供诊断，不作为 importer 输入。自定义组件和稳定 ID 保存在原生 `.hasset` 中，通过场景编辑和保存维护。外部模型/天空可从 glTF/GLB 或 HDR/EXR 来源导入。容器、依赖表与发布顺序见 [NativeAssets.md](NativeAssets.md)。
