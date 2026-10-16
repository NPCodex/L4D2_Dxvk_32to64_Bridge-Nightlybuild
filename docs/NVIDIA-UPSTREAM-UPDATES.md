# NVIDIA 构建源码基准

NVIDIA dxvk-remix、L4D2 原项目和独立 GPLALL 后端是三个不同来源；本页仅记录 NVIDIA 源码采用。L4D2 原项目基准继续见 [原项目同步记录](ORIGINAL-PROJECT-UPDATES.md)。

## 2026-10-16：运行时初始化更新与全局驱动门槛移除

本次审阅并采用 NVIDIA `main` 的 [`029602f83a2fdec8f4d4419e1967bf231fb8f0b5`](https://github.com/NVIDIAGameWorks/dxvk-remix/commit/029602f83a2fdec8f4d4419e1967bf231fb8f0b5)，替代此前固定的 `d2a5033ec79303fffc33ca8e6595dd560162ff69`。范围包括运行时缺失时回退 x64、Remix 初始化状态 IPC、Host 初始化状态传播、Bridge API 初始化顺序及上游 RTX/稀疏渲染与粒子法线变更。Client 与 x86/x64 Host 必须使用同一组配套二进制；本次没有声称修复首次启动或开机冷启动闪退，也没有新增游戏/FPS/画质验收证据。

上游将 Windows 全局最低 NVIDIA 驱动提高到 `616.92`，本包明确排除这项提升。本分支同时移除 Windows 与 Linux 的全局驱动版本门槛，默认值为 `0`，因此不再因 `610.47`、`616.92` 或其他统一版本下限拒绝创建设备。NRC、Vulkan 扩展和各 RTX 功能仍保留自身能力检查；绕过全局门槛不等于旧驱动具备所需 Vulkan/RTX 能力，也不构成旧驱动实机兼容性保证。显式设置 `dxvk.nvidiaMinDriver` 或 `dxvk.nvidiaLinuxMinDriver` 为正值时仍可重新启用对应门槛。

这次 NVIDIA 来源推进与 L4D2 的 `cafc044` partial 适配保持独立，不能写成完整 L4D2 main 同步。正式构建仍需三端 MSVC、原生回归、源码收据及附件核验；上游 CI 不能替代本项目验证。

## v1.5.2

2026-10-13 审阅并选择 [`d2a5033ec79303fffc33ca8e6595dd560162ff69`](https://github.com/NVIDIAGameWorks/dxvk-remix/commit/d2a5033ec79303fffc33ca8e6595dd560162ff69)，替代 v1.5.1 的 `0a8c8a3`。六个新提交（三项改动及三个合并）涉及 SHaRC 预设、RTXDI 邻域采样和 Packman 发布迁移。SHaRC 上游说明的收敛速度和照明响应取舍属于未编译的 RTX renderer；本包未引入该功能，也不能宣称获得 RTXDI 阴影修复。

同版本、同补丁下逐文件核对 Bridge 和 Detours，确认 NVIDIA 基准推进未改变实际构建源码。生产脚本直接使用 Bridge/Meson 与 Detours，不调用此次修改的 Packman 下载链。GPLALL 仍独立固定。v1.5.2 的零字节上传修复来自 L4D2 上游同步，不能归因于 NVIDIA 新提交。

选择源码、构建完成和正式发布分别以本轮 `results/nvidia-update-20261013/` 收据及自动化 memory 记录；本项目仍执行完整 MSVC/原生回归和源码、附件核验。上游 CI 状态只作为补充，不代替本项目验证。

## v1.5.1

用户于 2026-10-12 明确要求采用已审阅的 NVIDIA 新提交，并纳入每小时检查及后续验证更新。

- 上一发布使用：`3a75b814600bdf7ef7b0d3a0bae55b9a8c81f34b`。
- 本次审阅并选择的构建提交：`0a8c8a3b744e19e4403c2d10feab9d1fce686cfd`。
- [上游差异](https://github.com/NVIDIAGameWorks/dxvk-remix/compare/3a75b814600bdf7ef7b0d3a0bae55b9a8c81f34b...0a8c8a3b744e19e4403c2d10feab9d1fce686cfd)：4 个提交（2 个实际改动与 2 个合并），涉及 RTX 透明表面 NRC/SHaRC 缓存采样和 Remix 覆盖层 Alt+Tab 后拖动。

这些改动位于未构建的 `src/dxvk` 组件。本项目只构建 `bridge/` 与 L4N 设置插件，渲染后端由 `config/backend.json` 独立固定。采用本次 NVIDIA 基准不会将这两项 RTX/Overlay 修复编入玩家包，也不构成本项目 FPS、画质或兼容性改善的证据。保持既有 Bridge 补丁、GPLALL、默认 x64/C8、静态映射关闭及 68 文件全量包。

源码兼容性检查比较新旧 NVIDIA 基准在相同补丁及版本渲染后的完整 Bridge 源码与 Detours；正式构建还要经过本项目三端 MSVC 编译、原生回归、源码收据和发布附件核验。源码采用与正式已发布状态分别记录于自动化 memory 和本地交付收据；上游 CI 不能代替本项目验证。

## 每小时处理规则

Codex 的 `l4d2` 任务同时检查 L4D2 与 NVIDIA 默认分支，审查 Release、已知问题和验证证据；授权范围内的更新经验证后采用并发布，不重复请求逐项确认。已知有害更新继续按用户既定标准筛选，不以预发布绕过。

GitHub Actions 同样每小时观察 NVIDIA 默认分支，但默认仅从 `config/nvidia-upstream.json` 的 `approved_commit` 构建；未经审查的新 HEAD 只报告待审查。Codex 完成审查后更新该记录。手动构建仍允许明确指定完整 SHA。先核对已有运行与源码/配方指纹，避免重复构建或发布。
