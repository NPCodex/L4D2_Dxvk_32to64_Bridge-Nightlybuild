# v1.5.0

- 合入上游 v1.3.0 的 Steam 窗口、输入与工坊适配，补齐呈现子窗口的配套默认设置。
- 整合 45 项台账中的已实现改进：上传与参数边界、Query 未完成结果、并发回复及 IPC 关闭尾包修复，上传池扫描与数据库解析调整。
- 加入 IPC 批次、静态映射设置及中英文菜单；设置保存后重启生效，语言切换仅本会话有效。

历史 v1.4.4 的 45 项候选在三组实机对照中，以 Host Present 提交间隔统计的平均 FPS 开灯/关灯分别下降 2.59%/1.97%，1% low 下降 0.72%/0.78%；维护者知悉后明确要求采用。这些结果不含本次新增 Steam 组合，最终 v1.5.0 包未重新进行完整实机性能对照，不承诺帧率收益。Steam/ReShade、重 Mod 和长期稳定性验证范围有限。本次编译与原生回归结果见下方构建记录。

**如果出现 1% low 下降、掉帧或帧数不稳定，请返回使用 [v1.1.10](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/releases/tag/v1.1.10-nightly-20261009-3a75b814-r44c3dfa4ec5c-b38052959126.1)。**
退出游戏和所有 Bridge Host 后，成套恢复 Client 与两个 Host。

[安装说明](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/blob/main/README.md) · [45 项处理台账](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/blob/main/docs/OPTIMIZATION-45.md)
