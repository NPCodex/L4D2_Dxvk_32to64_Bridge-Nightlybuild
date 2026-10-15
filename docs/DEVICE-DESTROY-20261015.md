# v1.5.15：设备销毁清理修复

设备创建失败后的清理或重复 Destroy 原先通过严格 GET_RES 查找；设备不存在时会把整个 IPC 会话标记为故障。现在只在 Host IDirect3DDevice9Ex_Destroy 中读取句柄、允许查找为空，再调用原有 safeDestroy 和 erase。有效设备仍只释放一次；普通命令的无效句柄继续触发故障。没有更改 wire schema，自动源码身份随版本与补丁更新，因此 Client 与两个 Host 仍须同批更新。

该修复为 L4D2 cafc0442446c502a0de0ebb52d5e73ccaae67516 的选择性移植，完整 main 基准不推进。Query、Shader、UpdateTexture、失败 Lock 和 SharedHeap 保护已存在于 v1.5.14；IME、工坊更新器、旧跳过 Steam 组、VB/IB unpin 与新 NVIDIA pin 不在本次范围。

候选使用 MSVC 14.51 完成 x86 Client、x86 Host 和 x64 Host 编译。实际源码提取的 Destroy/lookup/safeDestroy/live-command 夹具在两种架构各通过 28 个断言，旧实现负对照均准确检出重复销毁导致通道故障。D3D 后端及传输使用适配器，这不是完整真实 IPC、游戏、FPS 或长时间稳定性测试。

正式 workflow 新增显式 MSVC 14.29 的双架构 Destroy 门禁，并保留三端编译、完整原生回归、最终源码收据与 68 文件包检查。正式构建完成与发布状态由 results/release-v1515-20261015/ 收据记录；此前候选结果不能代替正式 CI。

同时带入已授权的发布日志维护：仅提取当前 VERSION 的说明，历史章节继续保留在 docs/RELEASE-NOTES.md。NVIDIA approved_commit、GPLALL 固定源码/补丁、运行配置与安装包规则不变。
