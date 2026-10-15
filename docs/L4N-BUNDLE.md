# L4N 整合包

本构建包含 DXVK-GPLALL、L4N 2.52.1 和 Bridge 桥接工具，按 [README 的下载与安装步骤](../README.md) 解压覆盖即可安装。不要与其他同类项目或整合包混装；先移除 `-vulkan`，备份移走游戏根目录的 `d3d9.dll`，保留本包的 `bin/d3d9.dll`。

**L4N / Left4Neko 原作者为 Starfelll，原文件署名为 @Starfelll。** 本仓库负责组合交付和 Bridge 构建，不将 L4N 标为本项目或 Codex 原创。原 `readme_l4n.txt` 完整随包保留，原文和 [L4N 归属说明](../licenses/L4N-NOTICE.txt) 也收录于 `THIRD-PARTY-NOTICES.txt`。原说明涉及普通 DXVK 的安装路径时，以本整合包的 Bridge 路径为准。

## 完整交付与来源

- 维护者提供 `L4N_v2.52.1.7z`，SHA-256 为 `1647f8ae44ff77cfdbe22f594a49535d0ba0ac0d91a92e988865523c845f49f9`。原包共有 76 个文件，除下述配置模板处理外，其余 75 个文件全部按原路径、原字节交付。
- 完整保留 `bin/neko/`，包括 SDK、音频和模型转换脚本、模型、`other_tools.7z`；根目录 `to_l4n_survivor.exe`、`left4dead2 no-insecure.bat` 和原始说明也全部随包。原包中的嵌套归档保持原样，未展开或执行其中的工具。
- 唯一例外是 `left4dead2/neko/config_template.vdf`：以旧模板、原有用户 `config.vdf` 和新版模板完成三方合并，更新实际 `config.vdf`，模板不单独附带。本次新增 `!force` VPK 目录说明，没有新增活动配置键；保留用户的 `index_buffer_size=131072`、`-novid`、`-l4n_use_neko_engine_post` 及其余设置。
- 继续保留维护者此前提供的 `dxvk.conf` 与 `left4dead2/shaders/fxc/neko_floattoscreen_ps20b.vcs` 原字节。这些是整合预设，不代表 L4N 原版默认配置。
- `runtime/l4n/manifest.json` 同时记录全部原包文件、最终 78 个 L4N 文件及模板合并的输入/输出 SHA-256。最终 L4N 文件共 23,396,342 字节，打包前逐项验证，不能按扩展名或旧精简清单遗漏原包内容。

完整玩家 ZIP 共 88 个文件：78 个 L4N 文件、7 个 Bridge/后端/配置/设置插件文件、ThinFlex DLL、`README.txt` 和合并的 `THIRD-PARTY-NOTICES.txt`。不加入原包之外的玩家私有日志、转储、旧 Bridge、重复外层原包或本仓库诊断材料。

## v1.5.16 附件更新

按维护者要求，L4N 2.52.1 更新替换现有 v1.5.16 的完整 ZIP 和校验文件，不新增版本号或 Release。Bridge Client、x64/x86 Host、设置插件、GPLALL 后端和 ThinFlex DLL 沿用已验证的正式 v1.5.16 二进制，逐字节核对。原构建标签和二进制指纹保留，Release 另记本次重新打包提交、配方和 ZIP 校验值。

原作者将新的骨骼计算接管标为实验性功能，相关限制见原始说明；整合包未额外启用这些选项。本次没有新增游戏或 FPS 测试，归档及字节校验不代表游戏性能或完整兼容性验证。

## 配置与安装

首次安装可使用包内 `dxvk.conf` 和已合并的 `left4dead2/neko/config.vdf`。升级时先备份个人配置，在临时解压目录对照合并后再覆盖。自定义配置不要直接丢弃。

`dxvk.conf` 保留维护者提供的 `dxvk.bridgeMappedChunkSize`、`dxvk.bridgeMemoryDiagnostics` 等键；它们属于可选 mem1 后端的扩展，不表示默认 GPLALL 实现了这些键。默认 Host 为 x64。

Bridge 设置插件直接位于 `bin/neko/plugins/L4D2BridgePlugin.dll`，无需额外复制。加载插件不修改配置或执行 GC；菜单操作由玩家主动选择。不需要菜单时，退出游戏后移走该 DLL。详见 [菜单说明](L4N-BRIDGE-CONTROLS.md)。

安装前仍须按 README 核对 `studiorender.dll` 的适用版本并保留最初的原始备份。
