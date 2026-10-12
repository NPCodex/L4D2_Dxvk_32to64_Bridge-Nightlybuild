# L4D2 原项目完整同步

当前完整功能基准为 [`d063395146bfcad8688efe52904541d342f237f0`](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/tree/d063395146bfcad8688efe52904541d342f237f0)，对应上游 **v1.3.0-pre.1**；该次采用的 Nightly 版本为 **v1.3.0**。维护者于 2026-10-12 查看三组实测中 1% low 下降的取舍后，明确批准采用，并要求在发布说明中提供 v1.1.10 回退链接。范围、保留修复、测试结果及最终整合包未复测的边界见 [本次集成说明](UPSTREAM-1.3.0-INTEGRATION.md)。

2026-10-12 已检查的 upstream/main 推进至 `f6a059b758f9fcbf9c66ace73625aae9db34ad6a`（上游正式 v1.3.0）。完整合入的 main 基准仍为 `de74cd7d5dd546bfd4f03ab451d0565f1fff0107`，实际完整功能基准仍为单独授权采用的 d063395；检查新 main 不代表已完整同步。v1.4.4 候选按维护者全部 45 项处理授权，选择性导入 a10df351 的 IPC/映射设置与语言菜单及其测试，并独立移植新 main 的键盘状态空指针保护；同时修正 x86/x64 学习数据库回归脚本的源码路径传递。没有导入其他发布流程。当前改动和关闭项见 [45 项台账](OPTIMIZATION-45.md)。Git 合并保留历史；普通后续更新仍按既定性能、稳定性及兼容性要求筛选。

2026-10-12 后续整包验证：上述选择性移植所在的 v1.4.4 / `84aff8b` 候选通过编译、原生测试与四组双架构 smoke，但六轮十二段实机采集的平均 FPS 重复下降，整包未获采用、未合入正式 main、未发布。详见[性能门禁报告](../results/optimization45/game/PERFORMANCE-GATE-REPORT.md)。正式 main/Latest 仍为 `ac75142` / v1.3.0；已检查 main、完整 main 合入和完整功能基准分别保持 f6a059b、de74cd7、d063395。不能把候选中已编写的菜单或键盘保护标记为正式采用。

## 2026-10-12：上游正式 v1.3.0 增量筛选

已检查上游发布说明、PR #7/#8/#9/#10 状态、原生 CI 和实机验收边界。上游 [38223017006](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/actions/runs/38223017006) 完成编译和回归。还原累计补丁后，相对 d063395 的实际 Bridge 源码变化共 17 个文件；完整 IPC v2、上传 lease、VA 分配及按需映射核心没有新增变化，不能因正式版重新整合而重复移植。

- **实际选择**：L4N IPC/Shadow/语言菜单已由 D6 实现，不重复导入。新增独立键盘保护：UI 捕获期间 `GetKeyboardState(nullptr)` 返回失败及 `ERROR_INVALID_PARAMETER`，不再写入空指针；普通状态继续委托原函数。x86/x64 MSVC Release `/O2 /DNDEBUG /W4 /WX` 通过有效输出 256 字节、边界、错误码和空指针检查；移除保护的负对照在两种架构均检出原错误。原始提交为 [`4494cdd`](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/commit/4494cdd47dc88240daf634ecd147f92e1a0654ea)，现已进入 f6a059b 的累计生产补丁；本轮测试和审查证据保存在 `results/optimization45/upstream-monitor-now/`。
- **本次不导入 Steam 整组行为**：上游用户已报告 Shift+Tab、鼠标、Join Server 和工坊入口通过，限定 GameUI/Renderer 指纹、调用位置、Host 存活及 3 秒新鲜度检查有原生覆盖。但最终组合没有新的平均 FPS / 1% low / 完整地图与 Reset 对照，ReShade 新组合和其他 Steam 版本也不在本次实机验收范围。不能将“尚未验证”写成“已证明性能回退”，也不能据此宣称本分支已支持这些场景。
- **需解决的整合边界**：上游默认 conf 开启窗口适配却保持 `server.presenterWindow=False`，而实际窗口替换需要它创建的 Host 子窗口；完整修复片段另设为 True。工坊代理开启时调用观察器的 `poll(true)`，即使 `steamWebDiagnostics=False` 仍执行观察包装和有上限的日志/计数。保留本分支已验证默认配置，不把这组行为改成随包可选/测试版本绕过验收。

本节只推进已检查基准，记录候选中的实际选择和未采用原因；完整合入基准及正式发布状态不因本次审查而推进。

## 历史：上游 1.2.2 集成

此前完整功能基准：[`de74cd7d5dd546bfd4f03ab451d0565f1fff0107`](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/tree/de74cd7d5dd546bfd4f03ab451d0565f1fff0107)，即上游 **1.2.2 稳定性更新**。本分支 v1.2.2 在此前完整基准 `cf49175` 上同步全部新增源码、测试与说明；两者版本号独立。维护者已了解本机单轮对照约 8% 的平均 FPS 下降与未见明显 1% low 收益，仍明确批准采用本次 IPC 修复。性能、地图切换结果及最终重建边界见 [本次集成说明](UPSTREAM-1.2.2-INTEGRATION.md)。

用户于 2026-10-10 明确授权完整合并上游，包括上游新增功能；2026-10-11 在查看本机测试取舍后再次明确批准同步上游 1.2.2。“不自行增加功能”不再作为筛除上游能力的理由。Git 合并保留两侧提交历史；以完整上游 Bridge 源码为基础融合 Nightly 修复，后续在此基准上增量同步。

## 历史：v1.2.2 功能范围

2026-10-11 完整同步双向数据环完整预留、消费进度原子发布与等待重查、失败批次中止、Header 有界提交、请求/回复所有权清理、optimized Lock 指针保留、已知包完整规划及环尾 padding 消费。IPC 协议为 3，Client 与两个 Host 必须配套。保留原有 FULL_SHADOW、画质、后端和配置选择；不恢复已撤回的 Nightly 绑定引用优化。上游正常路径/实机性能报告原文保留，但不能作为本分支性能提升证明。

完整保留上游 PageBlock residency、keep/learned-aggressive/drop、LGC/AGC/FGC、retention DB、Host ACK 和 readback recovery；Reset 状态机及等待；Presenter/ReShade 与 Steam 输入路径；x86/x64 Host、adapter/caps；L4N v2 设置/保存 API 与插件；memory/crash/data/color/API-wait 诊断及关闭时零观察开销；相关配置、实验源码、分析工具、文档与测试。

## Nightly 兼容差异

- 默认仍为 x64 Host，x86 Host 与匹配后端随完整包提供。GPLALL 2.6.8-2 两种架构按固定归档与文件哈希校验，实验 DXVK 后端仍通过独立手动实验入口构建。
- 继续使用 `bin/d3d9.dll` 的默认客户端路径，支持用户为 `-vulkan` 改名；Host 与后端位于 `bin/.l4d2bridge`。新诊断工具与 L4N 接口兼容两种客户端名称。
- 保留纹理字节布局/偏色修复、额外资源创建失败清理、buffer lock 边界、连续上传复制、Logger 名称所有权与队列优化。内存监控开启时沿用异步普通采样，关闭时遵循上游新开关，不调度采样。
- 新安装采用上游的新增 Bridge 配置与策略，唯 Host 默认保持 x64。L4N 与 DXVK 配置按用户提供的整合目录随包；升级先从临时目录移除想保留的配置，再覆盖，操作见 README。retention DB 不随包。
- 只发布一个完整 ZIP 及 SHA256，包含配套 Client、两种 Host/GPLALL、固定 ThinFlex `studiorender.dll`、Starfelll 的 L4N 2.51.0、用户提供的 `dxvk.conf`/`config.vdf` 及直接安装的 `bin/neko/plugins/L4D2BridgePlugin.dll`；不附 `config_template.vdf`，不恢复 update 包或独立插件/ThinFlex 工具包。L4N 来源与归属见 [整合说明](L4N-BUNDLE.md)。玩家 ZIP 保留运行组件、实际配置、L4N 配套 VDF 模板与 QC/VMT 范例、安装说明与合并归属文本；文档、JSON、SDK、诊断脚本及离线 Mod 制作工具留在仓库。加载设置插件本身不改配置或执行 GC，需玩家主动操作；不需要菜单时可退出游戏后移走该 DLL。
- 保留精确源码/暂存区校验、构建指纹、发布草稿重试及附件不可覆盖保护；增加三程序共同 build ID、PE 架构和 x86 Host LARGEADDRESSAWARE 验证。

## 历史：v1.2.2 验证与采用决定

生产补丁 SHA-256 为 `22a7bd685a2e6ed701215e3a5cadad079d1d70053bd9bfc589e3919869324367`，分别对 NVIDIA 原始基准 `9aa74f8dfad2188efbd0f717c64d9f8fa909787e`、`5fd30eae2d397f369bdf5660f3ddd0bc0bd1fa58` 和构建源 `3a75b814600bdf7ef7b0d3a0bae55b9a8c81f34b` 通过应用校验，所得 Bridge 源码树一致。

本地 x86/x64 数据环、Device/Module 双向 IPC、回绕/满载/失败、资源生命周期及 Nightly 既有回归已通过。测试使用的 v1.2.1 候选来自 `a831bcff1174abb93f0d12db517b06a18f3caa9e`；对应[验证构建 38108732058](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/actions/runs/38108732058)完成编译、测试和包校验。

2026-10-11 本机一轮对照中，开灯平均 FPS 由 400.37 降至 367.58（−8.19%），1% low 由 229.18 至 229.65（+0.21%，未建立明显收益），0.1% low 由 137.26 降至 131.79（−3.98%）；关灯平均 FPS 下降 8.00%。两方向官方地图切换均约 10～11 秒，本轮未复现上游另报的约 20 秒额外加载时间。维护者在了解这些结果后明确采用上游 1.2.2 的 IPC 正确性修复；这不把更新改称性能优化，也不证明长期稳定性。

以上游戏数据来自采用相同生产补丁的 v1.2.1 候选。v1.2.2 最终包会因版本/构建标识产生不同二进制，仍需完成对应 Actions 和附件校验；不能将候选实测写成最终发布包已重新游戏复测。本轮不追加游戏采样。PR #7 仅作后续独立可行性评估，未纳入此完整同步基准。

## 此前同步的验证历史

2026-10-10 的[手电专项对照](FLASHLIGHT-PERFORMANCE-2026-10-10.md)及更早 L4N 回放属于此前版本，不能替代本次 IPC 更新的测试。此前完整同步曾通过 164 项本地 Python 回归及 GCC x86/x64 的 Volume、Reset、PageBlock、配置、readback/adapter 测试；下述构建故障与修正仅保留为历史记录，不描述当前构建状态。

此前完整合并的首轮 MSVC 构建 `38020218495` 完成各主程序编译，但本地 Reset 生命周期回归的链接命令缺少 `user32.lib`，未发布。随后为正向与负对照补齐该依赖，并严格要求负对照返回预期的断言失败码，整合包与修正共同重新构建。

后续构建 `38021878907` 通过完整编译、上游原生回归和 Reset 测试，在本分支三维纹理分配故障夹具失败，未发布。夹具需显式拦截上游改用的 `nothrow new[]`，并区分协议容量与 MSVC x86 数组长度限制；普通分配故障仍必须实际进入拦截器，超限数据仍必须在分配前拒绝。这项修正只调整测试适配，不修改游戏运行补丁。

之前的选择性移植与延期决策保留为 [历史记录](ORIGINAL-PROJECT-UPDATES-HISTORY.md)，已不再限制后续完整同步。
