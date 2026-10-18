# L4D2 Bridge Nightly

> **本构建已包含 DXVK（GPLALL）、L4N 和 Bridge 桥接工具，一体整合，解压覆盖即可一键安装，无需另下这三项组件。请勿与其他同类项目或整合包混合安装。**
>
> **安装前移除 `-vulkan` 启动参数，并备份移走游戏根目录的 `d3d9.dll`；保留本包的 `bin/d3d9.dll`。** L4N / Left4Neko 原作者为 **Starfelll（@Starfelll）**，感谢原作者的工作。
>
> **ReShade 安装：选择游戏目录下的 `bin/.l4d2bridge/L4D2Bridge64.exe`，渲染 API 选择 Vulkan；不要选择游戏根目录的 `left4dead2.exe`。** 本包默认由 x64 Bridge Host 执行 Vulkan 渲染。这里的 Vulkan 是 ReShade 安装程序中的 API 选项，不是游戏启动参数；游戏的 `-vulkan` 仍需移除。

**简体中文** | [English](README.en.md)

## 说明

**v1.5.17 本次修订将完整 L4N 更新为 2.53.0，包含新增的 `bin/left4neko_server.dll`，并合并新版配置模板。** 原作者更新说明涉及本地服务端骨骼动画计算、`l4n_lookat` 打断和模组加载顺序输出修复；整合包未额外启用实验性骨骼计算。本次范围与来源见 [整合包说明](docs/L4N-BUNDLE.md)。

**[前往 Releases 下载完整包](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/releases)**。每次发布只提供一个 `l4d2-bridge-*.zip`，另附 `.sha256` 校验文件。现有 v1.5.17 包含 **完整 L4N 2.53.0、DXVK-GPLALL、Bridge、`dxvk.conf`、L4N 的 `left4dead2/neko/config.vdf`** 和修复后的 `bin/studiorender.dll`。L4N 的 `bin/neko`、SDK、全部转换工具、内嵌 `other_tools.7z`、`to_l4n_survivor.exe`、`left4dead2 no-insecure.bat` 与原始 `readme_l4n.txt` 均完整随包。唯一例外：新版 `config_template.vdf` 的修改已合并进 `config.vdf`，模板不单独附带。共 89 文件，来源与合并细节见 [整合包说明](docs/L4N-BUNDLE.md)。

| 游戏 `bin/studiorender.dll` | SHA-256 |
|---|---|
| 支持的原版 | `3f5f5b0f539e8ad22bcfc4381be41571257c0c29e8061057682f9b8525ca7b85` |
| 包内修复版 | `03964dedcf8b7f4ebde24cd3d0738873d37c075a7a9b313dad001bb937f9d1b6` |

ThinFlex 修复已收到用户复测有效反馈，仍限定于以上精确版本。游戏更新后重新核对；直接复制文件不会自动检查目标版本。仓库的 [引擎修复清单](runtime/engine/studiorender.manifest.json) 记录改动，包内 `THIRD-PARTY-NOTICES.txt` 保留引擎归属；修改后的 DLL 原数字签名失效。详细安装和恢复步骤见 [ThinFlex 说明](docs/THINFLEX-TEST-README.txt)。

如需 `-vulkan`，把客户端 `d3d9.dll` 改名为 `dxvk_d3d9.dll`，文件仍留在游戏 `bin`。从旧版切换到默认加载方式时，先备份旧 `bin/dxvk_d3d9.dll` 并移除 `-vulkan`。两种方式共用 `bin/.l4d2bridge`。

完整包同时包含 x64/x86 Host 与对应 GPLALL 后端，**默认仍为 x64**。切换 Host 可修改 `client.testX86Server`（`False` 为 x64，`True` 为 x86），保持 `forceX64Server=True`，退出整个游戏后重新启动。

L4N 本体和 Bridge 常用设置菜单插件一同随包安装，插件直接位于 `bin/neko/plugins/L4D2BridgePlugin.dll`，启动游戏后可在 L4N 菜单中使用。安装插件本身不会更改配置或执行 GC；这些操作需在菜单中主动选择。不需要菜单时，退出游戏后移走或删除该 DLL 即可，Bridge 本体仍可使用。详见 [L4N 设置说明](docs/L4N-BRIDGE-CONTROLS.md)。L4N 原始说明与作者署名原文收录于包内 `THIRD-PARTY-NOTICES.txt`，另见 [Starfelll 归属说明](licenses/L4N-NOTICE.txt)。

新安装配置包含上游的 learned-aggressive 内存策略，并默认关闭日常 memory/crash/data 诊断；已有用户升级继续保留自己的配置，新功能的键与开销见仓库 [配置说明](docs/CONFIGURATION.md)。IPC 批次默认 `client.ipcBatchSize=8`；静态映射默认 `client.vbibMappedShadow=False`。已有显式配置优先，修改后需完整重启游戏与 Host。

当前使用 **完整 IPC v2（ABI 0x00020004）**，`bin/d3d9.dll`、`L4D2Bridge32.exe` 和 `L4D2Bridge64.exe` 必须同批更新，不能混用旧版。回退 Bridge 时同时恢复配对的客户端和 Host；回退 ThinFlex 时恢复本机保存的原始 `studiorender.dll`。卸载时移除本包安装的文件并恢复备份。

本次范围见 [L4N 整合包说明](docs/L4N-BUNDLE.md)；此前 [ReShade 输入修复](docs/RESHADE-INPUT-20261016.md) 的机制与验证记录继续保留；v1.5.17 原始 NVIDIA 更新说明保留于 [历史发布记录](docs/RELEASE-HISTORY.md)，源码采用范围见 [NVIDIA 基准更新](docs/NVIDIA-UPSTREAM-UPDATES.md)。此前 [设备销毁清理修复](docs/DEVICE-DESTROY-20261015.md)、[GPU 与 IPC 调整](docs/GPU-IPC-20261013.md)、[Steam 选择性更新](docs/UPSTREAM-STEAM-20261013.md) 和 [v1.5.0 整合与验证说明](docs/UPSTREAM-45-INTEGRATION.md) 的历史记录继续保留。[历史 v1.2.3 优化对照](docs/HOT-COMMAND-PACKETS.md) 记录本机 GPLALL + x64 Host 的帧率结果；[此前 IPC 更新对照](docs/UPSTREAM-1.2.2-INTEGRATION.md) 记录升级至 v1.2.2 时的取舍。[手电专项记录](docs/FLASHLIGHT-PERFORMANCE-2026-10-10.md) 和 [早期性能实测](docs/PERFORMANCE-2026-10-10.md) 属于此前版本；更多验证步骤见 [游戏验收与性能基线](docs/GAME-VALIDATION.md)。

基于 [NVIDIA dxvk-remix Bridge](https://github.com/NVIDIAGameWorks/dxvk-remix)，沿用 [L4D2 原项目](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge) 的补丁，自动构建适用于 32 位《求生之路 2》的 x86 客户端与 x86/x64 Host。

## 与上游的区别

- 仅构建 Bridge，不构建或分发 RTX Remix 渲染器。
- 应用 L4D2 补丁：调整 Host 和后端加载路径，加入表面／缓冲区影子内存管理及诊断日志，并使用游戏专用配置。补丁见 [patches/l4d2-bridge.patch](patches/l4d2-bridge.patch)。
- 默认使用固定源码构建的 **DXVK-GPLALL 2.6.8-2 x64** 后端，源码、三项 GPL 修复和 MSAA 循环提示变换见 [config/backend.json](config/backend.json) 与 [四项调查与验证记录](docs/OPTIMIZATION-37-20261013.md)。
- 本仓库增加上游检查、自动编译、测试、Nightly 发布和精简安装包规则。
- 修复三维纹理字节步长及上传偏移，避免颜色校正查色表损坏造成的偏色。来源与复测步骤见 [偏色修复说明](docs/VOLUME-TEXTURE-COLOR-FIX.md)。
- 完整同步原项目 **1.3.0 / `f6a059b`**，包括最新双向 IPC 预留/消费、回绕与错误传播修复，以及此前 PageBlock 保留/回收/恢复、手动 GC、Reset 状态机、Presenter/输入路径、诊断分离、x86 Host 与 L4N 常用设置。源码、配置片段、测试和可选 DXVK 内存实验均保留，实验后端不替换默认 GPLALL。同步基准与本地兼容差异见 [更新跟踪](docs/ORIGINAL-PROJECT-UPDATES.md)。
- 将创建失败清理扩展到三维/立方体纹理、顶点/索引缓冲区及独立表面，释放客户端对象并清空输出；响应超时仍保留有序的服务器清理。
- 加固缓冲区锁边界和三维纹理临时内存管理，优化连续纹理复制与队列读取；验证方法见 [资源清理与传输优化](docs/RUNTIME-RELIABILITY.md)。

`bin/d3d9.dll` 是 32 位 Bridge 客户端；`.l4d2bridge/d3d9vk_x64.dll` 和 `d3d9vk_x86.dll` 分别供对应位数的 Host 使用。

## 自动与手动构建

项目版本从 **v1.0** 起，重大更新递增次版本号，小改动递增补丁号；当前版本见 [VERSION](VERSION)，维护者设置见 [版本规则](docs/VERSIONING.md)。

每小时第 23 分钟检查 NVIDIA 构建源的默认分支（当前为 `main`）。L4D2 原项目功能同步另记于 [更新跟踪](docs/ORIGINAL-PROJECT-UPDATES.md)。Codex 每小时审查新提交，后续生产更新先汇报、再由维护者决定；Actions 默认只构建 `config/nvidia-upstream.json` 中已批准的提交，相同源码与配方已发布时跳过。详见 [NVIDIA 更新记录](docs/NVIDIA-UPSTREAM-UPDATES.md)。GitHub 调度可能延迟。

在 **Actions → Build latest upstream Bridge → Run workflow** 中：

- `upstream_commit` 留空时观察最新 HEAD、构建已批准提交；填写完整 40 位 SHA 可显式指定已授权的提交。
- 勾选 `force_rebuild` 可强制编译，默认创建新的独立版本并保留已有附件；仅在维护者明确授权同版本修订、且发布目标已固定时替换指定原 Release。默认关闭。
- 勾选 `validation_only` 仅构建、测试和运行 A/B 基准，保存结果供检查，跳过发布。
- 手动勾选 `thinflex_test` 标记为完整上游同步与 ThinFlex 修复测试版（Pre-release，不设为 Latest）。所有构建均提供包含固定 ThinFlex 修复 DLL 的单一完整包；安装时先核对游戏版本。

去重同时检查上游提交和构建输入指纹；补丁、后端配置、脚本或测试更新会触发新构建。版本名为 `nightly-YYYYMMDD-上游短SHA-r输入指纹-b运行ID.重试号`，日期采用上游提交日期（UTC）。强制重建和重新运行默认产生独立版本，旧附件不覆盖。例外是维护者明确授权的同版本原位修订：固定原 Release/tag 身份，完成新源码的全部构建与验证后替换该 Release 的完整 ZIP 和 SHA，保留旧附件证据并记录新提交、配方及构建实例，不另建版本。Release 说明记录完整提交、输入指纹和构建实例；JSON 清单只用于构建校验，不放入玩家安装包。补丁冲突、编译或测试失败时不发布。

复用本地源码目录时，构建脚本核对完整补丁和暂存区，拒绝混入额外源码修改并保留现场。普通发布中断后重跑失败的发布作业，会分页查找尚未发布的草稿，校验已有附件的内容，只上传缺失附件；内容冲突时停止。明确授权的原位修订使用单独固定的替换目标与旧附件身份校验，不把普通发布重试视为覆盖授权。

## License

- 项目特有的新增与修改：**MIT**，见 [LICENSE](LICENSE)，保留 `yeyunyyds` 的原版权声明。
- NVIDIA Bridge：**MIT**，见 [licenses/Bridge-MIT.txt](licenses/Bridge-MIT.txt)。
- L4N / Left4Neko 2.53.0：原作者 **Starfelll（@Starfelll）**，原文件与说明保留作者归属，不属于本项目原创或根目录 MIT 授权，见 [L4N-NOTICE](licenses/L4N-NOTICE.txt)。
- DXVK／DXVK-GPLALL：随附 **zlib/libpng** 许可，见 [licenses/DXVK-LICENSE.txt](licenses/DXVK-LICENSE.txt) 和 [licenses/DXVK-GPLALL-LICENSE.txt](licenses/DXVK-GPLALL-LICENSE.txt)。
- Bridge 所含 Detours、Tracy 等依赖继续遵循各自许可，见 [licenses/Bridge-third-party.txt](licenses/Bridge-third-party.txt)。
- 包内修改版 `studiorender.dll` 来自维护者本机的匹配游戏文件，按用户要求应用限定 ThinFlex 修复。原引擎归 Valve，不属于根目录 MIT 授权；见 [引擎归属说明](licenses/Valve-engine-NOTICE.txt)。

根目录 MIT 许可不替代第三方许可。发布包保留版权及许可文件；完整归属见 [THIRD_PARTY.md](THIRD_PARTY.md)。L4D2 游戏本体不在本项目授权范围内。
