# v1.5.2 上游安全增量审查

本次采用 L4D2 main `c8d55b0f5d913d71488266469b4dca236439a4e9`（PR #11），NVIDIA 构建基准独立推进到 `d2a5033ec79303fffc33ca8e6595dd560162ff69`。依据是上游源码、发布/Issue/CI 记录和本项目实际实现对照，不把上游对 Nightly 已有修复的采用重复算作新功能。

## 运行实现的对应关系

| 上游内容 | 本分支最终处理 |
| --- | --- |
| Reader 关闭时重新读取已发布边界 | 已有 c2e7694 修复；保留相同算法和确定性调度测试。 |
| MessageQueue 关闭尾包 | 已有修复；保留 Nightly 仅读取已初始化范围的 scratch 优化，不用上游整文件覆盖。 |
| 其他 UID 的回复不消耗当前等待者重试次数 | 已有 9724c007 修复；保留 UID/载荷归属和硬截止时间。 |
| 取消检查在 Sleep 前执行 | 保留现有 Sleep 后检查、Client 统一失败出口；当前生产 UID 验证调用来自 Client，均失败关闭，差异最多一个调度等待。新增取消用例验证现有路径，不宣传降低延迟。 |
| VB/IB、Surface、Volume 后端 Lock 失败保护 | 已有 Host helper 和生产处理器回归；失败不得复制或 Unlock。保留 Nightly 的紧凑复制及安全分配策略。 |
| x64 上的小 mip 逻辑行宽与源/目标边界 | 已有修复；保留实际像素宽度、pitch、溢出与分配失败检查，不复制对齐填充。 |
| VB/IB 零字节上传 | 本次独立修复：把有效空上传交给后端 Lock/Unlock，保留后端 HRESULT，不读写空载荷，不提前制造会话故障。 |
| 300 retries、3300/6500ms 所有者暂停、取消、遗弃 | 新增生产 IPC 提取套件；保留 3300ms 观察和明确的旧缺陷负对照。 |

零字节包可以由现有 Client 产生：动态 VB/IB 的 Lock 仅拒绝 `offset > Size`，因此 `offset == Size` 可以成功进入；Unlock 将剩余长度算为零。旧 Host 的 `!bytes` 检查在后端 Lock 前返回 INVALIDCALL，随后触发会话失败。本次仅修正这条兼容路径，不跳过真实后端失败，也不将所有空请求无条件视作成功。

## 合并范围与验证

Git 合并保留 c8d55b0 为上游父提交，再逐项协调差异。`patches/l4d2-bridge.patch` 的运行时增量限于 Host 空上传 helper；现有 Query、资源引用、池分配、日志、Steam、偏色和 ThinFlex 行为保留。实验镜像与历史测试不替换已覆盖的生产实现；关闭尾包测试和保护 hash 继续使用本分支版本。

上游 `test_ipc_upload_safety.ps1`、Host 提取器和 fixture 的重复检查由现有 `test_host_upload_contract.ps1` 与新增空上传用例覆盖。新回复套件为 `test_ipc_reply_budget.ps1`，接入完整原生回归入口和 CI 验证附件。负对照只撤销目标修复，并要求命中特定断言/早退时序，不能把编译失败或任意异常当成成功检测。上游冻结 f6a059b 的整版替换不进入生产源码。

NVIDIA 的六个提交只改变未构建的 RTX 组件和未调用的 Packman 下载链。同版本、同补丁下，285 个 Bridge 文件仅归一化 CRLF/LF 后完全一致，195 个 Detours 文件原字节一致。因此 NVIDIA 基准推进本身不带来 RTXDI 阴影、SHaRC 画质或本包性能收益。其等价证据与 Host 的实际代码变更分别记录。

本轮要求三端 MSVC 编译、全部原生回归、Python 检查、最终源码收据以及 CI/Release 逐字节核验；不启动游戏或修改玩家安装，不声称实机 FPS、画质全覆盖或长期稳定性通过。源码选择、构建和正式发布状态以 `results/nvidia-update-20261013/` 本地交付收据及自动化 memory 为准，未完成的阶段不得标成已发布。既有性能和 Steam 兼容性限制继续披露。

## 来源

- [L4D2 PR #11](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/pull/11) 与 [安全说明](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/blob/c8d55b0f5d913d71488266469b4dca236439a4e9/docs/IPC-UPLOAD-SAFETY.md)。PR 正文中的 Draft 字样是旧描述，以已合并的实际提交为准。
- [NVIDIA 差异](https://github.com/NVIDIAGameWorks/dxvk-remix/compare/0a8c8a3b744e19e4403c2d10feab9d1fce686cfd...d2a5033ec79303fffc33ca8e6595dd560162ff69)。
- 本地审查证据：`results/upstream-c8d55b0/`、`results/nvidia-update-20261012/heartbeat-20261013T020130Z/` 和 `results/nvidia-update-20261013/source-equivalence-pre-version.json`。
