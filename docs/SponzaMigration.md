# Sponza 示例场景

当前 Content 归属、挂载配置及外部资产重建见 [Content 与虚拟文件系统](ContentFileSystem.md)。

SceneViewer 的 `experiments/Scene.json` 打开 HyperionAssets 中的 Khronos Sponza 示例。场景包含一个模型节点、一台相机、主方向光、三盏点光和 Cloudy 天空环境；这些相机和光源由场景清单定义。

## 运行与操作

```powershell
./tools/Build.ps1 -Preset debug -Target hyperion_viewer
./out/build/debug/bin/hyperion_viewer.exe --config experiments/Scene.json
```

默认窗口为 1440×728，曝光为 2.5，调试面板显示；Tab 切换面板。WASDQE 连续移动、右键环绕、滚轮推拉、Home 取景。相机不会自动旋转；未保存的编辑在重启后恢复为源场景状态。其他场景可通过 `--scene /Game/Scenes/Showcase.hasset` 选择。Viewer 不带参数时仍打开 Triangle 实验。

| 文件 | 作用 |
| --- | --- |
| [Scene.json](../experiments/Scene.json) | 窗口、曝光、面板与场景入口 |
| `Sponza.json`（见 HyperionAssets 的 Metadata 与本地源缓存） | 模型、相机镜头、层级与光源节点 |
| `Sponza.gltf`（见 HyperionAssets 的 Metadata 与本地源缓存） | 上游模型，关联 bin 和全部源图片 |
| `HyperionAssets/Metadata/Sources.json` | 固定上游版本、下载 SHA-256 和场景配方 |
| [PrepareContent.py](../tools/PrepareContent.py) | 显式恢复来源、调用 AssetTool 导入和构建 catalog |

Sponza 原生资产位于 HyperionAssets，由 `/Game/Scenes/Sponza.hasset` 加载。原始模型和图片仅保存在忽略的源缓存；固定版本、哈希和自创场景配方记录在 `Metadata/Sources.json`。普通构建不下载或转换样例；重建流程见 [ContentFileSystem.md](ContentFileSystem.md)。

## 来源

固定源版本与文件清单以 HyperionAssets 的 `Metadata/Sources.json` 为准；上游说明和许可分别保存在 `Metadata/SponzaREADME.md`、`Metadata/SponzaLicense.txt`。源 glTF 有一个节点、一个 mesh 和 103 个图元，使用 25 个材质、69 张图片；没有相机或灯光节点。场景光源是项目另行创作的内容。

## 相机与光照

相机的世界变换和镜头保存在场景节点中。初始位置约为 `(-10.521811, 1.233887, 0.461733)`，垂直 FOV 为 `0.729224966` 弧度（约 41.78°），near/far 为 `0.05 / 100`，focus distance 约为 13。投影、CSM 与 Fit 使用选中相机的镜头参数，其他场景不会被强制套用 Sponza 的 FOV。相机可在面板编辑并随场景保存；glTF 相机导入仍未实现。

默认使用 Deferred、compact GBuffer、reversed-Z 和聚簇光照。主方向光使用四级 CSM；三盏暖色点光照亮庭院。Cloudy 天空提供背景、SH 漫反射和 GGX 镜面 IBL；Scene 面板可换为 Dusk、Clear 或自定义原生天空。关闭天空背景仍保留天光。

这些设置是对参考构图与照明的近似，不保证逐像素匹配。局部光没有阴影；天光没有场景遮蔽或多次反弹 GI。曝光与 Reinhard 输出、GBuffer 量化、纹理过滤和切线生成也会影响最终画面。详细契约见 [局部光](LocalLights.md)、[聚簇光照](ClusteredLighting.md)、[天空与 IBL](SkyLighting.md) 和 [Deferred 渲染](DeferredRendering.md)。

## 资产与验证边界

源模型与纹理保持原始内容，导入时生成独立材质、RGBA8 纹理和完整 mip 链。原生资产体积不能与压缩 JPEG/PNG 源文件体积直接比较。没有引入纹理压缩或流式加载。

原始迁移验收属于 2026-09-12 的场景版本，不能代表加入场景相机、局部光和天空后的截图或资产数量。当前验证入口见 [Verification.md](Verification.md)，对应功能的具体测试和历史证据由各专题文档链接到 OpenSpec 归档。
