# v1.4.0

- 修复上传 Lock 失败、尺寸/行距、Shader 常量范围、Query 未完成结果及共享分配边界的处理，并避免并发回复互相耗尽重试。
- 减少上传池扫描、紧密上传复制和学习数据库解析开销；保留历史数据、同步、画质与默认设置。
- 增加 IPC 批次、静态映射配置及中英文设置菜单；配置保存后重启生效。

原生验证覆盖 x86/x64、真实跨进程传输及旧错误负对照。组件收益不代表保证提升游戏帧率；整包实机验收结果和限制随本版本 Release 记录。

**如果出现 1% low 下降、掉帧或帧数不稳定，请返回使用 [v1.1.10](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/releases/tag/v1.1.10-nightly-20261009-3a75b814-r44c3dfa4ec5c-b38052959126.1)。退出游戏和所有 Bridge Host 后，成套恢复 Client 与两个 Host。**

[45 项处理台账](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/blob/main/docs/OPTIMIZATION-45.md) · [安装说明](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/blob/main/README.md)
