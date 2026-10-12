# v1.5.1

- 更新 NVIDIA dxvk-remix 构建源码基准至 `0a8c8a3`，完成本项目三端编译、原生回归及全量包核验。
- 每小时检查 NVIDIA 新提交，构建使用审查通过的明确提交，避免未经审查的新 HEAD 直接发布。

本次 NVIDIA 的透明表面采样及 Alt+Tab 覆盖层修复属于未打包的 RTX/Remix 组件；Bridge 实际源码未因这些提交改变，GPLALL 后端保持原版本。本次不宣称帧率或画质提升，未重复实机性能对照。此前已披露的性能与 Steam 兼容性限制仍然存在。

**如果出现 1% low 下降、掉帧或帧数不稳定，请返回使用 [v1.1.10](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/releases/tag/v1.1.10-nightly-20261009-3a75b814-r44c3dfa4ec5c-b38052959126.1)。**
退出游戏和所有 Bridge Host 后，成套恢复 Client 与两个 Host。

[安装说明](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/blob/main/README.md)
