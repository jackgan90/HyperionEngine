# 选中物体轮廓

编辑器在场景点选或 Outliner 选择模型后显示橙色外轮廓。默认宽度为两个视口物理像素；不会标出三角形、法线变化或同一模型内部的 section 接缝。轮廓不测试场景深度，因此被其他物体完全挡住时仍可见。模型真实孔洞和 alpha 裁剪形成的边界也属于剪影。

视口选项的 **Selection outline** 提供两种模式，修改立即生效，不修改场景文件或撤销历史：

| 模式 | 多个目标相互重叠 | 成本 |
| --- | --- | --- |
| Union（默认） | 合并覆盖区域，仅描并集边界 | 一次 mask 绘制与一次边缘提取 |
| Per object | 分别保留每个对象的完整外轮廓 | 每个对象一次 mask 绘制与边缘提取 |

两种模式最后都只合成一次颜色；交叉处取覆盖率最大值，不重复加亮。**Smooth outlines (2x)** 使用两倍宽高的 mask，平均子像素覆盖率，轮廓宽度仍以原视口像素计量。超出设备贴图尺寸限制时退回普通质量。

当前选择手势仍为单选，因此日常单选时两种模式的结果一致。Renderer 输入从一开始就接受多个对象；相同对象的所有 primitives 必须放在同一个分组中。选择父节点不会隐式递归其子节点。非几何灯光/相机图标沿用编辑器自身显示。

## 数据与模块边界

Editor 在 Main 冻结 `FSelectionOutlineRequest`：`SceneInstance::ResolveRenderPrimitives` 经 SceneBridge 得到每个场景对象的 primitive 句柄分组，并携带与 frame seed 相同的 publication token。Render 拒绝不匹配的 publication，直接收集有效 generation 的 primitives，并保留变换、可见性、材质和对象参数。独立于绑定场景使用 Renderer 时，frame 和 request 都不带 publication。

通用代码位于 Runtime/Renderer，Editor 在启动时显式加入 `MakeSelectionOutlineFeature`。Viewer 和默认 pipeline 不激活此功能。`AddSilhouetteOutlinePass` 与 `AddOutlineCompositePass` 可独立复用；同一 graph 中调用多次时需提供唯一 pass 名称。Scene 和 RHI 不依赖 Editor。

```text
owned primitive groups -> coverage mask(s), without depth
    Union: draw all -> exterior once
    Per object: clear/draw one -> exterior -> maximum accumulation
    -> single alpha composite after tonemap -> GUI / gizmos
```

边缘提取对二值覆盖 mask 膨胀，再减去原 mask；不读取场景深度或法线。复用一张 R8 mask 和一张 R8 轮廓纹理：普通质量约 `2 * width * height` 字节，2x 质量约 `5 * width * height` 字节，不随对象数量增加。空选择和无有效相机时释放独占缓存；尺寸变化通过已有资源 scope 和 GPU fence 退休。每次 mask 绘制清空覆盖范围，避免切换模式或减少选择时残留。

最终 pass 对加载后的输出 attachment 作 alpha blend，使用 sRGB RTV；不采样同一输出 attachment，保留目标 alpha。线性显示颜色由 `FSelectionOutlineSettings::Color` 配置，不受场景曝光、色调映射影响；宽度范围为 `(0, 8]`。

## 材质覆盖

标准 `Model.hlsl` 材质通过运行时缓存的辅助定义使用 `SilhouetteMask` permutation，复用 `ClipModelAlpha`，保留 UV、纹理、alpha cutoff、对象 override、实例化与剔除设置。不会修改材质资产。透明混合材质目前使用几何剪影，alpha mask 使用原裁剪规则。

自定义材质可显式提供 `SilhouetteMask` usage，覆盖样本输出 `1`，未覆盖样本裁剪，形成二值剪影，并负责自己的顶点变形和裁剪。模块保留源编译接口中的完整参数及覆盖值，由剪影 pass 的编译结果决定实际使用的参数；同时强制实心、单通道、不写深度及不测试深度。不支持的 shader family 会被跳过并输出诊断。首次使用辅助材质时异步准备，未就绪部分暂不产生轮廓。

## 对比与验证

```powershell
out/build/debug/bin/hyperion_editor.exe --exercise-outlines out/outline-comparison --layout out/outline-comparison/Layout.ini
ctest --test-dir out/build/debug -R '^(selection_outlines|editor_outlines)$' --output-on-failure
```

对比入口构造两个重叠 Cube，输出 `Union.png`、`PerObject.png`、`UnionAgain.png`、`Occluded.png`、`Smooth.png`、`Cleared.png` 后退出，不保存测试场景。它通过正常 SceneInstance、Editor 离屏视口和 GUI 合成路径提交多目标请求，不改变日常选择手势。图像在指定输出目录，可重复运行比较。

`selection_outlines` 使用 D3D12 图像读回验证重叠与完全包含、遮挡、section 合并、alpha mask、镜像/零缩放、曝光、Forward/Deferred、深度约定、选择清除与失效、resize、无相机、无 feature 和 GPU validation。`editor_outlines` 验证真实编辑器合成、模式切换、取消选择、文档不变及插件不可用时的受控失败。
