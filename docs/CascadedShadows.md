# Cascaded Shadow Maps

## 使用

SceneViewer 默认使用单方向光、四级 2048×2048 D32 shadow map，四级每帧更新。`experiments/Scene.json` 的原有多模型场景和地面已经接入阴影；`experiments/Shadows.json` 是接触、斜面、薄片、镂空及镜像投影的专用场景。

```powershell
.\tools\Build.ps1 -Preset release
.\out\build\release\bin\hyperion_viewer.exe --config experiments/Scene.json
.\out\build\release\bin\hyperion_viewer.exe --config experiments/Shadows.json
```

`Directional shadows` 面板提供启用、1024/2048 分辨率、距离、split lambda、receiver bias、normal offset、cascade overlap、距离淡出及光源方向控制。`Cycle shadow display` 在正常着色、cascade 着色、四张真实深度纹理预览之间切换；深度预览在阴影面板下方，隐藏 UI 时在右下角；白色为 depth=1。面板同时显示各 view 的可见物体、draw 数、CPU 准备和最近已完成 GPU 帧的各 pass 耗时。参数是当前会话设置，不写回实验配置。

CLI 对应参数：`--no-shadows`、`--shadow-resolution 1024|2048`、`--shadow-distance 100`、`--shadow-debug 0..5`、`--shadow-light x y z`。光方向表示表面指向光源的单位方向，输入会验证并归一化。默认方向为 `normalize(-0.45, 0.8, 0.65)`，默认 lambda=0.6、normal offset=0.6 texel、receiver bias=0.15 texel、overlap/fade=0.1。按实际场景比例调节距离和偏移；增大偏移会增加接触分离，降低距离可提高单位世界空间的纹理密度。

## 管线与资源

`FForwardRenderPipeline` 在 Renderer 内集中组织一帧：共享场景空间索引维护 → 四个 `ShadowDepth` view → `Forward` 主 view → 可选深度预览 → 扩展/plugin passes。所有 view 使用同一个冻结材质 frame，一次 Render 准备完成后再交给 RHI 0；空间索引每个 family 更新一次。`FRenderSession::ViewStatistics()` 分别报告各 view；原 `Statistics()` 保留“最后 view 的可见性、family 总 draw/batch 数”的兼容语义。

`FCascadedShadowMap` 持有稳定 view identity 和纹理 source。主相机的 forward、up、FOV、near/far 显式放入 `FRenderView::Camera`。当前 pipeline 接受单个透视主相机和单个方向光；普通 `BuildViews` 仍可单独使用。

方向光通过冻结 frame 的 semantic provider 解析，与 Forward 的材质求值使用同一注册规则；允许 Global/Scene/Frame 依赖。若该 provider 依赖 Material/View/Pass/Object/Draw，则保留 Forward 的局部光照求值并关闭全局 CSM，避免一套投影对应多个光源。各阴影视图使用光相机的量化 near-plane origin 作为 Eye；主相机的亚 texel 移动不会单独刷新其 View scope。光方向不变时复用已有正交 basis，避免反复归一化产生浮点漂移。

`FMaterialDepthTexture` 是没有原生依赖的 CPU 描述。Renderer 的材质 GPU cache 将 source identity 解析为同一张 RHI texture，既供 attachment 写入，也供材质反射绑定采样。D3D12 使用 R32_TYPELESS resource、D32_FLOAT DSV 和 R32_FLOAT SRV，支持独立 comparison sampler 及比较函数。四张 2048 贴图的像素 payload 为 64 MiB，1024 为 16 MiB；实际设备统计还包括对齐、常量页及其他场景资源。关闭阴影会保留当前贴图供重新启用；切换分辨率会在引用和 fence 释放后回收旧集合。

`FGraphicsPass` 显式声明颜色使用、深度 attachment 和 sampled depth。Graph 检查初始化、读写冲突与依赖，并生成 `ShaderRead → DepthWrite → ShaderRead` 转换；空 cascade 仍整张清到 1。外部已初始化深度通过 `ImportDepth` 声明。未提交帧取消不会改变 GPU 资源状态，已提交帧及 clear-only pass 都保留纹理直到原有 fence 完成。初始 clear 排入同一 graphics queue，不增加 CPU idle。没有每帧 shadow texture 创建、深度读回或 profiling 专用同步；深度预览直接在 GPU 采样。

显式 sampled-depth attachment 按自身的 D32 格式准备 PSO 和 pass；主视图可独立使用 D32S8。深度预览的纹理与 binding set 跟随 attachment lifetime，隐藏预览后切换分辨率也会在 RHI 回收旧引用；预览的 PSO 和几何继续复用。

普通材质客户端使用已初始化的 1×1 depth=1 及 shadow strength=0 作为缺省输入。自定义 caster 通过声明 `ShadowDepth` usage 接入；未声明该 pass 的材质会跳过投影。透明材质接受阴影但不投影有色/透明阴影。

## 着色与稳定性

`Model.hlsl` 通过 `HYP_SHADOW_CASTER` 和已有 `HYP_ENABLE_INSTANCE` 生成 permutation。各 cascade 共用 shader，改变矩阵和目标。Opaque caster 没有 PS；masked caster 的 PS 只执行与 Forward 共享的 base-color/alpha clip，包含 UV0/UV1、vertex alpha、factor、texture alpha 和 cutoff。保留 double-sided、镜像 winding 和 instance batching。

CSM 使用 uniform/log 混合 splits、旋转不变的 receiver 包围球、filter guard band 与光空间 texel snapping。光源 basis 避免与 up 共线，并在缓慢改变方向时保持连续。Caster 查询将每个 receiver 沿光方向挤出，独立查询共享 BVH，包含主相机外的遮挡物；caster 决定保守 Z 范围，向外量化避免近裁剪遗漏。未知 bounds 保守通过，非有限/退化输入和投影溢出关闭阴影并发布有限缺省值。

Forward 只对方向光直接光照施加 shadow visibility，ambient/emissive 不受影响。固定 3×3 comparison PCF 配合有上限的 raster slope/depth bias、按世界 texel 缩放的 receiver bias 和几何 normal offset、受限 receiver-plane correction。相邻 cascade 在重叠区混合，最远距离淡出；采样越界返回 lit。

固定分辨率无法表示任意细小几何，bias 也存在 acne 与 peter panning 的精度权衡。当前默认值在专用接触、斜面、薄片、mask 和镜像场景中没有观察到明显条纹或分离；不声称消除任意场景比例下的全部 artifacts。初版没有分帧更新、PCSS、VSM、多个 shadowed lights、GPU culling 或 transient aliasing。

## 性能证据（2026-09-09 审计修复后）

NVIDIA GeForce RTX 5080，MSVC Release，1440×900，D3D12 debug layer **开启**，Tracy/RenderDoc/GUI/VSync 关闭。`Showcase.json` 79 个模型已全部 ready，主 view 约 195 items/6 draws；静止阴影总计 643 items/24 draws。每个开/关组合预热 200 帧、采样 800 帧，重复两次并倒换开关顺序；所有 GPU 测量串行执行，不与构建/测试并行。

| 场景 | Pipeline CPU off/on (ms) | CPU 增量 (ms) | Shadow depth GPU mean (ms) | Shadow depth GPU P95 (ms) | Forward GPU 增量 (ms) |
| --- | ---: | ---: | ---: | ---: | ---: |
| 静止 | 0.233 / 0.951 | 0.718 | 0.054 | 0.060 | 0.016 |
| 相机运动 | 1.137 / 3.035 | 1.898 | 0.058 | 0.061 | 0.018 |
| 光源运动 | 0.880 / 3.182 | 2.302 | 0.062 | 0.061 | 0.020 |
| 相机与光源运动 | 1.179 / 3.159 | 1.980 | 0.056 | 0.062 | 0.019 |

mean 为两轮平均，P95 取两轮较大值。GPU timestamp 包括对应 pass 的 barriers、clear、draw，通过已完成的 submission fence 读取。UI 继续显示最近完成的结果，benchmark 另行限量收集全部完成提交，正常 shutdown 等待完成后按 submission ID 匹配 CPU 行；没有增加帧内同步。CSV 中 `gpu_sample_frame = frame + 1`，16 轮分别完整覆盖提交 201–1000，持续测试完整覆盖 201–18200，均无重复、漏帧或溢出。旧版仅采样 latest GPU 结果造成重复和遗漏，本节已用完整提交流重测替换。

保留所有尖峰而不剔除样本：光源运动首轮有两个约 4.3–4.5 ms 的 shadow GPU 样本，因此其平均值可高于 P95；本轮没有定位这些尖峰的具体来源。最初 shadow depth mean≤1ms/P95≤2ms 的目标满足；最初“CPU 增量≤1ms”的工程目标仅静止满足，运动时仍为约 1.9–2.3 ms。多 view 的材质刷新、批次兼容性规划及 frame packet 准备是剩余 CPU 开销；没有通过丢 caster、降低校验或跳过 cascade 更新取得数据。

最终代码额外连续运动 **18000** 个样本：平均帧墙钟 4.308 ms、P95 5.686 ms；shadow depth GPU mean 0.050 ms/P95 0.055 ms，最大单样本 4.730 ms。每帧仍 24 个 shadow draws，caster items 在 614–672 之间。PSO 始终 4、累计描述符始终 136。静止时 instance upload 为零；运动时按可见集合变化上传，单帧最大 23,472 bytes。

长测 GPU allocation 范围为 74,289,152–78,876,672 bytes，结束时 78,417,920 bytes；峰值比预热后增加约 4.38 MiB，期间发生 321 次下降。本轮未见逐帧持续累积的 shadow texture/descriptor/PSO 分配。证据为 `out/audit-csm/performance-final/Sustained.csv` 与 `Summary.json`。

此次修复了 View 数值变化误使纹理 descriptor table 失效的问题，并在单 view 准备期间共享与 Object/Material/Draw 无关的 provider 结果。旧帧仍使用不可变参数，资源值真正变化才更换 resource identity；对象、draw 或混合依赖仍分别求值。GPU cache 的资源使用者也跟随这个 identity，避免仍在使用的绑定因旧 View scope 过期而持续触发后台回收轮询；运动停止后的任务计数回归测试先在旧实现复现失败，再在修复后通过。

复现及原始证据：

```powershell
python tools/MeasureShadows.py --sustained 18000 --output out/audit-csm/performance-final
# 单独测量 1024；数据写入另一个目录
python tools/MeasureShadows.py --resolution 1024 --output out/shadow-performance-1024
```

脚本检查模型 ready、非空主/阴影视图、validation=0、每级 GPU 样本、测量区间提交 ID 逐一完整覆盖、无新增 PSO/descriptor 及有界显存变化。`out/audit-csm/performance-final/Summary.json` 保存全部结果，每轮 CSV 和日志在同目录；硬件相关耗时以记录比较，不作为跨设备固定阈值测试。

## 验证

- `cascaded_shadow_maps`：split/overlap receiver coverage、八种轴向/近轴向光源、极点连续性、texel snapping、主相机外 1000 单位高 caster、退化与溢出输入。
- `material_rendering` 内深度测试：真实 32×32 depth-only raster、64×64 receiver comparison、反向 compare、mask clip、初始及空 pass clear、图初始化/读写冲突、取消未提交帧和 clear-only 引用保留、错误布局/目标拒绝。
- `cascaded_shadow_rendering`：生产 Model shader 的主相机外投影、UV1/vertex/factor alpha、镜像、透明不投影、移除后消影、ordinary/instance 像素完全一致、20 帧相机/光源运动无 descriptor/PSO 增长、1024/2048 三次切换回收、实际深度预览及运动停止后的后台任务静止。审计增加混合 D32S8/D32、light provider 覆盖/缺失/全部局部依赖、隐藏预览后换分辨率、亚 texel 规划复用和真实变化失效、完整 GPU 捕获及溢出回归。
- Debug RenderDoc 验收实际 capture/replay Triangle、Model 和 Shadows，并分别检查 Forward、四级 ShadowDepth 与 UI 的已提交绘制。
- 审计修复集通过 Debug 全量 CTest 48/48（含 RenderDoc）、Release 全量 CTest 43/43（未编入 RenderDoc）；最后补齐 Pass 白名单后，两配置全量构建及相关 8/8 回归通过，独立 reviewer 另行运行最终 GPU 回归通过。286 个自有源码的路径/格式检查、177 个翻译单元的语义命名、277 个源码/25 模块的边界检查以及 OpenSpec strict validation 通过。审计日志位于 `out/audit-csm`；此前图像证据仍位于 `out/captures/Shadow*`，均不纳入源码提交。

OpenSpec 已同步主规范并归档至 [2026-09-09-add-cascaded-shadow-maps](../openspec/changes/archive/2026-09-09-add-cascaded-shadow-maps/proposal.md)。

后续材质、批次和原生录制优化的完整 A/B 数据与剩余 CPU 目标差距见 [RendererCpuPerformance.md](RendererCpuPerformance.md)。

静止 CSM setup/view/packet 复用、统一 RHI 帧协调以及第二轮相机运动对照见 [RetainedRenderFrames.md](RetainedRenderFrames.md)。setup 复用依赖真实 camera/light/settings 和稳定 scene/resource revision；四级阴影仍逐帧渲染，动态查询客户端保持保守回退。
