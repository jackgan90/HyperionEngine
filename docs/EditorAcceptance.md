# Editor 验收用例维护

Editor 的合成输入、场景状态和验收断言属于测试实现。新增或维护用例时，先明确场景所有者、输入目标、等待条件和完成结果，再选择对应的状态与上下文。共同遵守 [代码规范](CodingStyle.md#可维护性与语义表达)、[源码组织](SourceLayout.md) 和 [验证指南](Verification.md)。

## 实现入口与边界

场景实现位于 `Source/Plugins/Editor/Tests/Acceptance`，单元测试及其专用 support 位于 `Tests/Unit`。生产 `Private` 只保留 driver 契约、控件 observations、report 和 unavailable 实现；生产 include 闭包不能到达 Tests。`BUILD_TESTING` 关闭时，不编译验收场景或保存场景状态。GUI 与领域操作仍由生产服务执行，验收通过合成输入和语义观察使用这些服务，不能另建一套测试专用业务流程。

| 入口 | 职责 |
| --- | --- |
| [AcceptanceStates.inl](../Source/Plugins/Editor/Tests/Acceptance/AcceptanceStates.inl) | 各场景执行状态的声明；包含资产状态声明 |
| [AssetAcceptanceStates.inl](../Source/Plugins/Editor/Tests/Acceptance/AssetAcceptanceStates.inl) | 资产状态及其阶段、输入窗口元数据的单一声明源 |
| [AcceptanceTransition.h](../Source/Plugins/Editor/Tests/Acceptance/AcceptanceTransition.h) | 生成状态枚举与诊断名称；`TAcceptanceState` 只保存执行状态 |
| [AcceptanceScenarioContexts.h](../Source/Plugins/Editor/Tests/Acceptance/AcceptanceScenarioContexts.h) | 具体场景或操作拥有的输入进度、等待预算、观测、快照和一次性标志 |
| [AcceptanceInput.h](../Source/Plugins/Editor/Tests/Acceptance/AcceptanceInput.h) | 测试私有的点击、文本输入、输入节奏和帧等待原语 |
| [AcceptanceBounds.h](../Source/Plugins/Editor/Tests/Acceptance/AcceptanceBounds.h) | 共享控件与反射属性的类型化 observation 键及 bounds 访问 |
| [ReparentAcceptanceContext.h](../Source/Plugins/Editor/Tests/Acceptance/ReparentAcceptanceContext.h) | Reparent 夹具角色、记录及有序用例配置 |
| [AcceptanceContexts.h](../Source/Plugins/Editor/Tests/Acceptance/AcceptanceContexts.h)、[AssetAcceptanceContext.h](../Source/Plugins/Editor/Tests/Acceptance/AssetAcceptanceContext.h) | 执行状态到动作、实际用例索引、阶段及窗口的显式只读映射 |
| [EditorAcceptanceState.h](../Source/Plugins/Editor/Tests/Acceptance/EditorAcceptanceState.h) | 聚合场景上下文；承载预览、视口等聚焦上下文及验收结果 |
| [EditorAcceptanceHarness.h](../Source/Plugins/Editor/Tests/Acceptance/EditorAcceptanceHarness.h)、[EditorAcceptanceHarness.cpp](../Source/Plugins/Editor/Tests/Acceptance/EditorAcceptanceHarness.cpp) | 场景方法边界、`CollectInput` 调度、窗口路由、截图与完成判断 |
| [EditorAcceptance.cpp](../Source/Plugins/Editor/Tests/Acceptance/EditorAcceptance.cpp) 及同目录专项实现 | 输入 helper 和具体场景的事件、断言、显式状态跳转 |

`HYP_ACCEPTANCE_BEGIN/STATE/END` 和 `HYP_ACCEPTANCE_ASSET_STATE` 在 `AcceptanceTransition.h` 的展开位置局部定义，读取 `.inl` 后立即取消定义；资产元数据在 `AssetAcceptanceContext.h` 再次局部展开。维护声明源及对应映射，不把这些宏作为生产接口或全局宏使用。`Name()` 提供诊断文本，控制流使用类型化状态。

`FAssetAcceptanceContext` 是 `DescribeAssetAcceptance` 返回的阶段/窗口描述；`FAssetScenarioContext` 才是持有输入和观测的可变场景上下文。其他只读映射同样不能承担运行进度。

## 状态与字段的语义

1. **执行阶段使用场景专属枚举，跳转明确写出目的地。** 通过 `Progress.Is`、`IsAny` 或 `GetState` 分派，通过 `TransitionTo(E...State::...)` 跳转。值初始化从声明中的首个枚举项开始，因此默认首项须保持场景初态；需要不同起点时显式选择。维护状态声明时核对初态，其余分派及跳转不得依赖序号、范围比较、加减或隐含的“下一步”。
2. **场景及子场景拥有自己的输入与等待。** 例如滚轮场景使用 `Scenario.Wheel.MovementSample`，普通交互点击使用 `Scenario.Interaction.Click`。不能借用父场景或无关场景的等待字段来完成子场景；通用 `TAcceptanceState` 不存放输入或计数器。
3. **一个字段只表达一种职责。** 执行状态、输入阶段、帧预算、观测计数、用例索引、初始化标志、截图请求和验收结果分别建模。两个不同含义的等待使用具名字段；不能把同一 `Wait` 改名为 `Frames` 后继续按不同阈值解释，也不能靠计数值推断是否初始化、按键阶段或是否已经截图。
4. **有限选择使用表意明确的类型与选项。** 点击延迟使用 `EAcceptanceClickDelay::Settle/Immediate`；文本提交使用 `EAcceptanceTextCommit::KeepEditing/Enter`；输入节奏使用 `EAcceptanceCadencePhase::ReleaseKeys/Settle/Execute`。阶段和策略不能用 `0/1/2`、取模或含义模糊的布尔参数代替。确实只有“已初始化/未初始化”等二元事实时使用具名 `b` 标志。
5. **真实数量可以保留整数，但必须保留单位和归属。** 例如 `MovementSampleFrames` 是运动采样帧数，`CaseIndex` 是实际夹具索引，预览拖动使用具名采样帧预算。阈值应有场景语义、计数起点及有效条件；不能把这些数量重新用作执行阶段。相关动作、阶段和窗口通过显式映射确定，不能从枚举位置推导。

模型预览的 `FAssetPreviewExercise` 分别拥有 `PositionControlSettle`、`DragSample` 和 `HistorySettle`。控件稳定等待在第 4 次有效更新继续；拖动采样在前 48 次更新发出移动，第 49 次释放；历史刷新在第 3 次更新后检查预览就绪，成功处理后重启该历史等待。这些职责各有预算和复位边界，不共用一个 `Frames` 字段。

现有滚轮上下文把状态和采样预算分开：

```cpp
struct FWheelAcceptanceContext
{
	static constexpr unsigned MovementSampleFrames = 6;
	TAcceptanceState<EWheelState> Progress;
	FAcceptanceFrameWait MovementSample{MovementSampleFrames};
	FSceneCameraPose Before;
	float InitialSpeed{};
};
```

交互点击由 helper 返回本次点击是否完成，调用者决定下一状态。以下只展示首次打开文件菜单的分支；实际实现还显式处理重新打开菜单的目的地：

```cpp
if (ExerciseClick(InEvents, Scenario.FileMenuBounds, Scenario.Interaction.Click))
{
	Scenario.Interaction.Progress.TransitionTo(EInteractionState::OpenSceneMenu);
}
```

`TransitionTo` 只改变执行状态，不重置输入或等待，也不决定是否继续执行本帧后续代码。进入采样阶段时由场景显式 `Start` 或 `Restart`；循环、重试和复用输入时明确复位位置。保持 `break`、`return`、同帧调用及事件顺序的实际含义，不能因改用状态名而改变行为。

## 输入与计帧边界

控件 bounds 使用 `FWidgetKey`：固定控件复用生产 observation 的 `EEditorWidget`，列表项携带稳定 ID 或生产 observer 发出的 wire value；不能用显示文本、字符串前缀或枚举顺序识别目标。反射属性使用独立的 `FPropertyKey{Component, Field}`，保留权威 component type ID 与完整 field path。Component header 属于控件，以 type ID 标识，并非名为 `header` 的属性。动态键拥有字符串，不能保存临时 `string_view`。

`Scenario.Bounds.Require` 要求目标已被观察，缺失时失败；`Contains` 检查是否记录；`FindOrEmpty` 在尚未绘制时返回空矩形供输入 helper 等待，且不插入记录。记录端和所有读取端必须共同使用此契约。捕获条件与 surface 清理边界由 observer 保持，不能借迁移改变生命周期。

Reparent 的 `CaseIndex` 只遍历有序 `ReparentCases`。每项明确选择角色、拖拽源、Scene root/对象/窗口外目标、过滤、取消方式和结果；夹具记录集中拥有 handle、稳定 ID、初始 world 与父关系快照。成功用例声明被移动的根角色，取消、拒绝及 no-op 保留操作前父关系。维护用例时同步配置及行为断言，不新增按编号判断的分支；fixture 名称和 ID 保持现有持久化契约。

| 原语或行为 | 计数与完成语义 |
| --- | --- |
| `FAcceptanceFrameWait::Start/Restart` | 设置或恢复预算，不消耗一次更新 |
| `Advance()` | 预算为 N 时在第 N 次有效调用返回 true；零预算立即完成。预算耗尽后保持完成，下一轮由所有者显式重启 |
| `ConsumeFrame()` | 前 N 次调用返回 true，使调用者保持等待；第 N+1 次返回 false，适合完整空等 N 帧后恢复的场景 |
| `FAcceptanceFrameObservation` | `Advance` 累计有效观察，`IsAt` 查询具名帧点，`Restart` 从零开始；它不代表执行阶段或自动超时 |
| `FAcceptanceClick` / `ExerciseClick` | 在控件 bounds 有效且连续调用的常规 `Settle` 路径中，前两次等待，第三次按下，第四次释放并返回完成。`Immediate` 明确选择跳过当前延迟；helper 不修改场景执行状态 |
| `FAcceptanceTextInput` / `ExerciseTextInput` | `SelectAllPress`、`ReleaseAndType`、`AwaitTextCommit`、可选 `ConfirmRelease` 分开；提交方式和稳定帧预算属于该文本输入上下文 |
| `FAcceptanceInputCadence` | 每次有效调用按 `ReleaseKeys`、`Settle`、`Execute` 返回当前阶段；调用者负责各阶段事件，不以 `% 3` 分派 |

“有效调用”取决于原有控件就绪、资源就绪、输入窗口、ready-frame 等门控。等待调用应放在所属阶段及门控内，不能改为每帧无条件累计。检查 `IsAt` 位于递增之前还是之后；移动这个查询会改变观测帧点。完整空等与在最后一帧继续执行也不能互换，例如模型放置的 `NextCaseSettle` 和资产关闭按钮的 `Hover` 使用 `ConsumeFrame`。

扫描阻塞后的交互恢复由 `RestartInteractionAfterSceneScan()` 显式重启当前所属观察和点击延迟，并恢复相关延迟策略。已按下但尚未释放的点击仍须保留释放职责。需要继承立即点击行为时传递具名策略，不能通过另一个场景“碰巧达到某个计数”来获得该行为。

一次性准备或持久的“已截图”事实使用场景所属标志；只在当前更新有效的截图请求显式清除并设置。现有 `CapturePreference.bCaptureBeforeToggle` 在 `CollectInput` 开始时清除，再由所属场景发出请求。截图消费者使用这些事实和语义状态，不能读取某个共享计数阈值。

## 完成结果与报告

组合交互验收的 `interaction_verified` 是布尔结果，由 `IsInteractionComplete()` 同时供退出判断和报告生成使用。它包含该场景终态及就绪条件，不能仅用“枚举已到末项”代替；其他场景仍使用各自完成/验证结果。测试关闭或未启用组合交互时该值为 false。

旧的 `ExerciseStep`、`LegacyReportStep` 和 JSON `exercise_step` 已移除。新增用例及报告消费者使用语义结果与现有行为断言，诊断使用状态名称，不再恢复历史步骤号。其他已有 CLI 参数、报告键和类型保持现有契约；需要改变外部契约时单独设计并同步消费者。

报告定义和序列化见 [EditorAcceptanceReport.h](../Source/Plugins/Editor/Private/EditorAcceptanceReport.h)、[EditorAcceptanceReport.cpp](../Source/Plugins/Editor/Private/EditorAcceptanceReport.cpp)，实际完成结果由 [EditorAcceptanceHooks.cpp](../Source/Plugins/Editor/Tests/Acceptance/EditorAcceptanceHooks.cpp) 提供；现有组合验收消费者见 [EditorAcceptance.py](../Source/Tests/Integration/EditorAcceptance.py)。

## 新增或维护用例

1. 确定所属场景和子场景，列清初态、终态、输入窗口、就绪门控、采样/稳定等待、截图时点以及验证结果。维护已有用例时对照原事件顺序及断言；新增能力须遵守 [自动化接入契约](Automation.md#新功能接入契约)，用例实现不能代替生产领域服务。
2. 在状态声明源中添加动作、等待或验证含义明确的状态。资产状态同时声明阶段和输入窗口；其他需要动作/用例关联的状态在 `AcceptanceContexts.h` 维护显式映射。不要另建按状态序号或范围识别阶段的规则。
3. 在所属上下文声明实际需要的点击、文本输入、节奏、具名帧预算、观测或一次性标志。相关但独立的等待分别命名；确定谁开始、谁递增、谁复位以及谁读取，避免跨场景写同一进度字段。
4. 在专项验收实现中执行事件和断言，通过 `TransitionTo` 明确目的地；helper 返回输入动作或完成结果。需要新增方法时同步 Harness 声明及 `CollectInput` 或对应专项调度，核对主窗口/资产窗口路由、扫描恢复、重试与取消、同帧事件和截图脉冲。
5. 新增 `.cpp` 时在 [Editor CMakeLists.txt](../Source/Plugins/Editor/CMakeLists.txt) 的测试构建分支接入源文件及相关单元测试 target；新增集成用例时按 [测试注册](../Source/Tests/CMakeLists.txt) 接入。维护既有用例不必增加测试注册。测试入口与夹具保持在测试目录，不能从生产源码引用场景头文件。
6. 完成判断使用场景的真实验证结果，断言观察行为、数据和生命周期，不比较历史阶段号。计帧断言只用于确实以帧数定义的采样行为。修改完成契约或新增场景时同步报告/消费者与本文有关说明。

## 验证维护结果

先构建，再查询并选择当前注册的相关测试；测试数量与配置有关。以下命令从仓库根目录执行，构建配置要求见 [VisualStudio.md](VisualStudio.md)：

```powershell
./tools/Build.ps1 -Preset debug
./tools/Build.ps1 -Preset release
ctest --preset debug -N -R '^editor_'
ctest --preset debug --output-on-failure -R '^editor_'
ctest --preset release --output-on-failure -R '^editor_'
```

按修改范围运行 [代码规范检查](CodingStyle.md#检查与格式化) 和相关既有回归。涉及源文件选择或 driver 边界时，在独立构建目录验证 `BUILD_TESTING=OFF` 的编译/链接和 unavailable 行为，并核对 `editor_acceptance_boundary` 的实际源选择。边界检查工具见 [EditorAcceptanceBoundary.py](../Source/Tests/Integration/EditorAcceptanceBoundary.py)。

人工审查仍须核对上下文所有权、显式目的地、帧数边界、输入释放/复位、窗口路由和完成谓词；测试通过不能证明不存在字段混用。纯文档修改只核对描述与当前源码、链接及差异，不把历史构建、测试数量或本机输出目录写成开发要求。
