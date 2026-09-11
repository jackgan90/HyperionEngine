# 原生资产管线质量审计

用户要求审核并修复 `complete-native-asset-pipeline`。本轮采用不继承实现会话上下文的独立 reviewer，主 agent 逐项复核、局部修复，再由原 reviewer 对修复及其直接影响做两轮针对性复审。最终 6 项确认缺陷（5 项 P2、1 项 P3）全部关闭，限定审查范围内没有剩余的已确认问题。未执行 Git stage、commit 或 OpenSpec 归档。

## 审查版本和范围

- 基线 commit：`4a86215ff57fb62f0df2d9c379c291668a3182e4`。
- 初审：`out/NativeAssetAudit/ReviewSnapshot.json`，126 项未提交变更；物理 SHA256 `2e198ad94b98f73750fe88132933b146e3e7267dc991ffd2cdbabe852e9a4074`。
- 首轮修复：`out/NativeAssetAudit/FixSnapshot.json`，127 项；SHA256 `5570d101f1298dcce0be61ec8788a664669635637cd2fe0e45db8e580fa13401`。其中 audit_fix_files 列出相对初审变化的 18 个文件。
- 最终源码：`out/NativeAssetAudit/FinalSourceSnapshot.json`，127 项；SHA256 `ff4976e9d259c0861d087e72b7dd4051bf3ecb3f4a0a04d9ac501f7555560aa2`。相对上一版本只修改 NativeAsset.cpp 与 NativeAssetTests.cpp。此报告和 verification.md 的最终结果说明在源码复审结束后整理。

每轮独立审查开始和结束均校验逐文件 SHA256 和删除状态，受审文件保持不变。初审覆盖 Reflection/Serialization/AssetTypes、native Assets 管理、独立 AssetImport 及事务发布、SceneManifest 迁移、SceneInstance 快照、SceneViewer 保存、AssetTool、构建与测试接入、8 个 delta spec 及相关文档。用户没有额外指定单一审查重点。

## Findings 的主复核和处置

所有条目均由主 agent 阅读调用路径并通过针对性复现、测试或准确的边界分析确认，属于本次管线改动及直接修复范围。没有因个人风格偏好扩大修复，也没有需要扩大范围后再批准的项目。

| ID / 优先级 | 确认问题 | 最终修复与回归 |
| --- | --- | --- |
| NA-01 / P2 | 原生输入图升级时，原引用的固定 ID/Revision 在改写前没有验证，可能静默采用已替换的子资产。 | AssetPublicationGraph.cpp 在首次转换和 Published 去重命中两条路径验证来源头的身份、类型及固定 Revision，错误包含父来源和字段路径；允许合法迁移/重定位产生新的输出 Revision。CheckPinnedNativeImport 覆盖 ID/Revision 错误、首次/复用分支、旧 root 保持不变与合法重定位。 |
| NA-02 / P2 | 跨盘来源的 lexically_relative 返回空路径，不同同类型来源共享身份键，二次导入合并 ID，并持续失去增量命中。 | ImportRelativePath 在不能形成相对路径时保留绝对路径，统一用于来源指纹与输出身份键。CheckCrossVolumeImport 使用 Windows 跨盘路径和内存文件系统，验证两来源的 ID 在 unchanged、force 和来源修改后均稳定且互不相同。 |
| NA-03 / P2 | 显式 Drain 先关闭公共加载入口，已经接收的图任务随后无法派生依赖加载，合法图被返回为部分失败。 | 将外部请求与已接收图的私有派生加载区分；外部入口关闭后，已接收工作继续遵守缓存/取消/保存顺序和限额。Drain 循环等待 Pending，覆盖后续派生及清理。CheckGraphDrain 用 gate 和单 Worker 确认两资产完整完成、外部拒绝以及 InFlight/Entries 归零。 |
| NA-04 / P2 | Registry 把 std::function 的 target_type 当作绑定一致性，接受同种 lambda 捕获不同成员指针或迁移数据，造成读写合同不一致。 | TRecordCallback 为绑定保存不可变身份，复制保持身份、替换创建新身份；Registry 比较成员全部回调、Create/Validate/Commit 以及迁移映射。CheckBindingIdentity 验证成员整体和单回调替换、同 lambda 不同捕获、迁移键变化拒绝，原样 descriptor 副本仍可重复注册。 |
| NA-05 / P3 | EncodeAsset 对 payload 应用完整文件大小限额，再追加 80 字节 HAST 头，可能成功写出默认读取拒绝的文件。 | 编码从 MaxBytes 扣除固定头；小于头部的预算先拒绝。CheckNativeSizeBudget 使用小限额测试 0、79、完整文件少一字节失败，恰好完整文件大小成功往返。没有为边界验证分配 512 MiB。 |
| RR-01 / P2 | 复审发现 DecodeAsset 的内容哈希校验及 legacy Revision 计算丢失调用者 FArchiveLimits。24 层、2400 字节资产在 MaxDepth=256 下写入成功，读取却退回默认深度并失败。 | ValidateDocument 与 ReadLegacy 均将 InLimits 传给 HashArchive。CheckCustomArchiveLimits 验证 current HAST 与 bare HYPA 的结构/Revision 往返、自定义限额成功以及默认限额仍拒绝。新增用例在修复生产代码前实际失败，修复后两种构建均通过。 |

首轮两处测试 atomic_bool 声明改为仓库惯用且等价的 atomic<bool>，以通过现有语义命名检查；没有改变等待或通知逻辑。回调身份修复保持现有反射持久化协议，未重构模块职责。

## 独立复审证据

原始报告及原始日志均在 `out/NativeAssetAudit`：

- `IndependentReview.md`：初审 5 项缺陷；ReviewProbe 实际复现固定 Revision 丢弃、跨盘身份合并和冲突成员注册；DrainProbe 实际复现关闭期间依赖失败；大小问题有完整公式及边界分析。
- `ReReview.md`：NA-01 至 NA-05 关闭，新增 RR-01。独立重编译原 probes 后，坏引用和冲突注册被拒绝，跨盘第二次导入命中增量且 ID 不冲突，Drain 返回 2 个资产、0 个失败。独立重跑 native、archive、publication 测试通过。
- `FinalReReview.md`：RR-01 关闭，原 5 项关闭结论保持。FinalReviewProbe 重新链接最终 Debug 库，2400 字节案例编码和解码均成功；独立重跑最终 native_asset_tests 通过。未把旧静态链接二进制当作修复后证据。

## 实际验证

| 验证 | 结果与原始证据 |
| --- | --- |
| 首轮 5 项修复后的完整 Debug 构建 / CTest | 59/59，196.32 秒；DebugFull.log。 |
| 首轮 5 项修复后的完整 Release 构建 / CTest | 59/59，116.71 秒；ReleaseFull.log。 |
| 最后 RR-01 修改后的 Debug / Release 所有目标重建 | 均通过；FinalDebugBuild.log、FinalReleaseBuild.log。 |
| 最终 Debug 受影响回归 | 两次运行均 5/5，合计 9 个不同用例（model_fixtures 重复一次）；FinalTargetedDebug.log、FinalSupplementDebug.log。覆盖反射、模型数据、场景管理/运行时/Viewer 保存、原生加载、glTF 导入、事务发布。 |
| 最终 Release 受影响回归 | 同一组 9/9，7.74 秒；FinalTargetedRelease.log。 |
| 格式、路径大小写及语义命名 | 398 个源码文件格式/路径通过，244 个 C++ 翻译单元语义命名通过；FinalNaming.log。 |
| 模块边界 | 379 个源码文件、27 个模块通过；FinalBoundaries.log。 |
| OpenSpec strict | 45/45；FinalOpenSpec.log。 |
| Git diff whitespace | 通过；暂存区保持为空。 |

最后两处限额传递修改后重建全部目标并运行相关回归，没有再重复全量 59 项；全量结果的对应版本如上明确区分。此前 implementation verification 中的性能数据属于原始已记录哈希的二进制，本轮没有重新测量，也没有将它们表述为审计后性能。

## 验证边界

独立 reviewer 的动态结果限于其自行重编译的 probes 和实际运行的针对性测试；全量 Debug/Release 与命名检查由主 agent 执行。本轮未做穷尽 fuzzing、OOM 注入、每个发布边界的进程终止测试或任意用户 callback 副作用验证。原设计中的缓存估重、旧 immutable generation 保留、路径词法规范化等边界继续适用，见 verification.md 和 docs/NativeAssets.md。
