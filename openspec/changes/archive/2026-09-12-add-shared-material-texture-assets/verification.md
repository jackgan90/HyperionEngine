# 验证记录

开发验证日期：2026-09-11；独立审计复验：2026-09-12。基线 HEAD：`b49347c2090cad0ae085c918a8b85a95cc5179de`。本轮变更保留在工作区，未暂存、未提交 Git。

## 开发完成时结果（2026-09-11）

| 检查 | 结果 | 本地证据 |
| --- | --- | --- |
| Debug 完整构建 | 通过 | out/SharedAssetChecks/BuildDebugFinal.log |
| Release 完整构建 | 通过 | out/SharedAssetChecks/BuildReleaseFinal.log |
| Debug CTest | 62/62，通过，215.03 秒 | out/SharedAssetChecks/TestsDebugFinal.log |
| Release CTest | 62/62，通过，104.33 秒 | out/SharedAssetChecks/TestsReleaseFinal.log |
| 格式/文件名/include | 430 个源码文件，通过 | out/SharedAssetChecks/NamingFinal.log |
| 语义命名与局部声明 | 265 个翻译单元，通过 | out/SharedAssetChecks/NamingFinal.log |
| 直接和传递模块边界 | 410 个 Source 文件、28 个模块，通过 | tools/CheckBoundaries.py |
| OpenSpec 严格校验 | 通过 | openspec validate add-shared-material-texture-assets --strict |
| git diff --check | 通过 | 最终工作区检查 |

最终 Debug 和 Release CTest 按顺序执行，没有并行构建。两套均包含 27 项 GPU/desktop 测试、RenderDoc capture/replay、静态/移动 SceneViewer、深度模式、帧生命周期和原有通用材质回归。首次与 Release 构建并行的 Debug 回归中，scene_viewer_acceptance 的静态缓存最后一帧计数断言失败；单独重跑通过，最终完整 Debug 重跑也通过，像素与可见性断言保留。

## 新增专项覆盖

### material_asset_contracts

- 独立材质/纹理反射与原生往返。
- 通用 shader 路径、入口、defines、多 pass、固定/动态图形状态、结构/数组参数。
- typed texture dependency 收集，非法引用、未知参数、不支持的 buffer/render target 拒绝。
- 线性 RGB mip 为 128，sRGB 在线性空间平均后为 188；alpha 为 128。
- TextureSource 的 mip 与加载的 FTextureAsset 为同一存储，运行时没有 mip 重建或像素复制。

### shared_asset_publication

- 一个 Worker 下并发接受两个不同源模型导入；共享一个 library，根与材质 ID 独立，外部图片和白纹理共用 ID/revision 与加载对象。
- 相同输入重导入不写文件；外部 PNG 改动保留纹理 ID 并产生新 revision，旧根仍可读取旧图。
- 跨服务 library lease 冲突报错。
- glTF 的 sRGB/linear 角色变体与独立 sampler 值。
- 真实内嵌 model schema 1 被运行时拒绝，经显式离线拆分升级为 schema 2，保留根身份与几何。
- 独立纹理 JSON 往返和发布；越界 bulk 整数、负 unsigned 值、重复 JSON key 拒绝。

已有 native_asset_publication 保留源指纹、固定源引用核对、失败保留旧根、跨盘依赖、发布排序和本地文件 lease 回归。async_gltf_import 仍验证原有 accessor、拓扑、PNG/JPEG、外部/嵌入图片、取消和单 Worker 行为；源测试使用显式 FModelSource，渲染测试经过 SplitModelSource。

### shared_asset_rendering

设备：NVIDIA GeForce RTX 5080。Debug 启用 D3D12 debug layer。夹具使用一个 Worker、一个 RHI 队列；资产文件系统拒绝所有非 .hasset 读取，DeniedReads=0。ShaderCompiler 仍允许读取 HLSL，这是本轮明确保留的路径。

原始输出摘录见 `out/SharedAssetChecks/SharedGpuEvidence.log`：

| 场景 | 断言/测量 |
| --- | --- |
| 两个不同模型引用一个材质/纹理 | GeometryUploads=2；MaterialPreparations=1；TextureSources=1；TextureUploads=1 |
| 单实例 Tint 数值编辑 | 几何/纹理/管线创建增量均为 0；左侧绿、右侧红 |
| Save As 后清除 CPU 缓存重载 | 像素逐元素完全相同；材质引用重定位为 ../Material.hasset |
| 分段材质与 sampler 变体 | 同一灰度纹理只新增一次上传；不同 sampler 独立创建 |
| 同字节不同颜色编码 | sRGB 版本读回约 0.502，linear 版本约 0.737；只为新纹理资产增加一次上传 |
| 纹理和 sampler 类型化覆盖保存 | Save As/reload 像素完全相同，Object 覆盖保留 |
| 材质依赖版本更新 | 同一模型几何对象同时显示旧红色和新蓝色；只增加一次几何上传 |
| 缺失/错误 revision | 明确失败；缺失文件名包含在诊断中 |
| CPU 纹理缓存 | 4097 个存活源，元数据不超过 4096；淘汰元数据不破坏外部持有的源 |

材质描述来自资产，shader 使用 `SharedAsset.hlsl` 的 `AssetVertex/AssetPixel` 与 `ASSET_GAIN=1`。该测试没有 Renderer 按材质名称分支，也没有用 PBR 固定参数重建自定义材质。原有 fenced retirement、frame ownership、材质 GPU cache 和生命周期测试均在两套完整回归中通过。

专项测试发现并修复了当前版本就绪判断：FModel::IsReady 会核对每个绑定的发布 revision，防止新纹理尚未完成准备时沿用旧材质的 Ready 状态。纹理缓存也在每次插入时执行上限检查，覆盖一次解析多个新源的情况。

## Viewer 与可编辑来源样例

`assets/Scenes/SharedAssets.json` 通过共同 library 生成 5 个原生资产：scene、两个不同几何模型、一个材质、一个纹理。材质/纹理/模型 JSON 可以独立编辑并经 AssetTool 发布；原生 export-json 会重定位引用。

实际命令：

~~~powershell
./out/build/debug/bin/hyperion_asset_tool.exe validate out/content/Scenes/SharedAssets.hasset
./out/build/debug/bin/hyperion_viewer.exe --scene out/content/Scenes/SharedAssets.hasset --frames 900 --hidden --no-ui --no-vsync --capture out/SharedAssetChecks/SharedScene.png --verify-model
~~~

结果：5 个资产校验通过；2/2 模型就绪、2 items/2 draws、900 GPU frames、validation errors=0。截图已目视核对，左侧绿色方形与右侧红色矩形正确，局部值与共享资产默认值相互独立。日志 `out/SharedAssetChecks/Sample.log`，截图 `out/SharedAssetChecks/SharedScene.png`。

深度验收和 RenderDoc 验收增加了原生依赖准备的预热帧预算，继续检查模型就绪、实际场景 draws、像素和 capture replay，未放宽输出断言。

## 使用与边界

使用/API/迁移见 [SharedMaterialAssets.md](../../../../docs/SharedMaterialAssets.md)。本轮不包含编译后 shader 资产、可视化材质图编辑器、纹理压缩/虚拟纹理、源格式运行时 fallback 或孤立代际自动回收。

固定 revision 的原生引用不会自动跟随编辑后的资产；调用方须显式更新引用/重导入图。路径目前做绝对化和 lexical normalization，不折叠 Windows 大小写别名或符号链接。缓存淘汰可能导致后续重新准备，不改变存活对象/帧的有效性。

共享计数来自小型确定性夹具；本轮没有宣称大型场景帧率或加载耗时提升。已有 moving-camera A/B 的像素、覆盖、draw 减少和上传上界回归保持通过。


## 独立审计与最终复验（2026-09-12）

独立 reviewer 审查本轮全部变更，主 agent 复核并修复四项原始问题：待定/失败材质选择保存时丢失、加载结果覆盖已接受编辑、默认原生模型加载 API 沿用旧材质，以及不同固定 revision 的共享资产无法共同发布。第一轮复审发现的声明/预准备快照交叉释放导致重复准备问题也已修复。共关闭 1 项 P1、4 项 P2，原 reviewer 已完成针对性复审，无未关闭的已确认 finding。

最终 Debug 和 Release 完整构建通过；最终完整 CTest 分别 **62/62（328.70 秒）**、**62/62（238.40 秒）**，两套按顺序执行，各包含 27 项 GPU/desktop 测试。日志为 `out/SharedAssetAudit/TestsDebugDelivery.log`、`TestsReleaseDelivery.log`。格式/命名/模块边界和 OpenSpec 严格校验通过。

本轮也补齐了旧测试的异步就绪边界：切换回默认材质后通过既有 AwaitBridge 等待当前绑定版本，SceneViewer 有限预热预算从 180 增至 900 帧；像素、就绪数量、失败隔离、缓存与图像一致性断言均保留。原失败日志、原因、修复版本清单和复审边界完整保留在 [审计报告](audit.md)。运行时修复冻结后，只有这两项测试及交付文档继续调整；未暂存、未提交 Git。
