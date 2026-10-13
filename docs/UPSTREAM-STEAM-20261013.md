# v1.5.3：上游 v1.3.1 Steam 改动的选择性集成

审阅 L4D2 upstream/main `677653d5d6751377799187c0677ff1e7d3f2f9ad`，对应上游 v1.3.1 发布后的 main。上游 Release 指向 `9e3ee1292f901b0bb9352e36bba93a4cd5cfbda4`；main 原生 CI 38356616870 与发布候选 CI 38353311704 成功。上游历史 Steam 实验验收不能代替当前完整组合的游戏验收。NVIDIA 已批准/构建基准仍为 `d2a5033ec79303fffc33ca8e6595dd560162ff69`，本次未变。

## 实际采用

| 范围 | 变化与保护 |
|---|---|
| steam_web_diagnostics.h / steam_workshop_status.h | 增加 GameUI 邀请调用位置 `0x11a03` 的可用性保护；保留完整模块指纹、Host 存活与状态新鲜度要求，以及原 SDK 的一次调用、借用参数和 LastError 语义。 |
| steam_window_diagnostics.h | 固定观察包装代码所在模块的生命周期，避免包装仍被其他模块持有时卸载；新增向后兼容的角色参数。既有 Host 调用沿用默认角色，本次没有接入 Client renderer 观察。 |
| L4N 插件及测试 | 双语菜单改称 ReShade / Steam Overlay，说明共享 Presenter 依赖及停用影响；操作 ID、配置保存值和 ABI 不变。 |

累计生产补丁 SHA256：`b2dec80f95ebac25f0f5ab064e9c13d681ae1efb457e53b07b0cff54fb59efd1`。三个选定 header 的源码及插件/对应四个测试文件与审阅的上游内容一致（仅规范化换行）。其余 133 个累计补丁文件段保持原样。准备源码的 285 文件审计中，除三处 header 外只有版本日志和共同 IPC 源码身份标识变化；渲染、上传、队列与输入路径不变。

## 暂不采用的关联改动

`window.cpp` 与 `remix_state.h/.cpp` 的输入捕获/owner-thread 激活整组不导入，配套默认钩子 False/False 与 Client 观察器接入一并跳过，不制作可选包绕过。

新 `applySteamActivation(true)` 在旧 UI 消息处理前调用 `RemixState::setSteam(true)`。旧 `remixMsg` 依赖原聚合状态判断首次激活，并释放 Ctrl/Shift/Insert 及按需恢复自定义钩子。新顺序使旧分支看到的状态已是 active，因此绕过原有清理。此结论来自源码控制流，尚未在游戏中复现卡键，不能把它写成实机故障或性能回退。

另一个未解决边界是 `g_steamActive.exchange` 与 `RemixState::setSteam` 未共同串行化；若完整辅助函数并发执行 true/false，最终状态可能不一致。现有并发测试只覆盖 `setSteam`，不覆盖整个辅助函数。尚未证明真实 Steam 回调会以这种方式并发。上游默认 `client.hookMessagePump=False`、`client.overrideCustomWinHooks=False` 又与现有菜单 Enable 写入 True/True 不一致，缺少新组合实机证据。

所选邀请保护和生命周期 header 不依赖上述捕获状态，可独立分离。上游实验配置、后端替换和多包发布流程不进入本分支；现有 Bridge 配置、Steam 修复片段、GPLALL、L4N 源清单与用户配置保持原样。

## 验证范围与发布门槛

- 双架构 Steam 窗口/工坊原生回归及现有 ABI 转发测试；x86 邀请返回地址保护的生产 header 正例与精确移除新允许位置的旧行为负对照。
- x86/x64 真正 DLL 生命周期测试：包装安装后 Stop/FreeLibrary，生产固定模块仍可调用保留包装；仅移除新增 PIN 块的负对照检测模块已卸载，不解引用卸载地址。
- 正式 Actions 必须通过三端 MSVC Release 编译、x86 插件构建和完整既有原生回归，并核对最终源码收据、68 文件全量包、校验文件与 Release 附件字节。发布以交付收据为准，不能把本地 MSVC 14.51 检查冒充正式 14.29 构建。

本次不自动启动游戏；原玩家安装保持只读。源码范围和原生测试只支持上述保护行为，不证明实际 Steam 邀请/Overlay/工坊交互、所有模块指纹、游戏 FPS 或长期稳定性。固定模块会延长代码模块驻留到进程退出，不声称没有任何资源成本。

## 基准与证据

`config/original-project.json` 分开记录完整基准 `c8d55b0f5d913d71488266469b4dca236439a4e9`、已审阅 `677653d`、部分采用 `677653d`。这是普通选择性移植提交，不将 677653d 全部历史作为完整合入的第二父提交。正式已构建/已发布状态另记 `results/heartbeat-20261013T064200Z/` 交付收据与自动化 memory。

审查证据位于该目录的 `upstream/REPORT.md`、`selection.json`、`correctness-review/selected-subset-audit.json`、`native-review/` 和正式 Actions 附件。旧性能结果保留为历史记录，不作为本版本测量。Release 与玩家 README 继续保留 v1.1.10 回退链接，退出游戏和全部 Host 后成套恢复 Client 与两个 Host。
