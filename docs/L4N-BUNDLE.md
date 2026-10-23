# L4N-Necola 整合包

本构建包含 DXVK-GPLALL、L4N 2.55.0、necola 1.4.1 兼容扩展和 Bridge 桥接工具，安装方式见 [README](../README.md)。necola 通过独立 L4N 插件加载，L4N 与 necola 原模块保持原样，没有把全部 necola 功能重写进 `left4neko.dll`。

**L4N / Left4Neko 原作者为 Starfelll，原文件署名为 @Starfelll。** 本仓库负责组合交付和 Bridge 构建，不将 L4N 标为本项目或 Codex 原创。原 `readme_l4n.txt` 完整随包保留，原文和 [L4N 归属说明](../licenses/L4N-NOTICE.txt) 也收录于 `THIRD-PARTY-NOTICES.txt`。原说明涉及普通 DXVK 的安装路径时，以本整合包的 Bridge 路径为准。

## 完整交付与来源

- 固定输入为 `NPCodex/l4n-necola` 提交 `ba7104eecc4073f08ab609f87ce05ff50852bae2` 内受版本管理的 `release-assets/L4N-Necola.zip`，17,662,400 字节，SHA-256 为 `041143bdd3df275af81ac71249fa01d41c73a3d7e6ae3e22e90879a4198c6a79`。实际 ZIP 字节与 Git blob、同提交的校验文件一致；不使用本地忽略目录中的重建包。
- ZIP 清单中来源为 `originals/L4N/` 的原始文件共 79 个，逐项与提交内原件核对。除配置模板外，78 个按原路径、原字节交付。本次没有读取或声称校验独立的 L4N 2.55.0 原始 7z。
- 完整保留 SDK、转换工具、模型、嵌套 `other_tools.7z`、L4N 启动器、原 `readme_l4n.txt` 和已有运行组件 `crash_dumps/crashpad_handler.exe`。本次新增的原件为 `bin/vphysics_jolt.dll` 和 `bin/neko/读我_readme.txt`；crashpad 运行程序不是玩家日志或转储。
- 唯一原件例外是 `left4dead2/neko/config_template.vdf`：将 2.53.0 到 2.55.0 的模板改动合并到既有用户 `config.vdf`，保留原设置，并按维护者要求在 `launch_options` 中加入一次 `-l4n_vphysics_jolt`。新版 VPK Overlay 分组示例与说明随模板合并，模板不单独附带。
- `dxvk.conf` 和 `left4dead2/shaders/fxc/neko_floattoscreen_ps20b.vcs` 保留原字节。`index_buffer_size=131072`、`-novid`、`-l4n_use_neko_engine_post` 等既有活动设置保留；这些属于用户预设，不是 L4N 原版默认值。
- 同一 ZIP 提供 15 个 necola 运行文件：兼容插件、原始 `necola.dll`、`kpatch.ini`、两份 JSON 和 10 个音频库。使用新版包内插件；原 necola DLL 及运行配置、音频内容分别核验。
- `runtime/l4n/manifest.json` 记录固定提交、实际 ZIP、原始文件子集、配置合并和兼容扩展的逐文件 SHA-256。最终载荷共 96 个文件；构建前逐项核验，清单不放入玩家 ZIP。

完整玩家 ZIP 共 106 个文件：96 个 L4N/Necola 载荷文件、7 个 Bridge/后端/配置/设置插件文件、ThinFlex DLL、`README.txt` 和合并的 `THIRD-PARTY-NOTICES.txt`。不加入合并项目的说明、清单、开发文档、安装/恢复脚本、独立 necola 启动器、私有日志或转储。两份 JSON 是运行配置；L4N 原始工具、SDK 和说明完整保留。

## v1.5.17 附件更新与 Jolt

本次更新替换现有 v1.5.17 的完整 ZIP 和校验文件，版本号、原 Release、tag 名称及附件文件名保持。NVIDIA Bridge 基线仍为已批准的 `cf83c0514d5690a2e54369ba24a6d044b989e6c8`；正式构建执行三端 MSVC 编译和必需原生回归。当前构建身份以 Release 正文、源码收据和实际附件校验值为准。

按原作者说明，2.54.0 引入实验性 Jolt 物理替代；2.55.0 增加 VPK Overlay 层文件夹分组、修复 witch 实体使用 `player_foot`，并增加地图加载多线程控制台变量 `l4n_loading_boost`。本包按维护者要求启用 Jolt 启动参数；`l4n_loading_boost` 仍保持原默认 0，骨骼计算也未额外开启。Jolt 参数与加载加速 ConVar 是不同功能。

来源项目的基础游戏记录使用原版 `vphysics.dll`，未启用 Jolt；本次也没有运行 Bridge+Jolt 的实机物理、布娃娃、联机、地图或性能测试。上述来源说明和离线检查不代表这些场景已经验收，不承诺 FPS、加载速度或画质提升。

## necola 功能与限制

检视、普通开镜、额外粒子清单和堆内存配置由 L4N 负责。扩展在启动前检查以下六个开关必须为 `false`；重新启用其中任意一项会拒绝加载整个 necola 扩展。

| 配置 | 必须关闭的键 |
|---|---|
| `kpatch.ini` 的 `[Necola]` | `openInspect`、`customParticleManifest`、`enableHeapSizeHook` |
| `kpatch.ini` 的 `[AdsSupport]` | `enableAdsSupport` |
| `necola/FeatureConfig.json` | `InspectInitiative.EnableInspect`、`AdsSupport.EnableAdsSupport` |

保留 necola 的弹药/血量/击杀写回、武器动画同步、每武器曳光、多模型和挂件协议，需要对应 MOD 资产。necola 专用四级 ADS、MIXED 和 Mode C 挂件瞄具关闭，未转换为 L4N 协议。原 DLL 部分共享 hook 仍存在；启动检查不持续拦截原 necola 菜单在当前进程中的配置修改。

10 个音频库改为 L4N 的 `l4n/scripts/sound` 路径加载，并保留原缓存及 base 脚本；九个增补脚本的 4050 处缺失闭括号按原 base 模板修复。这些库提供声音映射和缓存，未附带实际 WAV、PCF 或 necola 武器模型，不能据此声称相关音画效果已通过验收。

来源项目的离线与隔离游戏记录只覆盖其自身组合；本次包内含 DXVK Bridge，不将来源项目测试当作本组合的实机结果。归档、配置和打包校验及 Bridge 原生回归不代表此组合的地图、FPS、画质、长期稳定性或正常退出验收。原 `necola.dll` 未修改，也不证明此前独立的 Necola 退出或 Special K 随机崩溃问题已修复。

## 配置与安装

首次安装可使用包内 `dxvk.conf` 和已合并的 `left4dead2/neko/config.vdf`。升级时先备份个人配置，在临时解压目录对照合并后再覆盖。自定义配置不要直接丢弃。

已有 necola 配置时，同样先备份 `kpatch.ini`、`necola/FeatureConfig.json` 与 `necola/weaponParticles.json`，保留个人设置并确认上述六个开关关闭。玩家 ZIP 不带合并项目的安装脚本，直接复制不会自动合并配置。使用 L4N 启动器，不再运行 necola 独立启动器或重复加载其 DLL。

`dxvk.conf` 保留维护者提供的 `dxvk.bridgeMappedChunkSize`、`dxvk.bridgeMemoryDiagnostics` 等键；它们属于可选 mem1 后端的扩展，不表示默认 GPLALL 实现了这些键。默认 Host 为 x64。

Bridge 设置插件直接位于 `bin/neko/plugins/L4D2BridgePlugin.dll`，无需额外复制。加载插件不修改配置或执行 GC；菜单操作由玩家主动选择。不需要菜单时，退出游戏后移走该 DLL。详见 [菜单说明](L4N-BRIDGE-CONTROLS.md)。

necola 兼容插件位于 `bin/neko/plugins/l4n_necola_bridge.dll`，与 Bridge 设置插件并存；其原始载荷位于 `bin/neko/necola/necola.dll`。necola 的必要归属与修改说明合并到既有 `THIRD-PARTY-NOTICES.txt`，不以本项目 MIT 许可覆盖原组件。

安装前仍须按 README 核对 `studiorender.dll` 的适用版本并保留最初的原始备份。
