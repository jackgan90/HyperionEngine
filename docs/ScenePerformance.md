# Scene Viewer 性能定位与验证

2026-09-08，基于 `6a4b147` 定位并修复。本机 NVIDIA GeForce RTX 5080，VS 2022 x64，`experiments/Scene.json`，1440×900，78 个模型，194–195 个场景 draw，另有 GUI draw。保留 D3D12 debug layer、索引校验和 GPU fence 生命周期保护；Tracy 关闭。

## 原因与修复

1. **CPU 从 GPU upload 内存扫描索引。** Debug UI 每帧重建索引 buffer，范围校验每帧重新 Map 并读取 upload heap。原静止 Release 的命令录制约 21.7 ms/帧，Debug 约 23.8 ms/帧。现在在创建不可变索引 buffer 时保存普通 CPU 内存副本，校验读取副本；顶点与索引上传使用明确的 usage。CPU 索引副本随 buffer 销毁，大小不超过索引 buffer；每个 buffer 的范围缓存仍最多 32 项，超过上限仍执行校验。这与 [Microsoft 对 upload heap CPU 读取的说明](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12resource-map)一致。
2. **相机变化触发全部材质重新求值和绑定。** 原 Debug 运动时每帧约 155.7 ms 用于材质准备、51.6 ms 用于 draw 准备。冻结后的 Renderer 参数按值共享不可变引用，刷新只复制引用和改变的值，不再深复制每个参数内部的动态数组。现在在 program、snapshot、Object 和 override 不变时，只更新依赖已改变引擎 scope 的参数；混合 Object/View、瞬态 scope 等仍走完整校验路径。数值刷新保持资源身份，复用已准备的纹理、采样器和 PSO；资源参数变化更新身份，命中时仍检查实际资源值。新结果在验证和分配成功后发布，旧帧保持不可变。
3. **缓存命中仍产生大量临时复制。** 常量缓存直接从绑定输入计算 hash，并完整比较 layout、mapping、scope 和数值，只有 miss 才创建持久 key；默认 provider 命中时直接比较已校验输入，保留重复名称、类型和所有权检查。没有新增随帧数增长的缓存；评价缓存仍由 primitive 管理并限制每 primitive 64 个条目，draw 缓存在资源身份、geometry 或 surface 失效后回收，常量页仍受 slice 引用和 GPU fence 约束。
4. **原配置打开 VSync。** 增加 `--no-vsync` 作为启动覆盖；配置文件仍保留原来的显示设置。性能数据应区分同步等待与渲染吞吐量。

## 复现命令

```powershell
# 正常交互，解除 VSync 限制
out/build/vs2022/bin/Release/hyperion_viewer.exe --config experiments/Scene.json --renderdoc --open-rdc --no-vsync

# 小幅连续相机运动；120 帧预热，记录随后 1000 帧
out/build/vs2022/bin/Release/hyperion_viewer.exe --config experiments/Scene.json --renderdoc --open-rdc --no-vsync --frames 1120 --benchmark-warmup 120 --benchmark-camera --benchmark out/ScenePerformance/Moving.csv
```

去掉 `--benchmark-camera` 测静止；去掉 `--renderdoc --open-rdc` 比较未加载 RenderDoc 的开销。基准模式输出每帧 CPU 墙钟耗时及实际场景 draw 数，包含输入、GUI、材质准备、命令录制、提交与 Present 等待，不是 GPU timestamp。CSV 在结束后写入；必须使用大于 warmup 的有限 `--frames`；若第一条样本时 Scene 尚未就绪则明确报错，应增加预热帧数，避免把未加载场景的空帧算作性能提升。相机运动模式暂时接管 SceneViewer 相机输入，保持可见负载近似相同。

## 数据与回归

最终数值记录在 `out/ScenePerformance/FinalResults.json`，逐帧 CSV 和完整日志位于同目录。修复前使用临时嵌套 CPU 计时探针，60 帧预热后采集 100 个 Debug 或 200 个 Release 样本；修复后移除探针，120 帧预热后采集 600 个 Debug 或 1000 个 Release 样本。所有对照依次运行，避免同时构建或运行 GPU 测试。表中 FPS 是 `1000 / 平均帧毫秒`，不是显示器实际刷新率。

| 构建与场景 | 修复前 FPS | 修复后 FPS | 修复后平均 ms | 修复后 P95 ms |
| --- | ---: | ---: | ---: | ---: |
| Debug 静止 | 29.25 | 80.46 | 12.428 | 14.719 |
| Debug 连续运动 | 4.12 | 37.13 | 26.930 | 33.469 |
| Release 静止 | 43.56 | 388.91 | 2.571 | 3.056 |
| Release 连续运动 | 31.66 | 266.07 | 3.758 | 4.914 |

以上四项保留 RenderDoc 和 GUI，并关闭 VSync。未加载 RenderDoc 的 Release 连续运动为 271.87 FPS，表明本场景的主要损失来自原 CPU 路径。测试期间没有主动抓帧；主动 capture 的瞬时开销由独立验收覆盖，不包含在这些帧率中。

Debug 连续运动由 242.73 ms 降至 26.93 ms，仍明显慢于优化后的 Release；此次没有更改 Debug 优化级别、STL 调试检查或 D3D12 校验。原配置的 VSync 保持开启时存在明显帧间波动：240 样本约 72.89 FPS，额外 320 帧预热后的 1000 样本约 87.21 FPS（`VsyncRetest.csv`）。这些包含同步等待的结果不能代表最大渲染吞吐量，交互时需要高帧率可使用 `--no-vsync`。

新增回归覆盖连续 128 次 View 更新时 Object/Material 常量、PSO 与 descriptor table 的复用、旧帧回放、缓存回收、View 纹理切换与缺失后恢复、默认 provider 命中后的重复输入拒绝，以及索引范围缓存满后的错误索引和部分初始化索引校验。`scene_moving_camera` 验收真实 Viewer 的相机运动、预热过滤、194–195 draws 和 VSync 覆盖，避免仅测试静止镜头。

最终 VS 2022 Debug **43/43**（122.03 秒）、Release **43/43**（74.41 秒）通过，含实际 RenderDoc capture/replay、失败 Present 恢复和既有 514 模型场景验收；Ninja Debug Viewer 构建通过。227 个自有源码的格式/路径、139 个编译单元的命名、222 个模块源码的依赖边界以及 `git diff --check` 通过。日志为 `out/ScenePerformance/VerifiedDebugTests.log`、`VerifiedReleaseTests.log`、`NinjaDebugBuild.log`、`Naming.log` 与 `VerifiedBoundaries.log`。
