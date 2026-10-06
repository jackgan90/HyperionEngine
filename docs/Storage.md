# 运行时存储与开发输出

Content 保存发布的引擎/项目资源；用户设置、会话状态和可重建缓存不注册为 Content mount。`Runtime/IO` 定义应用路径，`Runtime/Config::FStorageSettings` 负责路径设置、revision 和持久化，`Runtime/DerivedDataCache` 提供与 Shader 无关的本地派生数据存储。

## Editor 路径

默认用户根为 Windows LocalAppData 下的 `Hyperion`，缓存为用户根下的 `Cache`。应用目录为 `<UserDataRoot>/Apps/Editor/<Profile>`，默认 Profile 为 `Default`。

| 目录 | 内容 | 处理方式 |
|---|---|---|
| `Config` | `Preferences.ini`、`UiScale.ini`、`RenderSettings.json` | 保留；用户根变更时复制缺失文件 |
| `State` | `Layout.ini` 和资产窗口布局 | 保留；用户根变更时复制缺失文件 |
| `Logs/Session-*` | 本次启动日志 | 不迁移，不自动删除 |
| `Captures` | RenderDoc 输出 | 不迁移，不自动删除 |
| `Data` | 应用持久数据预留 | 不属于自动清理范围 |
| `<CacheRoot>/DerivedData` | 完整性校验的 shader 派生数据 | 可删除，重新编译恢复 |

现有 `--layout`、`--editor-preferences`、`--ui-preferences`、`--render-settings` 和显式截图/benchmark 输出参数仍有效。安装目录中可提供 `Config/Editor/Preferences.ini`、`RenderSettings.json`；仅在对应用户文件缺失时采用整份默认文件，随后仍由原有领域加载器验证。没有配置文件时使用代码默认值。

```powershell
# 明确指定所有启动存储；适用于需要离开系统盘的开发环境。
out/build/debug/bin/hyperion_editor.exe --storage-settings F:/HyperionLocal/Storage/Editor.json --user-data-root F:/HyperionLocal/User --cache-root F:/HyperionLocal/Cache

# 希望以后通过 Editor 调整根目录：保留稳定 locator，去掉固定根的启动覆盖。
out/build/debug/bin/hyperion_editor.exe --storage-settings F:/HyperionLocal/Storage/Editor.json
```

命令行高于环境变量，环境变量高于已保存选择；对应变量为 `HYP_STORAGE_SETTINGS`、`HYP_USER_DATA_ROOT`、`HYP_CACHE_ROOT`、`HYP_STORAGE_PROFILE`。默认 locator 为 LocalAppData 下的 `Hyperion/Storage/Editor/<Profile>.json`；显式 user root 且未指定 locator 时，locator 位于该根的 `Storage` 下。独立的 `--storage-settings` 是 GUI 管理根目录时的稳定入口，搬动用户根不搬动它。

Editor 的 automation 发现记录位于 locator 同级 `Discovery/<SID>`，保留用户 ACL；因此保存待重启根目录也不会中途改变发现位置。CLI 接受同样的 storage 参数/环境变量；附着自定义实例时传入相同 locator。`HYP_DISCOVERY_ROOT` 可显式覆盖双方的发现命名空间。

**Edit > Editor preference > Storage locations** 显示活动目录、保存值、locator、启动覆盖和重启状态。保存时验证绝对路径、可写性及与应用数据/Content 的重叠，再通过写租约和原子替换持久化。进程内目录固定；重启后才应用选择。空字段恢复默认。CLI/environment 固定的根仍然优先，不会被 GUI 保存隐式移除。失败保留原选择；其他进程改变设置后需 Reload 再编辑。

Automation 的 `application.storage.get` 与 `application.storage.set` 调用同一服务。先 get，再把返回的 `revision` 和 `roots: {userDataRoot, cacheRoot}` 提交给 set；返回包含 `active`、`saved`、`next`、`restartRequired`。过期更新返回 `stale_revision`，无效参数返回 `invalid_arguments`，保存失败返回 `save_failed`。服务由 `storage` 插件提供，`automation-storage` 注册反射操作；独立 CLI 或禁用 provider 时可发现操作，但调用返回 unavailable。此设置不属于场景 undo/redo。

用户根变更在下一次启动复制旧应用的 Config/State，目标已有文件优先，拒绝链接，不删除源。开发构建首次启动仅导入已知旧 `out/editor` 配置和布局，完成标记阻止重复导入；`--isolated-storage` / `HYP_ISOLATED_STORAGE=1` 和验收运行跳过旧配置导入。日志、截图、性能数据和 review 证据继续留在旧位置且保持 Git 忽略，不批量迁移或纳入仓库。

## 派生数据与部署

Shader 继续按逻辑源路径、源内容、编译选项与工具链版本构造语义键。DDC 使用命名空间和格式版本目录，记录校验 key、长度、SHA-256；损坏、缺失和存储失败会回退编译，发布失败不丢弃编译结果。维护仅处理自身命名空间的已识别记录，默认按 30 天年龄与观测到的 8 GiB 数据预算清理，每次最多扫描 4096 个目录项；这是有界的尽力维护，不是跨进程严格磁盘配额。缓存命中与逐帧路径不写日志。

应用优先查找 exe 相邻或上一级的 `Content`，也可通过 `--engine-content` 明确选择。`HYP_DEVELOPMENT_CONTENT=ON` 允许源码 Content 回退；部署构建应设为 `OFF`，同时携带 `Content` 和运行时 DLL。安装目录只读时，配置和缓存仍写入独立用户路径；发布的 `.hasset` 与 Shader 源是安装资源，不是可删除的 DDC。

## 构建、下载与测试

`tools/DevelopmentPaths.py` 是开发路径的共同入口。`HYP_OUT_ROOT` 默认为仓库 `out`，构建产物位于 `build`，测试输出位于 `tests`。`HYP_TOOL_CACHE` 默认为 LocalAppData 下的 `Hyperion/ToolCache`，保存 `Downloads`、按完整 lock 身份区分的 `Dependencies` 及 `SourceAssets/EngineSky`。它与运行时缓存互不依赖。Game 的来源缓存仍由所选 HyperionAssets 的 `.cache/Sources` 管理。

```powershell
.\tools\Build.ps1 -Preset debug -OutRoot F:/HyperionBuild -ToolCache F:/HyperionTools
.\tools\GenerateSolution.ps1 -OutRoot F:/HyperionBuild -ToolCache F:/HyperionTools
python tools/Bootstrap.py --cache-root F:/HyperionTools --legacy-root out --offline
```

Bootstrap 只使用 `dependencies.lock.json` 的固定条目，验证 archive SHA-256 后暂存解包并原子发布；同一依赖 bundle 使用发布锁。旧 `out/downloads` 只作为显式校验导入来源，旧 `out/deps` 不作为默认回退。保留树可用 `Build.ps1 -Dependencies <path>` 或 CMake `HYP_DEPS` 显式选择；直接 CMake 可指定 `-B`、`HYP_OUTPUT_ROOT` 和 `HYP_TOOL_CACHE`。Visual Studio 入口也接受 `-BuildDirectory`。固定来源配方校验的 fixture generator 保持原始文件内容，CTest 显式传入新输出目录。

验收入口创建 `tests/Runs/run-*`，子进程继承用户根、缓存、bootstrap、`HYP_DISCOVERY_ROOT` 和隔离标志；发现目录保留原有 same-user ACL 和协议。普通单元测试的相对输出在构建目录内。保留历史测试证据与缓存不会影响新运行目录；不保证删除工具来源缓存后还能离线重建。
