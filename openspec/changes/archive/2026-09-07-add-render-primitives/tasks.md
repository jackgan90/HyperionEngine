全部 37 项任务及对应验证已完成，独立质量审计的两项 P2 已修复并复审关闭。用户已授权整理交付记录、同步正式规格、归档并提交 Git。

## 1. 类型、模块与执行域契约

- [x] 1.1 在现有 Renderer target 内定义 `IRenderPrimitive`、`FRenderPrimitiveHandle`、`FRenderItem`、`FRenderView` 和自有 frame snapshot，明确每个接口的执行域、所有权和借用边界；允许零到多个 item。
- [x] 1.2 定义 Main 侧 `FRenderSceneClient`/`FRenderBinding` 与 Render 独占的 `FRenderScene`，用资源身份/租约隔离原生句柄；为不同 primitive 实现提供仅在 Render 调用的创建入口。
- [x] 1.3 接入执行域检查；若 Tasks 缺少查询能力，仅补最小只读接口，不改动其失败依赖及关闭语义。新增泛型 primitive 测试目标并保持 Scene/Animation 无 Renderer/RHI 依赖。

## 2. 命令、快照与 proxy 生命周期

- [x] 2.1 实现带场景身份/generation 的句柄分配、统一命令 mailbox、逻辑序号和 Create/Update/Remove 接收协议；Main 不传递完整 proxy 或借用的对象指针。
- [x] 2.2 实现单调 revision、资源请求版本检查、跨 section 原子更新 batch 和 BeginRenderFrame 消费边界，保证无半批状态和旧结果覆盖。
- [x] 2.3 在 Render 创建、独占注册、更新和析构 proxy；实现 PendingCreate/PendingResources/Ready/Retiring/Destroyed 状态与创建/注册失败清理。
- [x] 2.4 实现 move-only Main 绑定的移除、幂等重复移除、Render 移除回执，以及过期/跨场景句柄的处理；通过独立控制 pump 在最小化/无新帧时继续推进。
- [x] 2.5 实现异步结果与 Main 状态发布，覆盖 superseded/cancelled 结果；清理独立于可能失败的前置任务，避免等待 Main 回调。
- [x] 2.6 验证命令参数自有性、wrong-domain 拒绝、批量帧一致性、句柄复用、不同类型收集及已构造 proxy 恰好一次 Render 析构。

## 3. 共享资源与就绪发布

- [x] 3.1 从现有 PrepareModel/FModelRenderer 提取不可变几何、材质和纹理准备数据，定义含资产身份/版本、section、布局/配置、设备与色彩空间角色的资源键。
- [x] 3.2 实现设备级资源服务和并发请求合并，复用兼容几何/材质，隔离实例覆盖；取消一个使用者不取消其他使用者，失败后新请求可重试。
- [x] 3.3 实现 Worker 准备、RHI 0 资源创建/上传、Render 消费就绪结果的交接；初次模型资源按整组就绪发布，替换按完整资源版本切换。
- [x] 3.4 验证相同资产/配置复用、不同设备/布局/纹理角色不误复用、单实例变换/可见性/材质修改隔离，以及 pending/failed/cancelled 请求的独立终态。

## 4. RHI 资源退休与关闭协议

- [x] 4.1 实现 coordinator 权威资源保活与线程安全退休请求，明确 proxy 租约、frame/upload 工作与实际 RHI 句柄的引用关系，保证底层最后释放在 RHI 0。
- [x] 4.2 连接现有上传批次和 frame/recorded-list fence 保活，补齐未绘制资源、上传中移除、最后实例移除和帧失败的正常运行期采集；无新帧时也驱动完成/退休，不以全局 WaitIdle 或固定延迟帧数代替退休条件。
- [x] 4.3 实现 session 关闭 admission、排空生产者和异步结果、Render 注销/析构、RHI join/cancel/drain/退休及 executor 关闭顺序；关闭后的绑定析构不派发任务。
- [x] 4.4 通过受控队列/替身资源验证失败依赖仍清理、未提交资源的 RHI 0 析构、迟到上传不复活 proxy，以及无出帧/最小化和 shutdown 中每个已接收操作的终态；拒绝同队列及循环等待。
- [x] 4.5 用真实 D3D12 fence gate 验证 proxy 可先析构而 GPU 资源继续保活，最后安全退休；重跑既有 Present 失败、重复取消、下一帧恢复及独立 device/payload ownership 回归。

## 5. 静态 primitive 与统一场景绘制

- [x] 5.1 实现 `FStaticMeshRenderPrimitive`，支持已有模型和程序化三角形所需的几何布局/材质描述，保存独立实例状态与保守 bounds，收集过程不修改逻辑状态。
- [x] 5.2 实现隐藏/未就绪过滤与保守 CPU 视锥过滤；保持无有效剔除依据时保留对象，并验证边界相交和镜像/非均匀变换。
- [x] 5.3 实现跨模型 item 收集、opaque/mask/blend 分类、全场景稳定中心深度排序及 pass 级深度初始化；不为每个模型或 primitive 建立 graph pass。
- [x] 5.4 实现按帧/资源批次交给 RHI 0 的常量上传和 packet 物化，保活 frame 数据后交给既有 indexed RHI 录制/提交；保留图验证和错误取消。
- [x] 5.5 验证超过 context 数量的多个 primitive 仍使用受支持的聚合 pass，以及同一冻结状态多次/多视图收集不会推进逻辑状态。首版保持普通 draw，不加入 instancing shader/RHI 扩展。

## 6. Main 模型、插件与应用迁移

- [x] 6.1 在 Renderer 的独立桥接接口中实现 Main `FModel`，组合不可变资产和 bindings，按节点实例乘 section 注册、整组发布变换/可见性/材质覆盖及注销。
- [x] 6.2 迁移 ModelViewer 的加载、相机输入、状态展示与模型绑定；分开 Main 状态、Render 状态、RHI 工作，不再经共享可变 PImpl 跨域取值。
- [x] 6.3 迁移 Triangle，使用通用 geometry/material 描述及 primitive 注册，保留其缩放、shader 和既有可见行为，不通过 glTF/model 专用路径。
- [x] 6.4 在 Viewer 组装通用 render session、owned view/settings snapshots 和明确的插件执行域；保留 GUI overlay、RenderDoc 生命周期、配置 ID 与正常/异常关闭。
- [x] 6.5 迁移全部调用方和测试，移除旧 FModelRenderer 整模型资源/提交路径及临时桥接，保持现有 target、可执行文件、CLI 和序列化字段不变。

## 7. 多模型和端到端回归

- [x] 7.1 在不链接 ModelViewer/Triangle 的 Runtime 测试中覆盖代理扩展、消息顺序、批量快照、过期句柄、移除回执及 session 关闭；故意暂停 Render 后修改/销毁 Main 输入，验证数据隔离。
- [x] 7.2 增加两个共享资产模型的真实 GPU 验收，验证共享几何资源身份/上传次数、独立变换、可见性、材质覆盖和单实例移除后的剩余实例。
- [x] 7.3 增加跨模型遮挡与交错透明 section 的像素验收，确保每模型清深度和局部排序会被测试识别；保留中心透明排序的适用范围。
- [x] 7.4 运行既有 glTF/GLB、深度、alpha/mask、sRGB/UV/mip、镜像/双面、相机、延迟 IO 和加载失败回归，保持 CPU 资产序列化/导入独立有效。
- [x] 7.5 验证 triangle、GUI、clear-only、resize/minimize/restore、截图、默认离线启动以及既有 RenderDoc 可用/不可用路径，没有新的循环等待或关闭后派发。

## 8. 文档与最终验证

- [x] 8.1 新增 `docs/RenderPrimitives.md`，同步 Architecture/SourceLayout/AssetPipeline 的类型、线程、帧、资源退休、插件与扩展契约，移除与新标准冲突的整模型 RHI 绘制指引。
- [x] 8.2 执行 `python tools/CheckStyle.py`、`python tools/CheckBoundaries.py`，生成/刷新 Ninja 编译数据库后执行 `python tools/CheckStyle.py --naming --build-dir out/build/debug`，完成相应 Ninja 构建。
- [x] 8.3 按 `docs/VisualStudio.md` 执行 VS Debug/Release 构建与完整 CTest，完成当前可用 GPU/desktop/RenderDoc 验收并记录实际条件、失败或跳过原因，不沿用历史通过数量。
- [x] 8.4 执行 `openspec validate add-render-primitives --strict`、`openspec validate --all --strict` 和 `git diff --check`，核对 delta 与实现/文档一致，记录验证证据；归档或提交按后续明确指示执行。

需求覆盖：`render-primitives` 对应 1/2/4/5/7；`shared-render-resources` 对应 3/4/5/7；`render-graph-plugins` 对应 5/6/7；`static-model-rendering` 对应 5/6/7；`rhi-backend-abstraction` 对应 4/7；`runtime-module-organization` 对应 1/6/8。
