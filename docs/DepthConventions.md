# 深度约定与 reversed-Z

Viewer 默认启用 reversed-Z；ModelViewer、SceneViewer 和 Triangle 的示例配置均显式启用。配置字段位于实验 JSON 的 `properties` 中：

```json
"reversed_z": true
```

设为 `false` 并重启可恢复标准 Z。未提供此字段的旧实验配置默认采用 reversed-Z。此设置仅在启动时读取；诊断面板显示 `Reversed Z (restart)`、本次有效模式，以及修改后需要保存并重启的提示。保存配置保留下次启动值，已排队和随后提交的帧仍使用本次启动值。不会自动监视配置文件。

## 渲染行为

| 行为 | 标准 Z | Reversed-Z |
| --- | --- | --- |
| 物理 Near 映射 | 0 | 1 |
| 物理 Far 映射 | 1 | 0 |
| 深度清除值 | 1 | 0 |
| 内建材质默认深度比较 | LessEqual | GreaterEqual |
| 正标准深度偏移的有效符号 | 正 | 负 |

继续使用右手、零到一裁剪空间与 D32 浮点深度。相机的 Near/Far 仍是正值且 Far > Near，首版要求有限远平面。Viewport 的 MinDepth/MaxDepth 仍按升序设置。

Forward、Deferred Base、Compatibility、Transparent、Legacy FrameDepth 和 CSM 共享同一有效约定。透明排序保持由远到近；Deferred 通过匹配的逆 ViewProjection 重建世界坐标，不通过固定深度清除值判断 GBuffer 覆盖。CSM 的投影、比较采样器、目标清除、中性纹理和 caster/receiver 深度偏移一起适配；世界空间 normal offset 保持几何意义。

Triangle 仍以标准深度的裁剪空间描述屏幕形状，已开启深度测试和写入；Renderer 在生成 WVP 时映射其深度，World 保持几何变换，避免将深度翻转误判为镜像。UI 与不启用深度的 fullscreen pass 保持原行为。深度调试面板显示原始深度：标准 Z 的空白深度为白色，reversed-Z 为黑色。

## 接口约定

- Math 拥有 `EDepthConvention`、`GetDepthClearValue`、`GetDepthDirection` 和 `ClipDepthTransform`，不依赖 RHI。
- `Perspective(..., Convention)` 直接生成对应投影系数。省略第五个参数时保留标准 Z，兼容显式构造投影的现有低层调用方。
- 自定义 fullscreen pass 通过 `FFullscreenPassDesc::DepthConvention` 指定其深度约定；使用原始 SV_Depth 输出时由 shader 保证一致。
- `FRenderPrimitiveState::bClipSpace = true` 时，顶点经 World 变换后的深度使用标准 Z（near=0、far=1）。Renderer 在 WVP、裁剪和透明排序时统一映射到视图约定；不要预先把 `ClipDepthTransform` 乘进 World。几何镜像、法线和 OrientationSign 仍由 World 决定。
- `FRenderView::DepthConvention` 必须与 ViewProjection 一致。低层默认仍是 Standard；Viewer 显式设置启动时选定的约定。
- `FRenderPassTargets::Frame(DepthFormat, ClearColor, Convention)` 与 `FRenderSession::FrameTargets(ClearColor, Convention)` 生成对应的附件清除值。自定义附件的 ClearDepth 是原始数值，由调用方明确设置。
- `FRHISwapchainDesc::DepthClearValue` 是资源创建时的 optimized clear 值，须与该 swapchain 的 FrameDepth 清除约定一致；RHI 不隐式改写比较或采样器。

内建模型和 Triangle 材质设置 `FMaterialState::bViewRelativeDepth = true`，将材质声明的标准 Z 比较和偏移按视图解析。Less/Greater 与 LessEqual/GreaterEqual 在 reversed-Z 时交换；Equal、NotEqual、Always、Never 不变；Stencil 比较不受影响。无法取负的最小 int32 深度偏移会在反向解析时拒绝。

自定义材质默认 `bViewRelativeDepth = false`，显式原始比较保持原意。若希望跟随视图，使用标准 Z 语义声明并开启此标志：

```cpp
Pass.State.bDepthTest = true;
Pass.State.bDepthWrite = true;
Pass.State.DepthCompare = EMaterialCompare::LessEqual;
Pass.State.bViewRelativeDepth = true;
```

消费 `Engine.Object.WorldViewProjection` 的裁剪空间 shader 自动得到映射后的矩阵。绕过该语义、直接输出裁剪坐标的自定义顶点着色器，以及 SV_Depth 输出和深度采样逻辑须自行遵守所在 View 的约定；不通过机械改写 shader 输出为任意自定义材质自动转换。普通和实例绘制共用有效状态解析，PSO、绘制包及批次准备缓存区分不兼容的约定。

## 验证

`depth_conventions` 检查投影端点、单调性、裁剪、重建、清除和原始/相对材质状态。`cascaded_shadow_maps` 检查双约定的级联覆盖、缓存和中性纹理。`deferred_rendering` 与 `cascaded_shadow_rendering` 在两种约定下执行像素、透明混合、普通/实例绘制和资源生命周期回归。裁剪空间回归还覆盖 Front/Back/None 剔除、SV_IsFrontFace、真实镜像、OrientationSign、近远裁剪及只变更深度约定的参数缓存失效。

`depth_viewer_acceptance` 启动三个应用的 Standard/Reversed × Forward/Deferred 组合，并测试配置编辑、保存和重启。内部验收选项 `--exercise-depth-config` 在第三帧翻转待保存设置，至少需要四帧；它不提供运行时深度切换。运行日志报告 `Depth convention: active=...; configured=...`，用于区分有效值与保存值。

本次开发证据记录在 [implementation.md](../openspec/changes/archive/2026-09-11-add-reversed-z/implementation.md)。
