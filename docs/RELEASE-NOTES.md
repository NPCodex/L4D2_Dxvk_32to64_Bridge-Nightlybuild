# v1.4.4

- 修复上传 Lock 失败、尺寸/行距、Shader 常量范围、Query 未完成结果、键盘状态空指针及共享分配边界的处理，并避免并发回复互相耗尽重试。
- 修复 IPC 关闭时读取端可能遗漏最后一批命令的问题；以可稳定触发旧错误的 x86/x64 回归验证尾包只处理一次。
- 减少上传池扫描、紧密上传复制和学习数据库解析开销；保留历史数据、同步、画质与默认设置。
- 增加 IPC 批次和静态映射设置，保存后重启生效；中英文菜单可在当前会话切换。

原生验证覆盖 x86/x64、真实跨进程传输及旧错误负对照。组件收益不代表保证提升游戏帧率；整包实机验收结果和限制随本版本 Release 记录。

Steam Overlay 兼容性尚未确认，本次未包含上游整组 Steam 适配改动。

**如果出现 1% low 下降、掉帧或帧数不稳定，请返回使用 [v1.1.10](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/releases/tag/v1.1.10-nightly-20261009-3a75b814-r44c3dfa4ec5c-b38052959126.1)。退出游戏和所有 Bridge Host 后，成套恢复 Client 与两个 Host。**

[45 项处理台账](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/blob/main/docs/OPTIMIZATION-45.md) · [安装说明](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/blob/main/README.md)
