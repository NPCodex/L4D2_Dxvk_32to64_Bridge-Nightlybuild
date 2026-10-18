# L4N 整合包

本构建包含 DXVK-GPLALL、L4N 2.53.0 和 Bridge 桥接工具，安装方式见 [README](../README.md)。

**L4N / Left4Neko 原作者为 Starfelll，原文件署名为 @Starfelll。** 本仓库负责组合交付和 Bridge 构建，不将 L4N 标为本项目或 Codex 原创。原 `readme_l4n.txt` 完整随包保留，原文和 [L4N 归属说明](../licenses/L4N-NOTICE.txt) 也收录于 `THIRD-PARTY-NOTICES.txt`。原说明涉及普通 DXVK 的安装路径时，以本整合包的 Bridge 路径为准。

## 完整交付与来源

- 维护者提供 `L4N_v2.53.0.7z`，13,262,958 字节，SHA-256 为 `164879044ca55907c748ebcabcefb517ab78029b2f9ca190f1d227c5323dfa1f`。原包共有 77 个文件，除下述配置模板处理外，其余 76 个文件全部按原路径、原字节交付，包括新增的 `bin/left4neko_server.dll`。
- 完整保留 `bin/neko/`，包括 SDK、音频和模型转换脚本、模型、`other_tools.7z`；根目录 `to_l4n_survivor.exe`、`left4dead2 no-insecure.bat` 和原始说明也全部随包。原包中的嵌套归档保持原样，未展开或执行其中的工具。
- 唯一例外是 `left4dead2/neko/config_template.vdf`：以 2.52.1 模板、原有用户 `config.vdf` 和 2.53.0 模板完成三方合并，更新实际 `config.vdf`，模板不单独附带。本次合入 VPK Overlay 说明与注释状态的 `-nekook` 示例，移除原作者已删除的 `generate_addoninfo` 项；保留用户的 `index_buffer_size=131072`、`-novid`、`-l4n_use_neko_engine_post` 及其余设置。
- 继续保留维护者此前提供的 `dxvk.conf` 与 `left4dead2/shaders/fxc/neko_floattoscreen_ps20b.vcs` 原字节。这些是整合预设，不代表 L4N 原版默认配置。
- `runtime/l4n/manifest.json` 同时记录全部原包文件、最终 79 个 L4N 文件（共 23,748,123 字节）及模板合并的输入/输出 SHA-256。旧预设的缩进与上游删除 `generate_addoninfo` 发生一处合并冲突，按新版删除该废弃项，原始冲突结果与解决收据另存。打包前逐项验证全部文件，不能按扩展名或旧精简清单遗漏原包内容。

完整玩家 ZIP 共 89 个文件：79 个 L4N 文件、7 个 Bridge/后端/配置/设置插件文件、ThinFlex DLL、`README.txt` 和合并的 `THIRD-PARTY-NOTICES.txt`。不加入原包之外的玩家私有日志、转储、旧 Bridge、重复外层原包或本仓库诊断材料。

## v1.5.17 附件更新

按维护者要求，L4N 2.53.0 更新替换现有 v1.5.17 的完整 ZIP 和校验文件，不新增版本号或 Release。正式构建使用已批准的 NVIDIA 源码与现有 Bridge 补丁，执行三端 MSVC 编译和必需原生回归。原 Release、tag 名称及附件文件名保留以保持下载链接连续，当前构建身份以 Release 正文、源码收据和实际附件校验值为准。

原作者的 2.53.0 更新记录包括：`l4n_bonesetup_patch` 能接管本地服务端动画计算，修复右键攻击不能打断 `l4n_lookat`，修复 `l4n_show_addon_load_order` 输出时崩溃，并移除 `l4n_force_dummy_addoninfo` 与 `generate_addoninfo`。这些是原作者的更新说明；整合包未额外启用实验性骨骼计算。

归档、配置合并、打包校验和 Bridge 原生回归不代表 L4N 2.53.0 的游戏地图、FPS、画质或长期稳定性验收，也不证明此前独立的 Necola 退出或 Special K 随机崩溃问题已修复。

## 配置与安装

首次安装可使用包内 `dxvk.conf` 和已合并的 `left4dead2/neko/config.vdf`。升级时先备份个人配置，在临时解压目录对照合并后再覆盖。自定义配置不要直接丢弃。

`dxvk.conf` 保留维护者提供的 `dxvk.bridgeMappedChunkSize`、`dxvk.bridgeMemoryDiagnostics` 等键；它们属于可选 mem1 后端的扩展，不表示默认 GPLALL 实现了这些键。默认 Host 为 x64。

Bridge 设置插件直接位于 `bin/neko/plugins/L4D2BridgePlugin.dll`，无需额外复制。加载插件不修改配置或执行 GC；菜单操作由玩家主动选择。不需要菜单时，退出游戏后移走该 DLL。详见 [菜单说明](L4N-BRIDGE-CONTROLS.md)。

安装前仍须按 README 核对 `studiorender.dll` 的适用版本并保留最初的原始备份。
