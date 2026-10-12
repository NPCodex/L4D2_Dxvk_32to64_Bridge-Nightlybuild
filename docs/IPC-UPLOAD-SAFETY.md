# IPC 与 Host 上传最小正确性修复

基线：正式 v1.3.0，`f6a059b758f9fcbf9c66ace73625aae9db34ad6a`。
本分支是测试修复；保持完整 IPC v2、默认 C8，以及关闭 `vbibMappedShadow` 的设置。
不修改正式版本、main、Tag 或 Release。

## 问题与处理

| 问题 | LDB 源码依据 | 本次处理 |
|---|---|---|
| Release 中后端 Lock 失败仍复制 | Host `main.cpp` 的 VB/IB、普通 Surface、Volume/VolumeTexture 上传仅依赖 assert | 显式检查 HRESULT；失败不复制、不 Unlock；借用上传 pin 正确释放；成功 Lock 后即使布局/Unlock 失败也关闭会话，阻止后续提交 |
| x64 小 mip 复制宽度错误 | 普通 Surface 使用 `calcRowSize()`，其下限是 `sizeof(size_t)`；x86 发送 4 字节时 x64 可能复制 8 字节 | Host 复制逻辑像素/压缩块字节，源步长与目的步长分别处理；保留逐行复制与 padding |
| 其他 UID 的回复耗尽自己的预算 | `Bridge::waitForCommand()` 等待已经存在的其他 UID 回复时仍累计 commandRetries | 只在已知属于另一 UID 的同命令回复上保留重试预算；保持硬截止、取消及失败后禁止提交 |
| 关闭时遗漏最后发布批次 | 完整 MessageQueue 与 Reader 的等待谓词先读取 published，再观察 producerClosed | 冷路径返回关闭前重新读取发布游标，消费已发布尾包后才结束 |

失败处理仍遵循已有 fail-closed 契约。异步 API 先前已返回的结果不能被追溯修改；Host 发现失败会终止会话，而不是伪造上传成功或继续运行。

## 成本和边界

普通已匹配 UID 的回复热路径不增加锁、容器或状态；关闭修复仅在等待/关闭冷路径多读取游标。
上传增加标量布局、指针溢出及 HRESULT 检查，不增加新复制、CPU Shadow、扫描内存、同步往返或常驻状态。
没有加入 NB 的 Upload Pool first-fit、紧密布局一次 memcpy、Query 重构或独立回复队列。
SharedHeap 保留原有分配范围契约；本次指针/算术检查不等于能够证明每一个 SharedHeap 分配的真实大小。
ATI1/ATI2 的专用上传实现不改动，继续运行原有回归。
尾包缺陷可在真实队列组件重现，但生产通道通常保持进程生命周期；不能据此认定之前的切图故障由尾包引起。

## 可重复验证

准备源码后运行 `scripts/test_ipc_upload_safety.ps1`，并运行现有 `scripts/test_ipc_full.ps1` 及完整构建工作流。

- Host 用例提取五个完整生产 dispatch case，只替换后端对象类型；MSVC 14.29、`/O2 /DNDEBUG`，x86/x64，packed 与逐行 Volume 两种编译布局。
- 覆盖失败 Lock、成功但空指针、Unlock 失败、保留 pin、零字节缓冲上传、1×1 ARGB/RGB24/DXT1/DXT5、部分矩形、行/切片 padding、短 payload、负/不足 pitch、逐行解码异常后释放 Lock。
- 完整队列 24 个、Reader 32 个确定性关闭竞争场景：Device/Module、双向、游标回绕、不同尾包数与 spin 设置；数据和消费确认必须恰好一次。
- 真实生产序列化器跨进程回复用例：低重试预算、默认 300 次预算与 3300/6500ms owner 暂停、180ms abandoned deadline、取消；覆盖 x86 Client→x86/x64 Host，默认预算仍保留原来的 10 秒硬截止。
- 冻结 v1.3.0 在同样测试下必须暴露失败 Lock、x64 小 mip、两个尾包竞争及默认回复预算缺陷。旧队列只加入测试调度 hook，不改逻辑；不得把编译失败当负对照通过。

本地 Linux 64 位：40 项 Python 检查、现有完整队列/core 原生用例以及两种尾包负对照通过。Windows 构建与全量原生结果由该分支 Actions 提供；实机游戏与 FPS/AV 收益未验证。

首次 Windows CI：360 组现有跨进程测试、24 项失败封闭测试、新增上传与尾包正负对照、八组回复正向用例通过，但旧版 3300ms 回复暂停也通过，因此整项检查正确地报失败，没有将旧版通过当作负对照成功。该平台的 `Sleep(10)` 粒度使默认重试耗尽时间超过 3300ms；保留这条正向观测，再以相同默认预算、6500ms 暂停（仍低于 10 秒）进行旧/新对照，另保留低预算负对照。测试结果不用于宣称游戏收益。

## 来源与代码归属说明

参考 [NPCodex Nightly Build](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild) 的
[`9724c007`](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/commit/9724c0074d4420dafe094c25e6b1bddb8dc16f91)（上传/并发回复问题）及
[`c2e76940`](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/commit/c2e7694060aa44747adb5a5b6a83d5f8be779030)（尾包问题）。
尾包用例从 NB 的 MIT 标注测试改编，保留来源标注；延迟回复用例参考 NB 并增加真实默认预算与取消覆盖。
生产修复按当前 LDB 契约做最小适配，没有整体移植 NB 的优化套件。新增 LDB 适配与实现由 OpenAI Codex 根据 yeyunyyds 的规格生成；原有上游及参考代码的归属不变。
