# v1.5.3

- 选择性采用上游 v1.3.1 的 Steam 邀请入口保护，仍要求匹配模块指纹、调用位置与有效 Host 状态；保持原 SDK 恰好调用一次、原参数及 LastError。
- 为既有 Steam 窗口观察包装固定代码模块生命周期，避免包装仍被引用时模块卸载。
- L4N 菜单改称“ReShade / Steam Overlay”，补充共享 Presenter 依赖及停用警告；配置保存行为不变。

验证：三端 MSVC 编译、双架构原生回归，包含邀请保护及真实 DLL 生命周期的旧行为负对照；源码与全量包核验。上游新输入捕获与配套默认值未采用，完整同步基准仍为 `c8d55b0`。未新增实际 Steam 邀请/Overlay 交互或游戏性能验收，不宣称 FPS 提升或所有 Steam 版本兼容。

**如果出现 1% low 下降、掉帧或帧数不稳定，请返回使用 [v1.1.10](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/releases/tag/v1.1.10-nightly-20261009-3a75b814-r44c3dfa4ec5c-b38052959126.1)。**
退出游戏和所有 Bridge Host 后，成套恢复 Client 与两个 Host。

[安装说明](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/blob/main/README.md)
