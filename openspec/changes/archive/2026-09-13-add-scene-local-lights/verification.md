# 局部光验收记录

日期：2026-09-13。实现范围为 Scene 点光/聚光、可扩展可见性查询、Deferred 光体积、SceneViewer 编辑和 Sponza 点光效果。已完成 proposal、实现与验收；实现阶段按原要求未提交，后续按用户请求完成独立质量审核，并进入 OpenSpec 归档与 Git 提交流程。

## 独立质量审核

不继承会话上下文的 reviewer 对本次 65 个文件进行了独立审核，确认一项 P1 问题 LL-01：点光属性修改替换 Scene 节点后，属性面板仍解引用旧节点。修复让光源属性面板使用调用方持有的节点值快照；setter 继续读取当前 Scene 状态，仅修改对应光源参数。独立复审确认旧指针不再被访问，并关闭 LL-01，未发现其他已确认缺陷。

修复后完成 Debug Viewer/相关测试目标构建与 Release 增量构建，两个配置的 `scene_viewer_controls`、`gui_input_and_data` 各 2/2 通过；格式、26 个 C++ 编译单元的语义命名及补丁空白检查通过。原缺陷和修复由完整 C++ 生命周期分析确认，未宣称通过原生 GUI 自动输入或 ASan 复现；上述测试为辅助回归证据。

审核报告和修复验证日志位于 `out/LocalLights/Audit/`，包括 `Reviewer.md`、`Rereview.md`、`Summary.md`、`FixBuild-*.log`、`FixTests-*.log`。归档前逐项校验全部 65 个文件与最终复审快照一致；归档仅同步规格、补充本记录并修正归档后的图片相对路径，不改动已审核代码。

## 构建与自动验证

| 检查 | 结果 | 日志（仓库根目录下） |
|---|---|---|
| Debug 全量构建与 CTest | 63/63 通过 | `out/LocalLights/FinalDebug.log` |
| Release 全量构建与 CTest | 62/62 通过 | `out/LocalLights/FinalRelease.log` |
| 最后常量退休修复后的 Debug/Release 全量构建 | 通过 | `RetirementDebugBuild.log` / `RetirementReleaseBuild.log` |
| 最终 Debug/Release 相关 CTest | 各 6/6 通过 | `RetirementCtest-debug.log` / `RetirementCtest-release.log` |
| 所有源文件格式、文件名及 include 大小写 | 470 个源文件通过 | `Style.log` |
| 修改的 C++ 编译单元语义命名 | 26 个通过 | `Naming.log` |
| 模块依赖边界 | 447 个源文件、28 个模块通过 | `Boundaries.log` |
| 补丁空白及 OpenSpec strict | 通过 | `DiffCheck.log` / `OpenSpec.log` |

未写明目录的日志均位于 `out/LocalLights/`。全量测试之后补充了尺寸变更、光源中心、静态多灯常量回收回归，并修复了常量退休；随后重建两个配置，复跑相关的 material_rendering、cpu_frame_ownership、cpu_frame_pipeline、render_graph、deferred_rendering、render_resources 六项。最终验证没有关闭 D3D12 debug layer。

Scene/source/native 测试覆盖两种独立类型、无效参数原子拒绝、父级变换与启用状态、源 v3/native v5/节点 v2 的往返及旧版本迁移。空间测试使用 128 个点光和一个聚光，并通过自定义 `ISceneVisibility`、Linear/BVH 一致性、参数更新不 refit、边界更新 refit 和移除验证扩展契约。Shader 测试编译 DXIL/SPIR-V/MSL，并验证 Forward 不含局部光绑定。

真实 D3D12 回归在标准 Z 和 reversed-Z 下分别验证：

- 点光与共用 BRDF 的法向入射解析值、两灯线性叠加、ambient/emissive 不重复。
- 相机位于体积外、边界、内、光源中心及穿越正 W 平面时的覆盖；出口超过 far、近裁剪交叉、子 viewport 和非默认 depth range。
- 同一法向接收点在八个相机位置均得到约 0.189005 的反算 HDR 值，没有叠加次数变化。此数值由最终 8-bit sRGB 图像逆变换所得，包含量化误差，并非原生 HDR readback。
- 聚光内锥/半影/外锥、转向、距离截断、接收点位于点光中心、光源中心在屏幕外但影响范围仍可见。
- Forward 和透明接收体排除局部光；旧排队帧在灯关闭、管线与 GBuffer 变更后仍保留原结果。
- 带灯反复 resize、灯位置更新、13 盏不同强度点光在静止相机下连续绘制；PSO/描述符复用，显存增量保持在测试允许的四个 64 KiB 页内。

验收曾发现静止 Sponza 的常量页在 400 帧内单调增加 640 KiB。根因是临时材质快照不是受跟踪的引擎 scope，静止帧没有发出退休通知。最终光体积 pass 单独持有常量 scope，保留材质 identity/revision，并在 pass 退休时触发现有收集流程；GPU buffer slices 继续通过已有 fence 生命周期保活。没有加入 GPU wait、每灯 PSO 或逐帧强制清空缓存。

## Sponza 图像验收

参考：[Khronos Sponza README Screenshot](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/main/Models/Sponza/README.md)，原图 `screenshot/large.jpg`；本地查看副本为 `out/SponzaMigration/Reference.jpg`。上游说明灯光不包含在资产中。按参考图地面光斑位置向地面反投影后，使用真实截图调整了三处暖色点光。

| 灯 | 世界位置 | RGB | 强度 | Range |
|---|---|---|---|---|
| 1 | (-4.5, 1.35, -0.03) | (1, .94, .8) | 14 | 5 |
| 2 | (-.95, .85, -.04) | (1, .94, .8) | 7 | 3.8 |
| 3 | (5, .85, -.23) | (1, .94, .8) | 7 | 4 |

已持久化到 `assets/Scenes/Sponza.json`；正常构建生成 `out/content/Scenes/Sponza.hasset`。保持原始模型、材质、镜头、平行光、环境光、曝光和分辨率。1440×728、曝光 2.5、reversed-Z、CSM 开启。每次截图绘制 1200 帧，并检查模型 1/1 ready、79 个主视图模型 draw、局部光 3 个 draw、GPU validation=0。关闭灯的副本仍保留三个节点但无局部光 draw；Forward 的 active=0、draws=0。

最终效果：

![Sponza 点光开启](../../../../out/LocalLights/SponzaFinal.png)

同机位关闭点光：

![Sponza 点光关闭](../../../../out/LocalLights/SponzaOff.png)

`SponzaDebug.png`、`SponzaReload.png`、`SponzaNone.png`、`SponzaLinear.png` 与默认 BVH 的 `SponzaFinal.png` 逐像素相同。`SponzaSaved.hasset` 经 Viewer 保存后重新加载；节点和可见效果保留。图像及数值断言见 `AcceptanceResults.json`。

沿走廊三个地面区域的平均 RGB（0–255、x=[700,760)，y 中心±8；仅作为局部增亮证据）：

| y 中心 | 点光关闭 | 点光开启 |
|---|---|---|
| 710 | 102.50 | 198.99 |
| 638 | 103.14 | 202.11 |
| 587 | 100.21 | 173.11 |

与参考图的三处柔和地面光斑及附近柱面、旗帜照明接近。参考图的局部遮挡阴影、GI/IBL、天空和其他后处理未重建，因而不宣称像素级还原。局部阴影、RectLight、cluster lighting 和 Forward 多局部光均在本任务范围外。

## 隔离测量

RTX 5080，Release，D3D12 debug layer 开启，Tracy 编译关闭，CSM 开启，1440×728，VSync 关闭。每次 1300 帧 warmup 后记录 400 帧；benchmark 自身拒绝未加载场景并匹配已完成提交的 GPU timing。最终测量与其他 GPU 测试串行。

| 场景 | 局部光 GPU mean / p95 (ms) | 光源查询 mean (ms) | 全帧 CPU mean (ms) | 全设备 GPU allocation (MiB) |
|---|---|---|---|---|
| 静止相机 | 0.01521 / 0.01504 | 0.00117 | 5.275 | 497.820–497.820 |
| 移动相机 | 0.01489 / 0.01507 | 0.00102 | 7.276 | 497.820–498.008 |

两个样本段均为 3 个可见点光、3 个局部 draw；局部和模型索引 rebuild/refit 均为 0。全设备 PSO 数保持 8、描述符分配数保持 745；显存只在少量 64 KiB 常量页范围波动。全设备分配包含 Sponza 纹理、CSM、GBuffer 和现有资源，不能解释为局部光自身占用。CPU 时间受桌面调度影响，此处不建立通用性能阈值，也不据三个点光推断大量灯光的扩展性。

复现命令示例：

```powershell
./tools/Build.ps1 -Preset debug -Test
./tools/Build.ps1 -Preset release -Test
./out/build/release/bin/hyperion_viewer.exe --config experiments/Scene.json --hidden --frames 1200 --capture out/LocalLights/SponzaFinal.png --verify-model
./out/build/release/bin/hyperion_viewer.exe --config experiments/Scene.json --hidden --frames 1700 --no-vsync --benchmark-warmup 1300 --benchmark out/LocalLights/SponzaStatic.csv
# 同一命令追加 --benchmark-camera 得到移动相机样本。
openspec validate add-scene-local-lights --strict
```

最初 benchmark 命令误带仅适用于截图的 `--verify-model`，被 CLI 拒绝；修正参数后重新测量。早期未加载模型的短帧数截图未用作验收。最终原始日志、逐帧 CSV 和图片均保留在 `out/LocalLights/`。
