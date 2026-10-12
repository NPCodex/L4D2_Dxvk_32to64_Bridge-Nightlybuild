# v1.3.0

- 合入上游 v1.3.0-pre.1 的完整 IPC v2 与独立上传 lease，默认使用 C8 批次；保留六档配置。
- 纳入 VB/IB VA 分配与静态按需映射能力，按需映射默认关闭。

上游候选相对 v1.2.3 的三组实机对照：开灯平均 FPS +1.97%、1% low −6.78%；关灯平均 FPS +0.51%、1% low −7.43%。两版均通过换图、分辨率切换和退出检查；未证明长期稳定性。本次整合包执行三端编译、原生回归和全量包校验，未重复游戏测试。

**如果出现 1% low 下降、掉帧或帧数不稳定，请返回使用 [v1.1.10](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/releases/tag/v1.1.10-nightly-20261009-3a75b814-r44c3dfa4ec5c-b38052959126.1)。退出游戏和所有 Bridge Host 后，成套恢复该版本的 Client 与两个 Host，避免混用。**

[本次集成及测试范围](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/blob/main/docs/UPSTREAM-1.3.0-INTEGRATION.md)。
