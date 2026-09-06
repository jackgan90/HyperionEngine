# Model Viewer 独立审计与修复

日期：2026-09-06。范围为本轮静态 glTF/GLB Model Viewer，包括其 IO、资产服务、导入适配器、Scene 数据、渲染及 D3D12 资源生命周期。

独立 reviewer 以 `fork_turns: none` 启动，只收到仓库位置、审计范围与检查要求，没有继承主会话上下文。reviewer 只读代码，不修改文件或运行构建。主 agent 对每项发现独立核实并实施修复；修复后 reviewer 再次静态审阅，没有发现此次局部修改引入的新问题。

## 核实结果

| 优先级 | 发现与独立核实 | 最小修复及证据 |
| --- | --- | --- |
| P1 | sparse accessor 与 interleaved 基础数据组合时，cgltf v1.15 的浮点展开按基础 stride 前进 sparse values；稀疏值实际紧密排列，会读错数据，紧邻分配边界时可能越界。新增确定性 fixture 在旧代码上读出错误位置。 | 基础 accessor 与 sparse overlay 分开读取；overlay 显式使用元素大小作为 stride，保留 normalized 转换。`SparseInterleaved.gltf` 精确比较全部位置。 |
| P1 | 原节点深度 DFS 的已访问标记会让倒序存储的父子链绕过深度上限；后续递归实例遍历仍会走完整长链。300 节点倒序 fixture 在旧代码上未被拒绝。cgltf 自身逐节点向上检查长链还有平方级开销。 | 先验证引用与单父约束，再从全部无父节点迭代检查真实深度和环。在调用 cgltf 验证前完成此检查，维持原有最多 256 层约束；原生模型共用检查。`ModelTests.cpp` 和 `DeepNodes.gltf` 均覆盖倒序链。 |
| P2 | 透明 primitive 按中心到相机的欧氏距离排序，即使是不相交平行面，也可能因横向偏移把近面排到远面之前。原 GPU 测试加入偏轴红/蓝透明面后像素失败。 | 按 primitive 中心的投影深度从后向前排序；排序键每个实例只计算一次。黑底两层 0.5 alpha 的中心 sRGB 预期约为 `(0.7354, 0, 0.5371)`，用实际 GPU 读回验证。 |
| P2 | `Draws()` 原来每个 draw、每帧创建独立常量 GPU buffer；源代码调用链确认每次都会走 D3D12MA 资源创建和 Map/Unmap，并占用 RHI 协调线程。 | 每个模型每帧合并成一个不可变常量 buffer，各 draw 使用 512 字节片与偏移；保留已有帧 fence 资源持有机制。后端在录制前检查偏移对齐和剩余长度。GPU 像素测试验证不同 draw 读取自己的材质/变换，并测试未对齐、超范围及 `UINT64_MAX` 偏移被拒绝。 |
| P2 | `ModelBounds()` 对每个实例重新遍历共享网格所有顶点；大量实例共享大网格时，变换成本是实例数乘顶点数。 | 每个被引用 primitive 只计算一次局部 AABB，每个实例变换其 8 个角。结果是用于相机取景的保守世界 AABB；测试验证旋转、非均匀/负缩放后所有顶点仍被包含，并保留原平移/缩放断言。 |
| P2 | 已同时提供 normals 和 tangents 时，方向生成仍分配三个逐顶点临时向量并遍历三角形，而最终不会写回任何生成值。 | 两个属性都有数据时直接返回，省去约 `36 × 顶点数` 字节临时存储和无效遍历。缺失任一属性时沿用既有生成逻辑。 |
| P1 | 主 agent 核实 sparse 问题时另发现：cgltf 验证器在检查 bufferView 物理范围前就读取 sparse indices，不能把它视为所有内存读取的安全前置条件。 | 导入器先校验所有 view 相对 buffer 的物理范围，再校验 accessor 与 sparse 范围、数量及索引顺序；使用减法/除法避免长度运算溢出。畸形 view、巨大范围和超量 sparse count 加入拒绝回归。此项依据源码读取顺序确认，不把原进程超时描述为已经捕获的崩溃。 |

所有 6 项 reviewer 发现均经核实成立，另修复 1 项主 agent 发现的问题。修复没有增加 IO 线程、跨帧常量复用、资源池、额外锁或新的状态机。

## 性能与范围说明

常量 GPU 资源创建次数由每模型每帧的 `N` 次降为非空时 1 次；常量数据量仍为 `512 × N` 字节，没有声称减少 draw call 或 GPU 着色成本。包围盒计算从逐实例扫描网格改为局部顶点扫描加每实例 8 次角点变换；模型的属性验证仍然需要线性扫描。没有以未经测量的 FPS 数字表述收益。

保守 AABB 对旋转后的非盒状网格可能比逐顶点紧包围盒稍大，初始取景可能略松。透明排序仍以 primitive 中心为粒度，不能正确解决相交透明面或穿越相机的大 primitive；这属于已公开的当前渲染能力边界，没有在本轮引入 OIT 或三角形级重排。

reviewer 和主 agent 检查了单 Worker 下的 IO 等待、请求取消/Drain、插件停止、上传暂存资源以及帧内 GPU 资源保留链，未确认其他需要本轮修复的竞争或泄漏。自动化测试覆盖受控延迟 IO、取消、错误恢复及退出收尾；这些结论不能保证所有可能线程交错或所有 glTF 资产都已覆盖。

没有扩展动画、蒙皮、压缩扩展、其他模型格式、热重载、通用内存硬配额、完整任务系统或上传架构。更完整的能力边界见 [AssetPipeline.md](AssetPipeline.md)。

## 验证证据

修复前快照保存于本地 `out/ModelReviewBaseline`。先向旧实现加入针对性回归，再运行测试：`out/ModelReviewBeforeTests.log` 记录 Scene 深度、组合 accessor 和偏轴透明像素三项失败，fixture 生成通过。修复后的最终全量日志以以下表格为准；`out/ModelReviewFixTests.log` 是修复过程中的中间失败日志，不代表最终结果。

| 检查 | 最终结果 | 本地日志 |
| --- | --- | --- |
| VS 2022 Debug 构建与 CTest | 23/23，通过；测试耗时 35.62 秒 | `out/ModelReviewDebug.log` |
| VS 2022 Release 构建与 CTest | 23/23，通过；测试耗时 28.13 秒 | `out/ModelReviewRelease.log` |
| Ninja Debug 构建 | 通过 | `out/ModelReviewNinja.log` |
| 格式、路径及语义命名 | 通过，46 个翻译单元 | `out/ModelReviewStyle.log` |
| 模块边界 | 通过，82 个实现文件、21 个模块 | CTest `dependency_boundaries` |
| OpenSpec strict | 19/19，通过 | `out/ModelReviewOpenSpec.log` |

硬件路径为 Windows x64、RTX 5080、D3D12 debug layer。`model_rendering` 在测试结束断言设备 `ValidationErrors == 0`，并包含实际颜色读回、受控 IO、相机和错误恢复。CTest 同时运行真实 Viewer 的 glTF/GLB 截图验收以及原有三角形、GUI、配置与后端测试。

对应源码：

- [GltfImport.cpp](../Source/Runtime/AssetImport/Private/Adapters/GltfImport.cpp)
- [Model.cpp](../Source/Runtime/Scene/Private/Model.cpp)
- [ModelRenderer.cpp](../Source/Runtime/Renderer/Private/ModelRenderer.cpp)
- [D3D12RHISwapchain.cpp](../Source/Backends/D3D12/Private/D3D12RHISwapchain.cpp)
- [GenerateModelFixtures.py](../tools/GenerateModelFixtures.py)、[ImportTests.cpp](../Source/Tests/Assets/ImportTests.cpp)、[ModelTests.cpp](../Source/Tests/Scene/ModelTests.cpp)、[ModelRenderTests.cpp](../Source/Tests/Renderer/ModelRenderTests.cpp)
