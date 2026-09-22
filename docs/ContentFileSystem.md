# Content 与虚拟文件系统

引擎资产统一放在 `Content`，通过 `/Engine` 引用。独立 `HyperionAssets` 仓库保存模型、场景、天空、演示材质和演示 Shader，通过 `/Game` 引用。Shader 仍是 HLSL/HLSLI 文本，没有新增 hasset 封装或 Shader metadata。

## 新环境运行

准备引擎的构建依赖，将 HyperionAssets 检出到引擎的同级目录，并在资产仓库执行 `git lfs pull`。普通 Viewer 构建不下载、不导入示例原始资产。

```powershell
./tools/Build.ps1 -Preset release
./out/build/release/bin/hyperion_viewer.exe --config experiments/Scene.json
./out/build/release/bin/hyperion_viewer.exe --scene /Game/Scenes/SharedAssets.hasset
```

`ContentMounts.json` 的默认映射是 `/Engine -> Content`、`/Game -> ../HyperionAssets`。可复制为被忽略的 `ContentMounts.local.json` 修改本机位置，或用 `--mounts path/to/Mounts.json` 显式指定配置。相对目录以挂载配置文件所在目录为基准；相对 CLI 配置文件参数以当前工作目录为基准。

```json
{
  "version": 1,
  "mounts": [
    {"root": "/Engine", "directory": "D:/Engine/Content", "read_only": true},
    {"root": "/Game", "directory": "E:/MyContent", "read_only": false}
  ]
}
```

挂载配置在应用组合层加载，请求运行期间保持冻结。编辑器通过 assets 插件提供的 `FContentRootService` 验证候选挂载和资产索引；调用者停止旧内容生产、释放场景及渲染引用并等待工作结束后，才可在 Main 独占提交新 `/Game`。提交清空旧资产缓存和资产索引，并替换已验证的挂载集合，文件系统与服务对象地址保持稳定。其他调用者不得在存在请求或旧内容引用时直接替换挂载。

包路径使用 UTF-8 和 `/`，名称大小写必须与磁盘一致；拒绝越过挂载根的 `..`、重复/重叠映射及挂载根之下的符号链接/junction。Engine 默认只读；工具显式 `--authoring` 才可覆盖挂载的只读设置。路径查找失败不会退回旧目录。编辑器的目录选择、最近目录和启动恢复规则见 [Editor.md](Editor.md)。

## API 与模块

`Runtime/IO` 的 `FMountedFileSystem` 实现 `IFileSystem`，提供 Normalize、Resolve、Read、ReadRange、ReadTree、WriteAtomic、Remove、Exists、Enumerate、ListDirectory 和 AcquireWriteLease。ListDirectory 返回直接子文件、目录及逐项诊断，保留空目录并阻止跟随挂载内链接。ReadTree 将递归枚举、扩展名筛选和读取合并，返回文件路径与字节；每个文件独立受读取大小上限约束。目录和链接检查在本次操作内完成，不跨请求缓存文件内容或校验结果，不承诺多个文件的原子快照。`FIOService` 保留 IO 域调度、统计、取消和原子发布能力。实际文件访问在本地后端完成，内存文件系统仍用于独立测试。

`FAssetService` 在缓存、写入顺序、失效和依赖图处理中使用同一规范路径；启动和换根从文件头重建 Engine/Game 索引，重复 ID 报错，不需要持久化 Catalog。引用检查 ID 和类型；作者保存/导入的引用不固定 Revision。旧相对引用可读；新挂载资产的发布和场景保存使用包路径，搬迁只需修改挂载配置。

源导入、截图、缓存和隔离测试仍可显式使用本地路径。业务资源入口使用 `/Engine/...` 或 `/Game/...`。Scene、Environment 等 CPU 数据模块不依赖 Renderer/RHI。

Shader 编译器通过同一文件系统读取主文件和 include。Game Shader 可 include `/Engine/Shaders/...`；相对 include 按编译器源目录搜索。DXIL、SPIR-V、MSL 继续使用原有编译/反射流程。缓存包含逻辑文件位置、源目录内所有扩展名的文件内容、选项和工具链版本，保存在 `out/shader-cache`；物理目录搬迁不改变包路径 Shader 的缓存身份。缓存必须位于 Shader 源目录之外，指向挂载内目录时也遵守挂载的权限、大小写和链接检查。

## 导入与重建

正常运行只需要发布后的 hasset 和文本 Shader。原始下载文件不会进入资产仓库的提交。

```powershell
# 从固定清单下载缺失来源、恢复自创场景配方并发布；已有来源先校验 SHA-256。
python tools/PrepareContent.py --assets-root ../HyperionAssets
# 缓存齐全时进行完全离线重建。
python tools/PrepareContent.py --assets-root ../HyperionAssets --offline
# 只恢复源缓存，用于导入开发和测试。
python tools/PrepareContent.py --restore-only

./out/build/release/bin/hyperion_asset_tool.exe --mounts ContentMounts.json import D:/Sources/Model.glb /Game/Models/Custom.hasset --library /Game --source-root D:/Sources --source-id custom
./out/build/release/bin/hyperion_asset_tool.exe --mounts ContentMounts.json validate-library /Game
```

默认源缓存是 `HyperionAssets/.cache/Sources`，可通过 `--cache` 改到其他未跟踪目录。`Metadata/Sources.json` 记录固定来源/哈希、生成器、完整自创配方和发布根。`--source-root` 与 `--source-id` 必须一起提供；逻辑来源 ID 在不同机器上保持不变。来源记录是可选的，运行时只需要原生资产及文本 Shader。导入映射从现存 hasset 的可选 Import.OutputIds 重建；依赖存放在可见类型目录中，每个 ID 只有一个当前文件，历史由 Git/LFS 管理。

`--source-id` 是位置无关的逻辑名称，例如 `custom`，不能包含盘符、反斜杠或绝对目录。两项都省略时，导入器使用目标资产 ID 作为逻辑来源空间；同一目标可从另一机器重新选择源文件而保持身份。源文件与资产仓库的本地位置不写入 hasset。跨卷位置提示无法表达为相对路径时，应配置内容挂载或共同的源目录布局。

已有虚拟原生引用在导入时经过依赖图和身份校验后保留，并清除作者引用的 revision 约束。天空的通用 GGX Smith BRDF LUT 位于 `/Engine/Textures/EnvironmentBrdf.hasset`，不会随每个天空重复发布。引擎 LUT 可显式重建：

```powershell
./out/build/release/bin/hyperion_asset_tool.exe --mounts ContentMounts.json --authoring build-brdf /Engine/Textures/EnvironmentBrdf.hasset
```

HyperionAssets 的 hasset 使用 LFS，文本 Shader 和元数据使用普通 Git。第三方许可及固定来源在 `Metadata` 中按资产保留，根 LICENSE 不替代它们。发布兼容性应记录资产仓库与引擎的配套版本；未提交验收阶段以 OpenSpec change 和验证记录标识。

## 验证与测试

独立 IO/native/shader 测试生成小型夹具，不依赖示例仓库。Shader/RHI 测试用测试代码生成专用 Shader，避免将演示 Shader 留在引擎 Content。原始 HDR/EXR 与自创场景的导入集成测试明确使用 `sample_source_fixtures`：先准备 HyperionAssets 的源缓存，再通过 `PrepareTestSources.py` 生成 `out/fixtures/Sources`。Viewer 示例和桌面集成测试需要挂载已发布 Game 内容。

迁移验收检查：完整 native 依赖图、无旧目录读取、固定输入截图、天空切换与场景保存重载、不同挂载位置、Shader include/cache、LFS 指针诊断和无变化重导入。历史性能/审计文档保留当时路径作为证据；当前资源布局以本页为准。
