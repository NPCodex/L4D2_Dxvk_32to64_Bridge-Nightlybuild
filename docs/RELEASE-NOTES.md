# v1.5.4

- 合入 IPC 立即发布路径调整：省去空批次立即发布前的计时读取和临时 deadline 设置，发布失败仍恢复 pending 标记，保留关闭、取消及故障保护。
- 合入 GPLALL MSAA linear/nearest 着色器的 `DontUnroll` 循环提示调整。采样数、加权公式与回退保留；两个架构后端使用固定输入和输出校验的可重现变换。

验证：CI 执行三端编译、完整原生回归和源码收据校验；新增 IPC 双架构共 52 正例、2 个预期失败负对照与 Client32 四写者 2048 命令并发检查。GPLALL 双架构原生图像对照及真实帧回放均保留原始结果。

已知取舍：本次由维护者在审阅结果后明确采用。GPU 未通过原先逐字节一致门槛：捕获帧中间图像有 26 个分量相差 1 half ULP，最终图像相同；原生每组 224 案例中有 4 案例、7 分量不同。六组回放的目标事件耗时配对中位约增加 0.69%，另一个小事件也持续变慢，未证明 GPU 提速；事件区间可能重叠，不能当作完整帧耗时。IPC 此前配对对照也未证明稳定收益。未完成本组合的游戏性能或全场景画质验证，不承诺 FPS、1% low 或长期稳定性提升。[实现与验证范围](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/blob/main/docs/GPU-IPC-20261013.md)。

**如果出现 1% low 下降、掉帧或帧数不稳定，请返回使用 [v1.1.10](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/releases/tag/v1.1.10-nightly-20261009-3a75b814-r44c3dfa4ec5c-b38052959126.1)。**
退出游戏和所有 Bridge Host 后，成套恢复 Client 与两个 Host；恢复该旧版的两个 GPLALL 后端以撤销本次 GPU 改动。

[安装说明](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/blob/main/README.md)
