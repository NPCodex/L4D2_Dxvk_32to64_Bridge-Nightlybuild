# L4N-Necola 整合包

本构建包含 DXVK-GPLALL、L4N 2.53.0、necola 1.4.1 兼容扩展和 Bridge 桥接工具，安装方式见 [README](../README.md)。necola 通过独立 L4N 插件加载，L4N 与 necola 原模块保持原样，没有把全部 necola 功能重写进 `left4neko.dll`。

**L4N / Left4Neko 原作者为 Starfelll，原文件署名为 @Starfelll。** 本仓库负责组合交付和 Bridge 构建，不将 L4N 标为本项目或 Codex 原创。原 `readme_l4n.txt` 完整随包保留，原文和 [L4N 归属说明](../licenses/L4N-NOTICE.txt) 也收录于 `THIRD-PARTY-NOTICES.txt`。原说明涉及普通 DXVK 的安装路径时，以本整合包的 Bridge 路径为准。

## 完整交付与来源

- 维护者提供 `L4N_v2.53.0.7z`，13,262,958 字节，SHA-256 为 `164879044ca55907c748ebcabcefb517ab78029b2f9ca190f1d227c5323dfa1f`。原包共有 77 个文件，除下述配置模板处理外，其余 76 个文件全部按原路径、原字节交付，包括新增的 `bin/left4neko_server.dll`。
- 完整保留 `bin/neko/`，包括 SDK、音频和模型转换脚本、模型、`other_tools.7z`；根目录 `to_l4n_survivor.exe`、`left4dead2 no-insecure.bat` 和原始说明也全部随包。原包中的嵌套归档保持原样，未展开或执行其中的工具。
- 唯一例外是 `left4dead2/neko/config_template.vdf`：以 2.52.1 模板、原有用户 `config.vdf` 和 2.53.0 模板完成三方合并，更新实际 `config.vdf`，模板不单独附带。本次合入 VPK Overlay 说明与注释状态的 `-nekook` 示例，移除原作者已删除的 `generate_addoninfo` 项；保留用户的 `index_buffer_size=131072`、`-novid`、`-l4n_use_neko_engine_post` 及其余设置。
- 继续保留维护者此前提供的 `dxvk.conf` 与 `left4dead2/shaders/fxc/neko_floattoscreen_ps20b.vcs` 原字节。这些是整合预设，不代表 L4N 原版默认配置。
- 原有 79 个 L4N 文件（共 23,748,123 字节）与已经合并的用户预设继续保留。此前模板合并删除 `generate_addoninfo` 的原始冲突结果与解决收据另存。
- 兼容扩展来源为已完成的 `L4N-Necola.zip`，17,006,203 字节，SHA-256 为 `b4be6642cab06526238bd1147872af1a6aa54ec5aa22d67264d1ae65c562896d`。只取其中 15 个运行文件：`bin/neko/plugins/l4n_necola_bridge.dll`、`bin/neko/necola/necola.dll`、根目录 `kpatch.ini`、`necola/FeatureConfig.json`、`necola/weaponParticles.json` 及 `left4dead2/addons/` 内 10 个音频库 VPK。
- `runtime/l4n/manifest.json` 分别记录 L4N 原包、模板合并和 necola 扩展的来源与 SHA-256，最终组合载荷为 94 个文件、32,711,312 字节。打包前逐项验证，清单只用于构建，不放入玩家 ZIP。

完整玩家 ZIP 共 104 个文件：94 个 L4N/Necola 载荷文件、7 个 Bridge/后端/配置/设置插件文件、ThinFlex DLL、`README.txt` 和合并的 `THIRD-PARTY-NOTICES.txt`。不加入合并项目的 `README_merge.md`、`package-manifest.json`、开发与验证文档、安装/回滚脚本、私有日志和转储，也不加入 necola 独立启动器或重复外层原包。两份 JSON 是运行配置，必须随扩展交付；原 L4N 工具、SDK、原始说明和必要归属仍完整保留。

## v1.5.17 附件更新

按维护者要求，本次 L4N-Necola 更新替换现有 v1.5.17 的完整 ZIP 和校验文件，不新增版本号或 Release。正式构建使用已批准的 NVIDIA 源码与现有 Bridge 补丁，执行三端 MSVC 编译和必需原生回归。原 Release、tag 名称及附件文件名保留以保持下载链接连续，当前构建身份以 Release 正文、源码收据和实际附件校验值为准。

原作者的 2.53.0 更新记录包括：`l4n_bonesetup_patch` 能接管本地服务端动画计算，修复右键攻击不能打断 `l4n_lookat`，修复 `l4n_show_addon_load_order` 输出时崩溃，并移除 `l4n_force_dummy_addoninfo` 与 `generate_addoninfo`。这些是原作者的更新说明；整合包未额外启用实验性骨骼计算。

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
