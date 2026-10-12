# VA allocator + 完整 IPC v2 主传输实验

分支：`codex/ipc-v2-full-integration`。基于 PR7 的生产实验分支 `codex/ipc-c32-development`（中间档位源码 `d59493c`），保留已验证的 VB/IB VA allocator。构建、原生测试和对照由 `.github/workflows/ipc-full-development.yml` 执行；游戏验收仍待完成。准确源码提交、构建 ID 和三端 SHA-256 以测试包 `BUILD-INFO.json` 为准。本分支不修改正式 VERSION、main、Tag 或 Release。

## 移植范围

| 路径 | 原生产实验版 | 本分支 |
| --- | --- | --- |
| Device / Module Client→Host 参数 | v2 消息块 + PR6 Data Ring | v2 消息记录，适合的参数/Blob 内联 |
| 上传 / 长 Lock 数据 | PR6 Data Ring 与 pin 前沿 | 独立有界上传 lease，不占用长期消息块信用 |
| Host→Client 回复 / Readback | PR6 Header / Data 队列 | 两条反向 v2 流，保留原 UID 和调用接口 |
| 消费确认 | 消息块 + Data Ring 两套前沿 | 消息块确认；外部 lease 独立释放 |
| VB/IB Shadow | FULL_SHADOW | FULL_SHADOW + 原 VA allocator |
| Reply Slot | 无 | 无；回复通过反向流传输 |

四个方向均使用固定 ABI 的消息流，每个流 64 × 4 KiB 消息块。生产传输不再分配 PR6 Header/Data Ring，也不再依赖其 published/consumed/pin 前沿。记录携带固定宽度元数据；上传描述符为 16 字节，包含 slot、generation、offset、bytes。消息和上传池均校验归一化源码标识，配套二进制必须一起更新。

Host 消息处理、D3D9 dispatch、DXVK、PageBlock/LG/GC、资源持久 Shadow、Lock/Unlock 上传内容保持原有路径。Host 的 reserved VB/IB Unlock 使用已验证的 lease 范围解析指针，然后沿原有 backend Lock→复制→Unlock 顺序处理。消息完成代表 Host CPU 已停止读取，不代表 GPU fence 完成。

## 生命周期与失败边界

完整已知的 Packet/Surface 布局先验证总长度，再预留上传跨度；大 payload 不经过消息块进行二次搬运。当前实现仍可能先开启一个内联 Encoder 再转为外部描述符，不声称已消除所有 reservation/检查。未知布局可能增长或迁移前缀。借出的指针一直保持稳定，不能因增长移位。

普通已发布 payload 不再改写。优化动态 Lock 是明确的例外：Lock 发布后 Client 仍可写借出的区域；Host 在 Lock 时只保留 lease，实际读取等待后续 Unlock 的有序发布。最后一个消息引用和资源 pin 均释放后才允许复用。原有同一资源的 FIFO pin、Unlock/Destroy 释放路径继续使用；不增加资源 ID generation 镜像。

每个上传池使用原通道内存预算，4 KiB 页、1024 个 lease slot，元数据 65,600 字节。池容量按页向下取整；新元数据会减少相同预算下的可用 payload。连续空间不足、slot 用尽或者借出区域无法原地增长时，采用现有 2 秒实验背压预算；失败终止整个会话。**总空闲空间足够仍可能因碎片化失败，1024 个未释放 lease 也是独立上限。** 这些是实机验证需要关注的新增限制。

跨线程请求保留 Client 的原序列化 mutex。反向队列以 UID 区分等待者；消费正文和包状态退出后才归还信用。空回复、握手、排空等待和已有超时/失败处理均保留。故障联动四条流并唤醒等待者；冷等待会观察 Bridge 停止和晚注册的 peer PID。没有为每次 Lock 添加新的同步 RPC。

## 六档配置

在现有 `bin/.l4d2bridge/bridge.conf` 中只编辑这一项，重启游戏和 Host：

```ini
client.ipcBatchSize = 4
```

支持 **1、2、4、8、16、32**，对应 C1/C2/C4/C8/C16/C32；默认和不支持的值采用 C1。档位只在通道初始化时校验。已有 setter/draw 白名单可以延迟提交；其他命令、同步等待、外部 lease、Present、Reset、资源创建/销毁和 Lock/Unlock 都强制提交。

保留 1 ms API 入口延迟检查，没有后台提交线程。游戏不调用 API 时没有硬性 1 ms 提交保证。C1 仍使用完整 v2 协议，只改变提交频率；切换档位不会切回 PR6。

## 测试与对照

现有测试复用生产模板和实际 serializer，支持 x86 Client→x86/x64 Host。六档各执行三轮、九类负载：小参数流、混合 Blob、线程写入顺序、大 Surface 上传、边界与容量极限、小 Surface 通用/声明布局、并发 UID 回复、启动/退出空回复握手；总计 324 个配对进程。另有六档提交失败测试，以及两种位数的上传池满载、长 pin、代际、坏描述符、回绕、等待故障和停止中断测试。

既有 v2 核心 native 测试继续验证双向 Device/Module、2 MiB 数据、事件/自旋、慢消费者、多线程及 peer 退出。现有 Buffer、VA allocator、Volume、Reset、Readback、资源生命周期、PageBlock、输入和诊断等回归继续执行。旧裸指针测试适配复用 VA 分支已验证的 Storage 接口；没有删除原有内容或失败断言。

性能基线固定为中间档位更新 `00007e8`（生产 `d59493c`、构建 `ldb-ipc-c32-dev+ec19f9c2780fa8c1`）。两种实现使用同一测试调用源、MSVC 14.29 `/O2 /DNDEBUG`、2 MiB 通道预算、相同消息块几何、相同 RPC/Host 延迟。六档 × 两种 Host × 两种负载，各一轮预热和五轮交替 A/B，共 288 个配对进程。

采用外层批次计时和 `QueryThreadCycleTime`，不逐调用计时。保留全部原始样本和失败，并输出每轮配对比值。旧实现与 lease 池的元数据占用不同，真实可用容量同时记录，不给新实现额外 payload 容量。Host 每 1024 次调用 Sleep(1)，每 101 次有 RPC；整体耗时包含这些等待。混合负载三分之一是 512 字节上传。**这些数据不能直接推算游戏 FPS、帧时间或 AV 收益。**

## 安装、备份与回退

这是已有 Bridge 的配套更新包，包含三个二进制与构建/校验信息，不包含 DXVK、ReShade、配置或 retention DB。

1. 退出游戏和全部 Bridge Host。备份以下三个文件。
2. 校验 ZIP SHA-256，仅将包中的 `bin/` 合并到游戏目录，替换：
   - `bin/dxvk_d3d9.dll`：x86 Client。
   - `bin/.l4d2bridge/L4D2Bridge32.exe`：x86 Host。
   - `bin/.l4d2bridge/L4D2Bridge64.exe`：x64 Host。
3. 保留现有 DXVK、ReShade、配置和 retention DB。若自己的安装把 Client 放在其他 DLL 名称，按实际已加载 Bridge 文件更新，避免覆盖 ReShade。
4. 检查 Client/Host 日志为同一个 `ldb-ipc-full-dev+…`。两条 Client `IPC_FULL` 启动记录应显示选择的 batch，以及 `legacy_data_ring=0 reply_v2=1 upload_leases=1`。
5. 回退时退出全部进程，成套恢复三个备份；不要混用不同构建的 Client/Host。

实机先以 C1 为参照，再比较 C2/C4/C8/C16/C32。固定地图、路线、Mod、Host 位数及其他配置，交替重复测试，记录 Avg FPS、1% Low、0.1% Low、帧时间、Client AV、最大连续空闲区、进图时间和日志。覆盖训练图↔枪店图反复切换、烟花/粒子上传、长时间游玩、窗口切换和 Reset。性能与稳定性通过实机验收前，保持实验分支和 Draft 状态。
