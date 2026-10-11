# 完整 IPC v2 + VA allocator：构建与验证记录

## 结论

已完成生产主传输移植：Device/Module 的 Client→Host 请求、Host→Client 回复均使用 v2 消息流；上传与长 Lock 使用独立 lease 池，不再分配生产 PR6 Header/Data Ring。保留 VB/IB VA allocator、FULL_SHADOW、D3D9 dispatch 和 DXVK 路径。支持 PR7 更新后的 **C1/C2/C4/C8/C16/C32**。

两次独立 Windows CI 全部通过，三端 Release 构建可用于实机验证。**性能验收尚未通过：小参数负载的 Client CPU cycles 出现可重复增加；不能据此宣称解决此前 15% FPS 下降。** 本版本保持实验性质，不合并、不发布正式版本。

“完整”指四方向主传输、回复、上传与 pin 生命周期接入，不包含 PR7 设想中的可选 Reply Slot。回复使用反向有序流。详细结构、六档配置、容量限制与安装回退见 [IPC-FULL-DEVELOPMENT.md](IPC-FULL-DEVELOPMENT.md)。

## 可复核的来源与构建

- PR7 生产实验基线：`00007e8762385704afdacabea6e48444d798d27a`，生产功能提交 `d59493c`；原 PR7 页面 head 的 prototype 不是本次性能基线。
- VA allocator：复用 `b90d056006f892765b0cc2ebfc895b406bda4725` 的三个生产文件，不改变 Shadow 内容或上传策略。
- 最终编译源码：`2064d38c1f9c576846da3b2d31f1cd769462e0d1`。
- 三端构建 ID：`ldb-ipc-full-dev+9f30cbf7ddabdb95`，协议 ABI `0x00020004`。
- 最终包：`l4d2-bridge-ipc-full-dev-9f30cbf7ddabdb95.zip`。
- ZIP SHA-256：`5c5043f09d9eb1eccf32463f46b6cd10fba1a9605ba02b8909c661f0f0c06c42`。
- 最终 [Windows CI / 下载构件](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/actions/runs/38129726737)，artifact `11698342747`。
- 首次完整通过的 [独立 CI](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/actions/runs/38128983338)，源码 `d0dd21c`，相同生产传输标识，artifact `11698201946`。最终源码仅更新安装/结构说明。

构件已下载复核：三个文件 SHA-256 均匹配 BUILD-INFO，PE 分别为 x86 Client、x86 Host、x64 Host；全部嵌入同一构建 ID，x86 Host 保留 Large Address Aware。实际编译记录验证 MSVC 14.29 Release `/O2 /DNDEBUG`。包内仅有配套二进制、构建/校验信息、许可证和安装说明，不含 DXVK、ReShade、配置或 retention DB。

## 正确性验证

每次成功 CI 均执行以下检查；没有删除原有内容断言或为了通过检查忽略失败。

| 检查 | 范围与结果 |
| --- | --- |
| 生产 serializer 跨进程 | 六档 × x86/x86、x86/x64 × 九种负载 × 三轮，共 324 对，全部通过；双通道参数/Blob、声明和未知 Surface 布局、容量边界、线程顺序、并发 UID 回复、零载荷启动/退出握手 |
| fail-closed | 六档 × abort、oversize、underfill、overrun，共 24 例，全部通过 |
| v2 核心与完整传输策略 | 两种位数；双向通信、慢消费者、回绕、peer 退出、上传池满载、长 pin、lease 代际与范围、坏描述符、信用/事件唤醒、停止中断等，全部通过 |
| 现有原生回归 | 20 个脚本组，包含 Buffer/VA、Volume、Reset、Readback、资源生命周期、PageBlock、输入、诊断及固定 DXVK x86 DLL 加载检查，全部通过；PageBlock 使用现有 SkipPluginTests 模式 |
| Python 合约检查 | 39 项，全部通过 |
| 实际产品构建 | x86 Client、x86 Host、x64 Host 均完成；包内仅包含这三端 |

生产调用链为 `Bridge API → Client 原序列化锁 / BridgeCommand → FullQueue 编码 → v2 块发布 → Host 解码 / 既有 dispatch → 反向 FullQueue 回复`。外部 payload 经 `UploadPool` 预留、发布、校验范围和消费；长 Lock retain lease，通过有序 Unlock 后的最后引用释放才允许复用。Host reserved VB/IB 指针解析是 dispatch 中仅有的两处适配，后端 Lock/复制/Unlock 顺序保留。

实现依据：`patches/ipc-full.patch`、`experiments/ipc-full/message_queue.h`、`upload_pool.h`，以及 `experiments/ipc-v2/transport.h` 和 `platform.h`。`scripts/prepare_ipc_full.py` 记录并校验基础补丁、C32/Full helper 和 v2 core 的归一化源码标识。

原生验证覆盖代码与失败边界；**没有运行 L4D2、真实 GPU 场景或整章游戏**，不等于枪店↔训练图已实机验收。测试中的 loader 检查也不等于 DXVK 渲染正确性验证。

## 性能方法与限度

两次 CI 各执行六档 × 两种 Host × 两类负载 × 两版本 × 六轮，共 288 对进程；每组一轮预热、五轮实测，交替 A/B。基线是上述最新 PR7 生产 C32 分支，**不是 v1.2.1，也不是 PR6**。使用相同调用源、编译器和 `/O2 /DNDEBUG`，外层批次计时及 QueryThreadCycleTime，未逐 API 计时。

两版本均给 2 MiB 映射预算。C32 可用上传容量 2,093,824 字节，Full 为 2,027,520 字节；没有给新实现追加容量来抵消 lease 元数据。每批有 Device/Module 各 12,000 次调用；mixed 三分之一为 512 字节上传。每 101 次有 RPC，Host 每 1024 次 Sleep(1)，并校验上传数据。这些等待使外层耗时主要反映等待，不能用于证明吞吐或 FPS 无损。测试适配器使用真实生产方法，但与测试调用编译在同一翻译单元，内联条件可能不同于游戏 DLL。

下表采用每轮配对变化的中位数：`(Full / 基线 - 1) × 100%`。正值代表增加 CPU cycles，即成本变高。两次运行的原始样本、基线/生产源码清单和摘要保存在 [ipc-full-evidence](ipc-full-evidence/)；所有失败样本也保留在对应 CI artifact。

| 档位 | Host | 负载 | Client 第一次 | Client 第二次 | Host 第一次 | Host 第二次 |
| --- | --- | --- | ---: | ---: | ---: | ---: |
| C1 | x32 | 小参数 | +4.81% | +9.39% | -3.37% | +2.63% |
| C1 | x32 | mixed | -0.47% | +6.48% | +11.52% | +7.96% |
| C1 | x64 | 小参数 | +10.14% | +8.07% | +2.44% | +2.08% |
| C1 | x64 | mixed | -6.42% | +1.89% | -4.94% | -6.19% |
| C2 | x32 | 小参数 | +3.74% | +6.64% | -5.55% | -2.34% |
| C2 | x32 | mixed | -1.24% | +8.15% | +9.33% | +7.41% |
| C2 | x64 | 小参数 | +4.65% | +10.42% | -4.20% | +2.51% |
| C2 | x64 | mixed | -5.08% | +1.34% | -5.99% | -5.04% |
| C4 | x32 | 小参数 | +4.97% | +11.29% | -6.64% | +4.19% |
| C4 | x32 | mixed | +3.19% | +6.96% | +10.80% | +7.97% |
| C4 | x64 | 小参数 | +7.39% | +8.16% | -5.52% | -1.80% |
| C4 | x64 | mixed | -3.40% | -0.65% | -3.61% | -6.81% |
| C8 | x32 | 小参数 | +5.15% | +10.60% | -5.81% | -2.30% |
| C8 | x32 | mixed | +3.21% | +7.29% | +11.65% | +8.80% |
| C8 | x64 | 小参数 | +11.69% | +14.90% | -8.96% | +1.47% |
| C8 | x64 | mixed | -5.02% | -0.22% | -5.08% | -8.36% |
| C16 | x32 | 小参数 | +7.48% | +7.53% | -21.78% | -15.05% |
| C16 | x32 | mixed | -1.33% | +3.69% | +11.02% | +9.47% |
| C16 | x64 | 小参数 | +16.14% | +22.47% | -14.30% | -12.34% |
| C16 | x64 | mixed | -2.82% | +1.78% | -4.13% | -7.77% |
| C32 | x32 | 小参数 | +5.96% | +8.87% | -23.45% | -14.17% |
| C32 | x32 | mixed | +2.22% | +5.61% | +11.98% | +7.89% |
| C32 | x64 | 小参数 | +13.18% | +15.55% | -9.53% | -7.67% |
| C32 | x64 | mixed | +1.26% | +2.74% | -1.29% | -5.46% |

两次运行均显示全部六档的小参数 Client 成本增加，合计范围 **+3.7%～+22.5% cycles**。Host 小参数在较大档位往往降低成本。x64 mixed 的 Client 变化在运行间不稳定，不能宣称稳定收益；x86 mixed 的 Host 在两次运行中均增加成本，约 **+7.4%～+12.0%**。Client 外层时间变化范围第一次约 -0.18%～+0.74%，第二次约 -0.11%～+0.50%；由于上述等待，不将其解释为游戏性能持平。

已知新增成本包括 Client 命令进入的 atomic exchange、标量与布局检查、inline 转外部 descriptor 时的暂定 Encoder，以及读取端 peek/finish 的 mutex；外部上传另有页分配、lease 描述符、代际检查与释放。**尚未逐项隔离归因**，不声称其中任何一项解释了特定比例的回退。本次先完成完整接入与诚实对照，未继续删除验证或调整基准来追求通过。

## 尚待实机确认

上传池有 1024 slot、连续页与指针稳定性限制；总空闲足够仍可能因碎片化或借出区域无法增长而失败。池内元数据占用减少可用 payload。保留既有上传和持久 Shadow，本次不能承诺新增 AV 收益，也不属于取消 Shadow 架构。

实机先用 C1，再测试 C2/C4/C8/C16/C32。相同地图、Mod、路线、Host 位数与其他配置下交替重复运行，记录 Avg FPS、1% Low、0.1% Low、帧时间、Client AV、最大连续空闲区与加载时间。重点覆盖枪店↔训练图切换、烟花/粒子、长时间游玩、Reset 和资源回收。三端必须成套替换；回退成套恢复备份。实机性能及稳定性验收前保持 Draft，不合并。
