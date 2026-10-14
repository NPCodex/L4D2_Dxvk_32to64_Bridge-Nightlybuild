# v1.5.12

- 修复 Steam 捕获跨线程通知适配中引用不存在的 `RemixState` Steam 聚合接口，改用本补丁已验证的客户端捕获状态，恢复 x86/x64 Bridge 的一致编译路径。
- 保留 Renderer 11.12.30.7 x64 身份白名单、跨线程通知、鼠标捕获清理和诊断停止逻辑；本版本没有新增 FPS、ReShade 键盘或全场景游戏保证。

验证：全新 NVIDIA Bridge 基线可完整应用累计补丁；205 项 Python 回归通过（13 项按环境跳过）。正式 Windows CI 将继续执行三端 MSVC、原生回归、源码收据和 68 文件包校验。

安装方式见 [README](../README.md)。如出现 1% low 下降、掉帧或帧数不稳定，请按 README 的 v1.1.10 回退说明成套恢复 Client、两个 Host 和两个 GPLALL 后端。

# v1.5.9

- 适配已审阅的 Steam Renderer 11.12.30.7 x64 文件身份，保留未知版本拒绝和一次性哈希校验。
- 增加 Steam 捕获状态的跨线程窗口通知，恢复游戏线程上的鼠标捕获清理，并在窗口终止时停止 Renderer 观察器。
- x86/x64 可用性、Steam 窗口观察、跨架构适配和生命周期负对照通过；没有新增 FPS、ReShade 键盘或全场景游戏保证。

安装方式见 [README](../README.md)。如出现 1% low 下降、掉帧或帧数不稳定，请按 README 的 v1.1.10 回退说明成套恢复 Client、两个 Host 和两个 GPLALL 后端。

# v1.5.8

- 修复 retention 数据库补丁头的行数声明错误，避免 Git 在 CI 应用补丁时截断 `retention_database.h`。结合 Vulkan SDK 1.4.363 的 `glslangValidator` 安装，GPLALL/Bridge 源码构建链现在具备正确输入和着色器工具。
- v1.5.6 的 O34、GPL-CLIP、GPL-WORKERS 和 GPL-SPEC 内容保持不变；本版本仍不宣称 FPS、全场景画质或长期稳定性提升。

# v1.5.7

- 修复 Windows runner 未提供 `glslangValidator` 导致 GPLALL 固定源码构建在 Meson 配置阶段失败的问题。正式 workflow 现在安装并验证 Vulkan SDK 1.4.363，再开始三端 MSVC 构建。
- v1.5.6 的 O34、GPL-CLIP、GPL-WORKERS 和 GPL-SPEC 内容保持不变；本版本仍不宣称 FPS、全场景画质或长期稳定性提升。

# v1.5.6

- 合入四项经审阅的候选：Bridge retention 数据库的单次数字解析与延迟 CNG provider、GPLALL 裁剪平面 packed override 修复、低核数 shader worker 下溢保护，以及按实际可用 GPL 能力判断 specialization 状态。
- GPLALL 后端改为固定源码 `Digger1955/dxvk-gplall@6a0ea561f9add008899680e6c313aa21c151e03e` 构建 x86/x64，并保留已发布的 MSAA `DontUnroll` 变换。构建收据会逐字节绑定三项 GPL 源码补丁和两个架构 DLL。

验证范围：O34 的 x86 warm-cache `Database::open` 微基准在平衡对照中约快 49%（每次约 0.36-0.42 ms），不是 FPS、冷启动或完整游戏结论。GPL-CLIP 与 GPL-WORKERS 是正确性/边界修复，没有已验证的帧率收益。GPL-SPEC 的四轮游戏对照方向不稳定，未建立稳定 FPS 或 1% low 收益。本次仍不声称游戏性能、全场景画质或长期稳定性提升。

正式 Windows CI 使用 MSVC 14.29 完成三端 Bridge、固定 GPLALL x86/x64 后端、原生回归、源码收据和 68 文件 runtime 包校验；本地候选研究使用 MSVC 14.51 的结果不替代正式工具链证据。

**如果出现 1% low 下降、掉帧或帧数不稳定，请返回使用 [v1.1.10](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/releases/tag/v1.1.10-nightly-20261009-3a75b814-r44c3dfa4ec5c-b38052959126.1)。**
退出游戏和所有 Bridge Host 后，成套恢复 Client、两个 Host 以及该版本的两个 GPLALL 后端。

[四项调查与验证记录](OPTIMIZATION-37-20261013.md)

## v1.5.4

- 合入 IPC 立即发布路径调整：省去空批次立即发布前的计时读取和临时 deadline 设置，发布失败仍恢复 pending 标记，保留关闭、取消及故障保护。
- 合入 GPLALL MSAA linear/nearest 着色器的 `DontUnroll` 循环提示调整。采样数、加权公式与回退保留；两个架构后端使用固定输入和输出校验的可重现变换。

验证：CI 执行三端编译、完整原生回归和源码收据校验；新增 IPC 双架构共 52 正例、2 个预期失败负对照与 Client32 四写者 2048 命令并发检查。GPLALL 双架构原生图像对照及真实帧回放均保留原始结果。

已知取舍：本次由维护者在审阅结果后明确采用。GPU 未通过原先逐字节一致门槛：捕获帧中间图像有 26 个分量相差 1 half ULP，最终图像相同；原生每组 224 案例中有 4 案例、7 分量不同。六组回放的目标事件耗时配对中位约增加 0.69%，另一个小事件也持续变慢，未证明 GPU 提速；事件区间可能重叠，不能当作完整帧耗时。IPC 此前配对对照也未证明稳定收益。未完成本组合的游戏性能或全场景画质验证，不承诺 FPS、1% low 或长期稳定性提升。[实现与验证范围](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/blob/main/docs/GPU-IPC-20261013.md)。

**如果出现 1% low 下降、掉帧或帧数不稳定，请返回使用 [v1.1.10](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/releases/tag/v1.1.10-nightly-20261009-3a75b814-r44c3dfa4ec5c-b38052959126.1)。**
退出游戏和所有 Bridge Host 后，成套恢复 Client 与两个 Host；恢复该旧版的两个 GPLALL 后端以撤销本次 GPU 改动。

[安装说明](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/blob/main/README.md)
