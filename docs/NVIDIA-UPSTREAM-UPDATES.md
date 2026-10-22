# NVIDIA 构建源码基准

NVIDIA dxvk-remix、L4D2 原项目和独立 GPLALL 后端是三个不同来源；本页仅记录 NVIDIA 源码采用。L4D2 原项目基准继续见 [原项目同步记录](ORIGINAL-PROJECT-UPDATES.md)。

## 2026-10-22：v1.5.17 同版本 NVIDIA 源码更新

经维护者明确授权，源码基准由 `029602f83a2fdec8f4d4419e1967bf231fb8f0b5` 更新至 [`cf83c0514d5690a2e54369ba24a6d044b989e6c8`](https://github.com/NVIDIAGameWorks/dxvk-remix/commit/cf83c0514d5690a2e54369ba24a6d044b989e6c8)。完整差异包含 20 个提交、81 个文件，其中最近审查的 6 个提交包含材质纹理变换、表面材质索引和 DLSS 界面文字调整。此前已审查的 14 个提交也随该主线基准包含在源码中。

实际编入 Bridge 的变化是 Client 材质创建、共享材质序列化和公共 C API，API 版本由 0.6.5 改为 0.6.6。不透明与半透明材质扩展分别增加滚动速度/角度、可选缩放、偏移和旋转字段，共增加 32 字节序列化载荷；旧调用者按版本复制原有结构前缀。此次不能使用“整个 Bridge 编译输入未变”的等价结论。

同版本替换仍保留 VERSION 1.5.17 和既有 Release/tag/附件名称。为区分旧新二进制，启动身份和 IPC 四个构建字均绑定实际 NVIDIA 完整提交、项目版本及规范化累计补丁；手动指定已授权源码时同样绑定该实际提交。完整配套 Client/Host 才能连接。旧新 API 对象边界、材质传输和身份拒绝由新增原生回归验证；正式三端 MSVC 14.29、完整项目回归、源码收据和线上附件核验均为发布门禁，具体通过结果与原始失败保留在该次 CI 与 `results/nvidia-update-20261022/`。

`src/dxvk`、RTX 着色器、DLSS、根 Meson/Packman 构建链不进入本包的 Bridge 构建；Detours 及独立 GPLALL 基准保持固定。随包 `exposeRemixApi=False`，普通 D3D9 Draw/Present 路径没有新增材质变换工作，不能据此宣传本包获得 RTX/DLSS/纹理变换功能或帧率、画质提升。无需重复启动游戏验证未编入的渲染器；本轮没有新增游戏、FPS、画质、长稳或退出验收。L4N-necola、用户配置、ThinFlex 及历史失败记录保留。

## 2026-10-16：运行时初始化更新与全局驱动门槛移除

本次审阅并采用 NVIDIA `main` 的 [`029602f83a2fdec8f4d4419e1967bf231fb8f0b5`](https://github.com/NVIDIAGameWorks/dxvk-remix/commit/029602f83a2fdec8f4d4419e1967bf231fb8f0b5)，替代此前固定的 `d2a5033ec79303fffc33ca8e6595dd560162ff69`。编入本包的 Bridge 变化包括运行时缺失时回退 x64、Remix 初始化状态 IPC、Host 初始化状态传播及 Bridge API 初始化顺序适配。Client 与 x86/x64 Host 必须使用同一组配套二进制；本次没有声称修复首次启动或开机冷启动闪退，也没有新增游戏/FPS/画质验收证据。

本项目仅构建 NVIDIA 的 `bridge/`，不编译或分发其 `src/dxvk` RTX Remix 渲染器。此次基准包含的 RTX/稀疏渲染与粒子法线变化属于源码采用范围，未编入玩家包。随包 GPLALL 后端继续由 `config/backend.json` 独立固定；不能将这些上游变化描述为本包新增的渲染功能或画质收益。

按维护者要求，本分支补丁将 NVIDIA `src/dxvk/dxvk_options.cpp` 中 Windows、GFN 与 Linux 的全局驱动版本下限默认值均设为 `0.0f`，并使 `dxvk_adapter.cpp` 仅在下限非零时执行版本拦截；既不保留 `610.47`，也不采用上游 `616.92` 提升。这项源码策略不参与本包编译，不改变 GPLALL 源码及其驱动检查，也不能据此声称本包新增了旧驱动兼容性。渲染器源码中的 NRC、Vulkan 扩展和各 RTX 功能能力检查继续保留。若另行构建该渲染器，显式将 `dxvk.nvidiaMinDriver` 或 `dxvk.nvidiaLinuxMinDriver` 设为正值，或使用 `DXVK_REMIX_NVIDIA_MIN_DRIVER` 环境覆盖，仍可重新启用对应版本门槛。

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
