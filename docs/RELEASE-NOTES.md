# v1.5.2

- 同步 L4D2 上游至 `c8d55b0`，修复动态 VB/IB 缓冲区末尾的零字节上传被 Host 提前拒绝、导致会话终止的问题；保留后端错误处理与内存边界保护。
- 补充 IPC 默认重试预算下的长延迟回复、取消与遗弃所有者回归验证，包含旧缺陷负对照。
- NVIDIA 构建源码更新至 `d2a5033`。新增 RTX/Packman 改动不进入本包的 Bridge 构建，GPLALL 未升级。

验证：三端 MSVC 编译、双架构原生回归、源码及全量包核验。未重复游戏性能测试，不宣称帧率或画质提升；此前性能与 Steam 模块适配限制仍存在。

**如果出现 1% low 下降、掉帧或帧数不稳定，请返回使用 [v1.1.10](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/releases/tag/v1.1.10-nightly-20261009-3a75b814-r44c3dfa4ec5c-b38052959126.1)。**
退出游戏和所有 Bridge Host 后，成套恢复 Client 与两个 Host。

[安装说明](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/blob/main/README.md)
