# 同版本修订前的发布记录

以下保存 v1.5.17 原始 NVIDIA 更新的发布说明。它记录原发布范围；本次 ReShade 修订的新增内容与验证见 [当前发布说明](RELEASE-NOTES.md) 和 [输入修复记录](RESHADE-INPUT-20261016.md)，原构建通过结论不能替代修订后的 CI。

# v1.5.17

- 更新 NVIDIA Bridge 构建基准至 `029602f83a2fdec8f4d4419e1967bf231fb8f0b5`，加入运行时缺失时回退 x64、Remix 初始化状态 IPC、Host 初始化状态传播和 Bridge API 初始化顺序适配。x86 Client、x86 Host 与 x64 Host 必须成套更新。
- 适配新初始化流程时保留游戏根目录与 `bin` 的运行时定位，并使用原生 Unicode 路径读取配置和创建目录，避免中文或其他非系统编码字符在转换中丢失。
- NVIDIA Remix 渲染器源码中的 Windows/GFN/Linux 全局驱动版本门槛默认关闭，不保留 `610.47` 或采用 `616.92` 下限；功能能力检查保留。该 `src/dxvk` 渲染器未编入本包，随包后端仍为独立固定的 GPLALL，因此这项源码修改不代表玩家包新增了旧驱动兼容性或 RTX 功能。

发布检查覆盖 Python 回归、全新 `029602f` 源码树、三端 MSVC 14.29 编译、原生回归、最终源码收据、附件校验和 Latest 身份核对。本次没有新增游戏、FPS、画质、长期稳定性或首次启动/开机冷启动闪退验证，不能据此宣称该崩溃已修复。

[安装说明](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/blob/main/README.md)

## 2026-10-18 L4N 2.53.0 修订说明（历史）


- 本次同版本附件更新将完整 L4N 升级为 2.53.0，包含新增的 `bin/left4neko_server.dll`；原包 77 个文件中 76 个保持原字节，完整玩家包为 89 文件。
- 唯一模板例外：新版 `config_template.vdf` 三方合并进 `config.vdf`，保留用户预设，合入 VPK Overlay 说明及注释示例，移除原作者已删除的 `generate_addoninfo` 项。
- 据原作者 Starfelll（@Starfelll）的更新说明，2.53.0 扩展本地服务端动画计算接管，修复右键攻击不能打断 `l4n_lookat` 和 `l4n_show_addon_load_order` 输出崩溃，并移除 `l4n_force_dummy_addoninfo`。整合包未额外启用实验性骨骼计算。

验证范围：原包完整性、逐文件身份、配置合并和完整包校验；发布门禁要求三端 MSVC 14.29 编译、必需原生回归、最终源码收据与发布附件核验通过。本次没有新增游戏、FPS、画质或长期稳定性验收，不将原作者的更新说明视为本项目实机验证。此前独立的 Necola 退出与 Special K 随机崩溃问题不由本次更新判定为已修复。

[整合来源与配置说明](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/blob/main/docs/L4N-BUNDLE.md) · [安装说明](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/blob/main/README.md)
