# 验证与 reviewer 交接计划

这是未来 executor 的验证要求，不是本轮已执行的测试记录。实现前后的原始输出放在 `out/SceneOwnedCamerasAndLights/<run-id>/`；本 change 中后续只保存可读结论、相对日志路径和必要的小型证据，不提交 exe、生成的 hasset、shader cache、大型 trace 或 captures。

## V0. 基线与证据原则

- 记录实际 HEAD、工作区已有修改、构建preset/options、编译器、GPU/driver、validation状态、Tracy/RenderDoc开关、CPU frame lead、视口、depth convention、管线、CSM和batching配置。
- 规划基线是 `ee585cc88fd60a22a29dc2103cd4dafe6d234221`。executor开始时HEAD若不同，先检查差异是否改变本文契约；不是盲目reset。保留用户改动，不stage或覆盖非本change文件。
- 修改前冻结旧程序可复现运行环境：exe及runtime DLL、shader source、config/source/native assets及其hash。只复制exe但让其读取改过的shader/content不算基线。可在已授权工作区的独立baseline checkout/build中编译；本轮planner不创建该checkout或运行实现。
- 特别记录Sponza现有默认构图和Showcase固定计数fixture。性能命令必须显式指定`--scene`，不能依赖experiments/Scene.json后来切换的默认场景。
- CPU/GPU语义测试使用独立期望值，不能调用被测helper同时生成actual和expected。失败探针须能区分未处理更新/空场景/错误图像；不要只检查一个结构体“被写过”。
- 旧tests因API或v4字段改变可迁移fixture/调用方法，禁止删掉原来断言、降低覆盖、提高timeout掩盖性能问题或将实际GPU tests换成仅mock。

## V1. 固定验收矩阵

| ID | 层级/建议入口 | 固定操作与预期 |
| --- | --- | --- |
| N01 | Scene CPU | 五类节点增删/枚举；stable ID重复拒绝；slot删除再以不同kind复用，旧/foreign handle不产生变化；no-op不增加版本 |
| N02 | Scene CPU | 逆序nodes和至少4096层合法层级迭代加载/传播；自父/环/缺失parent拒绝；不得栈溢出 |
| N03 | Scene CPU | 带旋转/平移/非均匀缩放父级，比较独立矩阵乘法oracle；KeepLocal/KeepWorld分别核对local/world；keep-children移除保留world |
| N04 | Scene CPU | parent禁用影响混合后代；model.visible=false不影响camera/light child；删除selected节点清settings；非法重挂接/pose失败后所有相关数据和revision相同 |
| C01 | Scene CPU | identity相机forward=(0,0,-1)、up=(0,1,0)；非均匀/镜像父级pose正交；退化up/forward、NaN、Inf和无效lens拒绝；FocusDistance不改变projection |
| C02 | Renderer | 两个camera两个View identity，同一scene token；核对Eye、VP、可见物和相机方向，改变viewport/depth/FOV时同源更新 |
| C03 | Renderer/Viewer | 删除显式相机→有效default fallback；default也删除/disabled→NoActiveCamera+clear/UI，无旧pixels；恢复选择后可渲染；foreign handle直接报错 |
| C04 | Viewer controls | 外部camera edit/reparent后无输入tick不回写旧轨道值；orbit/dolly/pan/Fit经scene API更新；parent不可逆的world edit报错而非跳变 |
| L01 | Scene/Renderer | 默认方向光surface-to-light符号与已知受光平面相符；parent旋转改变方向，纯平移不改变光照；Color*Intensity及overflow/negative拒绝 |
| L02 | Scene/Renderer | 两个候选主灯，切换选择；未选灯编辑不改图；selected禁用/删除/零radiance为零direct且无CSM；ambient/emissive/unlit仍独立 |
| L03 | GPU | CastShadows true→false保持direct lighting，只移除shadow；pipeline开关与node属性独立；关闭后preview/neutral bindings不采旧map |
| P01 | Scene/archive CPU | native v1/v2/v3→v4：旧nested instance/material migration保留；旧eye/target/near/far→camera/focus/FOV；默认灯数值和ID冲突后缀可重复 |
| P02 | AssetImport/native | plain source v1、source v2、reflected source和native v4分别加载；children-before-parent成功；双payload、dangling selection、wrong kind、cycle、重复ID、TRS+matrix冲突拒绝 |
| P03 | SceneInstance | 插件无关：父子树+2 cameras+2方向光+environment，移动/重挂接/修改lens/light/选择→Snapshot→SaveAs→Load，比较stable IDs/local/payload/selection，runtime Handle不跨实例复用 |
| P04 | SceneInstance/assets | model/material/texture pinned reference跨目录rebasing；pending/failed material拒绝保存；source-less model拒绝；camera-only/group-only/empty场景可以保存且不补灯 |
| P05 | AssetImport/tools | model `--scene`包装产生1 model+真实default camera/lights；inspect instances=仍为model数；optional model payload内部references列为dependencies；旧conversion缓存不跳过v4升级 |
| A01 | Async SceneInstance | delayed model完成前移动parent、相机/灯light编辑、删除子树和slot复用；释放gate后不复活/覆盖编辑；一个model失败其他节点继续可用 |
| A02 | Async save | capture snapshot A→后续改camera/light为B→save完成→载入为A、live为B；注入IO保存失败，UI报错且live不回退 |
| F01 | Frame pipeline | 手动gate Render，按P1/F1/P2/F2顺序admit；P1与P2用可区分的geometry位置、camera和light；释放后核对两帧各自values/pixels/diagnostic tokens |
| F02 | Frame pipeline | freeze P1 token→应用P2→尝试build P1；必须ScenePublicationMismatch，不能悄悄画P2；同Scene重attach用旧epoch必须拒绝 |
| F03 | Frame/RHI lifetime | Render已build F1但RHI gate未开时admit新scene/删除节点；F1原constants/图像/资源仍有效；限制内多帧lead的0/1/2组合无race或提前释放 |
| F04 | Publication failure | Main验证/dispatch失败不ack且可重试；Render publication异常可观察且下一scene frame不混用；相机-only发布错误也有receipt；Close不依赖失败body继续执行 |
| F05 | Minimized/lifecycle | 初始空scene和manifest未ready的Tick也有首次有效token/clear输出；无presentation tick持续更新/删除camera/light/model并Flush；clear metadata/detach仍完成；reload无旧light/camera；已prepared旧Graph按leases存活 |
| M01 | Material guards | bound SetSceneParameters冲突batch不生效；Global/Frame/View受保护语义注入拒绝；5个protected语义的custom provider逐个负例；unbound custom及mixed-scope原tests仍过 |
| M02 | Effective revisions | 相机-only、light-radiance-only、rename/FocusDistance、未选节点编辑分开测：验证相关值更新，unchanged Object/Material/资源和geometry revision不误失效 |
| B01 | Geometry BVH | mixed subtree移动、reparent、删node、资源从unknown→ready；None/Linear/BVH图像及model-visible集合对照；所有camera/light/group不增geometry group或unbounded count |
| B02 | Geometry BVH | warmed camera-only/light-only连续运动，geometry IndexRebuilds/IndexRefits保持0；移动一个有模型后代的parent只更新其组；静态scene flush无全model preparation |
| R01 | GPU pipeline parity | 新场景入口Forward/Deferred各跑标准Z和ReversedZ，CSM on/off及ordinary/instance路径；继承camera+light、masked/transparent/mirrored模型，检查数据一致和零validation错误 |
| R02 | GPU CSM | parented offscreen caster、主camera lens变更、光源极角、主光关闭/删除后空shadow；验证caster独立visibility和实际像素变化 |
| U01 | GUI/input | 树显示全部kind；新增/选择/复制单节点/两种删除/reparent/设置default/main；表单值改真实scene，错误可见；model count和node count不混用 |
| U02 | Viewer/save | SceneViewer和ModelViewer原input/fit/capture流程；CLI light override和benchmark edit写scene；SceneViewer直接Snapshot save/reload与基线画面对照 |

GPU测试沿用真实D3D12和现有fixture infrastructure，可扩展现有tests或新增聚焦target。纯Scene tests不得链接Renderer/RHI。新增gated tests使用task/event gate与超时，不依赖不可复现的sleep碰运气；阻塞旧frame时预先保证后续工作不违反同一dedicated executor的等待契约。

图像比较：BVH/Linear和ordinary/instanced在同一candidate有效状态下继续要求既有一致性。旧/新schema相机pose重构允许必要的浮点误差，但必须在运行前固定容差并报告raw max/mean；推荐起点为8-bit通道max 2、全图mean 0.1，不能自动扩大。超过时先查FOV、light sign、roll、normal、depth与shadow差异，仍无法解释则交回reviewer。关键受光/遮挡像素需另有定向断言，不能仅用全图均值掩盖局部错误。

## V2. 基础构建与回归命令

命令在仓库根执行；这是executor阶段的命令清单。使用docs/VisualStudio.md和docs/CodingStyle.md的工具选择，不自行替换依赖版本。

```powershell
python tools/CheckStyle.py
python tools/CheckBoundaries.py
./tools/Build.ps1 -Preset debug -Test
python tools/CheckStyle.py --naming --build-dir out/build/debug
./tools/Build.ps1 -Preset release -Test
openspec validate scene-owned-cameras-and-lights --strict
openspec validate --all --strict
git -c safe.directory=F:/HyperionEngine diff --check
```

若使用VS方案，等效完整验证是`./tools/GenerateSolution.ps1 -Test -Configuration Debug`和Release；语义命名仍需要有效Ninja compile_commands。构建工具若不在PATH，使用Build.ps1已解析的VS bundled CTest路径，不把“找不到ctest”写成测试通过。

迭代期间可使用已构建目录的CTest `-R`聚焦检查。至少覆盖既有名称：`scene_management`、`scene_runtime_instance`、`scene_rendering`、`scene_spatial_visibility`、`scene_viewer_controls`、`scene_viewer_acceptance`、`scene_moving_camera`、`model_rendering`、`model_viewer_acceptance`、`native_asset_management`、相关publication tests、`material_contracts`、`material_bindings`、`material_rendering`、`instance_batching`、`cpu_frame_ownership`、`cpu_frame_pipeline`、`cpu_frame_viewer`、`deferred_rendering`、`depth_conventions`、`depth_viewer_acceptance`、`cascaded_shadow_maps`、`cascaded_shadow_rendering`、`lifecycle_recovery`、`render_resources`及新增tests。最终以CTest实际清单为准，不承诺固定测试总数。

场景记录变化会影响Assets、AssetImport、AssetTool、测试fixture和Python脚本，不可只编译Viewer。完成相关target后最终Debug/Release完整套件各跑一次；通过后无新改动无需机械重复。实际受影响功能所需的capture测试按构建选项运行；无法运行的GPU/desktop/RenderDoc检查列为未执行并给出真实原因，不伪装为pass。

## V3. 性能与最小更新量

这是行为/组织改造，不承诺加速；要求不以无关重建破坏既有最小更新量。使用等价内容的baseline/candidate、相同GPU与配置，序列交错A/B，不同时跑多个GPU benchmark。

固定矩阵：Debug/Release × Forward/Deferred × 以下四种motion；CSM默认开启、同分辨率/距离、相同batching、VSync关闭、GUI关闭、无额外profiling开销。每格至少3组交错A/B，warmup 240帧、samples 600帧；准备不足则统一提高双方warmup并记录，禁止采未ready/无camera/空draw场景。

| motion | 参数/目的 |
| --- | --- |
| static | 不加motion参数；确认空flush及跨帧复用 |
| small-camera | `--benchmark-camera --benchmark-camera-step 1`；固定小幅操作 |
| large-camera | `--benchmark-camera --benchmark-camera-step 12`；变化可见集/CSM压力 |
| moving-light | `--benchmark-light`；几何/相机不变，仅更新scene light |

参数示例（candidate Debug/Forward；使用前确认两个程序各自的root/content并使用匹配版本）：

```powershell
./out/build/debug/bin/hyperion_viewer.exe --config experiments/Scene.json --scene out/content/Scenes/Showcase.hasset --pipeline forward --frames 840 --benchmark-warmup 240 --benchmark out/SceneOwnedCamerasAndLights/Run01/ForwardStatic.csv --shadow-resolution 1024 --no-vsync --hidden --no-ui
```

示例输出目录由executor提前创建。其余格只改变指定preset/pipeline/motion及输出名，使用同一shipped fixture和同一有效相机/灯光。Showcase基线/新格式分别导入到对应checkout；不能让旧程序加载v4。Sponza另外保存迁移前后静态和移动构图证据，不把不同场景的模型数拼到同一个性能表。

报告每格frame/pipeline prepare/material/batch plan/RHI prepare/shadow setup及GPU mean/P95；记录visible items、实际draw、shadow items/draw、constant/instance upload、geometry rebuild/refit、cache reuse、descriptor/PSO/resource counts。GPU samples按完成submission ID一一关联，不能重复累加同一个GPU结果。已有CSV列保持；缺少的关键CPU诊断通过最小engine-owned计数/perf scope补齐，默认off路径不增clock/alloc/global RMW。

必须满足：camera/light-only的几何BVH rebuild/refit为0，未变模型不重复桥接发布，资源/缓存受容量约束，实例覆盖与选定draw路径一致，零unexpected validation errors。新增logical node counts不能降低可见model工作量伪装收益。

性能退化审查触发线：同一格三轮median frame mean或pipeline prepare mean比baseline增加超过5%，或P95超过10%，必须定位并附证据；这些是review触发线，不是允许executor删除慢数据/擅自放宽的硬件通用保证。小样本噪声、热状态及额外诊断开销单独说明。重大未解释退化交回planner，不靠关CSM、减少cascade更新或关闭Debug validation“修复”。

再做至少3000帧持续camera/light更新（Debug+CSM，资源ready后开始统计），验证camera/light metadata和旧frame history无无界增长，descriptor/PSO稳定、resource/constant pages按已有上限回收。该case无需每帧同步readback。

## V4. executor交付内容

实现阶段新增`implementation.md`记录实际决策映射与改动，不在本轮预填“完成”。executor完成后新增`review-handoff.md`，包含：

1. baseline/实际HEAD、分支与精确文件列表（含tracked/untracked），改动中有无用户原有文件；不自动stage/commit。
2. D1-D12实现位置和命名映射；每个V1 ID对应测试函数/fixture/命令、通过/失败/未运行、日志路径。
3. schema v4/source v2样例、旧migration fixtures及importer/cache失效证据；SaveAs references的实际检查。
4. 同帧token/epoch和old-frame lifetime的gated测试结果，metadata-only publication与failure cleanup证据。
5. Debug/Release实际套件统计、style/naming/boundary/OpenSpec/whitespace结果；命令退出码和原日志保留，不只摘一行pass。
6. 图像对照和raw performance表，包含全部重复样本、负面结果和限制。
7. 未解决问题、与设计不符之处、尚未执行项；不可用“整体完成”遮盖。
8. 明确请求planner/reviewer审查，并停止执行；tasks中reviewer-only项目不得自行勾选。

reviewer应优先检查双重状态是否消除、发布版本是否绑定、默认灯是否复活、schema是否真的往返、层级是否实际传播、缓存是否被全量失效，以及测试是否证明真实RHI结果。planner/reviewer负责最终review结论；executor收到findings后再执行修复并回交。
