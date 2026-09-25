# Renderer CPU 提交优化

CPU 提交路径围绕材质依赖、批次准备、D3D12 录制和资源所有权组织。增量数据流见 [增量更新](IncrementalRenderUpdates.md)。

## 实现

- 材质求值按实际依赖预备参数索引。仅依赖 Global/Frame/Scene/View/Pass 的兼容语义使用同一视图内共享的不可变值覆盖层；Object/Material/Draw、混合依赖、覆盖和默认值仍按各自契约处理。覆盖层是平坦存储，不串联历史帧。
- 内置 instance 策略在局部值、资源、几何、状态及组内共享值仍兼容时复用成员计划；instance 参数变化仍重建载荷，自定义策略仍完整重新求值。全局缓存过期扫描移到 family 边界，逐视图只遍历自己的 chunk 范围。
- D3D12 按完成 fence 的 frame slot/context 复用原生命令列表。逻辑录制仍独立；外部保留旧录制时创建另一个原生列表。每次录制的 PSO、topology、VB/IB view、stencil、blend 和 scissor 缓存从空状态开始。
- 保留设备/类型/范围/发布版本/上传完成/附件验证。常量槽重复检查改用经过 root signature 预算验证的位掩码；已发布常量区间使用有序查找；纹理上传完成用一次 fence 快照和 binding set 的最大上传 fence 验证。
- `CompileAndConsume()` 转移图的命令向量，`RecordOwned()` 让 fence 保留同一份不可变命令。原有 `Compile() const` 和借用式 `Record()` 继续复制输入。调用 `RecordOwned()` 后不得通过其他别名修改命令。
- 不透明和 overlay 项不再计算无用的透明深度排序键；关闭剔除时不计算局部剔除矩阵。静态 primitive 直接在输出容器构造条目。设备统计并入 EndFrame 的 RHI 任务，GUI 无绘制时仅在首次隐藏时清理旧数据。

未改变 Debug 编译优化、验证开关、CSM 数量/分辨率/更新频率、材质 ABI 或 shader 行为。Present 转换保留为独立 pass；资源转换和并行录制的既有同步顺序继续生效。


性能采集入口与当前 Editor 工作负载约束见 [渲染诊断](RenderDiagnostics.md)。
