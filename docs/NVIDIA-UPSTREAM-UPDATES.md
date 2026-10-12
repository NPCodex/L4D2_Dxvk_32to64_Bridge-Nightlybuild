# NVIDIA 构建源码基准

NVIDIA dxvk-remix、L4D2 原项目和独立 GPLALL 后端是三个不同来源；本页仅记录 NVIDIA 源码采用。L4D2 原项目基准继续见 [原项目同步记录](ORIGINAL-PROJECT-UPDATES.md)。

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
