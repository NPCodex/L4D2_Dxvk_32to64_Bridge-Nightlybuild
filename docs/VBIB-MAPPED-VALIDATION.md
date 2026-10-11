# 静态 VB/IB 按需映射：可行性与生产接入验证

## 结论

已将非 DYNAMIC、非 SharedHeap、达到大小阈值的 VB/IB 接入生产 section 存储。内容仍是 FULL_SHADOW，只有未锁定的视图可以解除映射；不是删除历史字节，也不是 Host 回传架构。默认阈值 256 KiB、空闲视图缓存 32 MiB，动态/小缓冲仍使用原 VA allocator。

Windows 原生负载证明了机制可释放 x86 VA，但成本取决于访问方式：缓存命中增加几十 ns；冷映射增加数 µs；超过缓存的循环访问会显著增加 CPU 成本；大资源创建/完整写入也可能增加加载时间。因此提供可开关的配套实机包，保持 Draft，不宣称重度 4K 用户已获益或游戏 FPS 无损。

结构、配置、安装与回退见 [VBIB-MAPPED-DEVELOPMENT.md](VBIB-MAPPED-DEVELOPMENT.md)。实际结果和原始样本位于 [vbib-mapped-evidence](vbib-mapped-evidence/)。

## 实际调用链与语义

`CreateVertexBuffer/CreateIndexBuffer → 既有 Client wrapper → LockableBuffer::initShadowMem → SectionShadow 创建 section`。正常 Lock/Unlock 仍经过原 API/device 序列化；选中资源在 `acquire()` 映射，返回同一视图的 offset 指针；Unlock 保持原字段、有效范围和普通 payload 复制。`ClientMessage::finish()` 成功后退出锁记录，再释放一层映射 pin。最后一个 Lock 结束才进入 LRU 或解除映射。

部分写入与 READONLY 直接使用保留的 section 字节，重叠 Lock 共用视图且 FIFO Unlock 不变。alwaysCopyEntireStaticBuffer 继续发送完整原内容。提交失败不弹出锁记录、不解除源视图；真实 IPC fatal fault 仍终止会话，单元测试中的可控重试不代表支持故障后重连。映射失败返回 E_OUTOFMEMORY 并清空输出指针；section 创建失败沿 bad_alloc 路径传播，不静默回退到大堆副本。

COM 引用、父子关系、StateBlock/绑定保活、Reset 及资源销毁沿既有路径；section 随实际 wrapper 生命周期关闭。缓存没有 Host 状态镜像、资源 generation 表或后台线程。每个 wrapper 增加一个可空 owning pointer，选中资源有一个约数十字节的存储对象及一个 section 句柄；缓存复用对象内的 intrusive 链接，操作为 O(1)。静态选中资源 acquire/release 使用一个全局缓存 mutex；动态路径只保留可空指针判断，不执行映射或该缓存锁。

缓存按 Windows 分配粒度计费，mapped VA 另用 VirtualQuery 查询真实视图区间，两者不混淆。活动 Lock 可以超出空闲预算。Map 失败会先清空可淘汰的空闲缓存再重试；不会回收活动指针。Unmap 失败保留视图且记录失败，不把未归还的 VA 计为成功释放。

生产改动仅在 `patches/vbib-mapped.patch` 的 client_options.h、lockable_buffer.h、memory_diagnostics.h，及新 `experiments/vbib-mapped/buffer_mapped_shadow.h`。未改 IPC wire、Host dispatch、DXVK、纹理 PageBlock/GC、retention DB、ReShade 或正式 VERSION。保留完整 v2 主传输和六个 batch 档位。

## 成本测试方法

原模板来自已验证的完整 v2 构建 `ldb-ipc-full-dev+9f30cbf7ddabdb95`、提交 `759c8e8` 对应生产源。准备脚本在应用映射补丁前保存原模板，并验证基础构建标识。编译原/新生产 LockableBuffer，使用相同 COM/上传捕获适配器；没有删除新实现的检查或合成原分配算法。

MSVC 14.29 `/O2 /MT /DNDEBUG /LARGEADDRESSAWARE`，x86 实际 Windows API。四种情况为原模板 old、新模板禁用 off、启用零缓存 zero、启用 32 MiB 缓存 cache；共 24 个交替进程，各一轮预热、五轮实测。每轮覆盖 64 KiB/256 KiB/1 MiB/8 MiB、静态/动态热点、64 MiB 循环工作集、完整创建上传销毁和加载后存活空闲。64 KiB 在默认阈值下未进入映射，用于控制检查。QueryThreadCycleTime 和墙钟均在批次外测量，不逐 API 计时。所有样本保留。

这是实际存储及 LockableBuffer 的组件测试，不含真实 IPC/Host/GPU、外层 D3D9 wrapper 的全部工作，也不能推算 FPS。上传捕获适配器保留相同字节复制，但与生产 DLL 的分配及内联条件不同。VA 采用完整 VirtualQuery 扫描，进程变化包含少量适配器及 CRT 保留；映射/section 的核心计数另外报告。

最终完整通过的 run [38183934718](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/actions/runs/38183934718) 的成本如下。前一独立 run [38183263654](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/actions/runs/38183263654) 也完成全部跨进程与原生回归，成本相近；其最后被 Python 字符串误判阻断，未产出测试包。误判把 allocated() 通知匹配成禁止的 allocate()；修复保持分配函数禁令，并增加独立匹配回归。两轮样本均保留，失败没有计入性能成功样本。

| 资源大小 | 原静态热点 | 32 MiB 缓存命中 | 零缓存热点 | 64 MiB 工作集 / 32 MiB 缓存 |
| --- | ---: | ---: | ---: | ---: |
| 256 KiB | 111.2 ns | 157.7 ns | 5.07 µs | 5.54 µs |
| 1 MiB | 107.5 ns | 158.1 ns | 6.37 µs | 6.65 µs |
| 8 MiB | 107.9 ns | 157.4 ns | 6.65 µs | 8.52 µs |

同一轮中，8 MiB 资源创建→完整写入→上传捕获→销毁：原路径约 3.48 ms，32 MiB 缓存版本约 5.37 ms，每资源多约 1.89 ms。其他独立 run 的绝对值有变化，不能把单轮比例当作所有机器的常数。两轮缓存命中相对新实现禁用约多 46～49 ns；动态热点的少量包装/布局差异约为数 ns，不称零代价。

64 MiB 循环访问大于 32 MiB 缓存，各访问可发生重新映射。静态 Usage 不能证明低频，LRU 也不能保证工作集会命中。若每帧有 100 次此类冷访问，按组件测试 5～9 µs 估算会增加约 0.5～0.9 ms CPU 工作；这只是条件成本模型，不是实机测得的帧时间或 FPS 损失。

## VA 证据与边界

保留 128 个 1 MiB 静态缓冲，加载上传完成后全部仍存活：原路径的进程 VA 增量约 133 MiB；32 MiB 缓存版本约 32 MiB，零缓存版本接近零映射增量。16 个 8 MiB 缓冲的原路径约 129 MiB，32 MiB 缓存约 32 MiB。section backing 仍为 128 MiB，数据重新映射后逐字节校验通过。

128 个 256 KiB 缓冲合计仅 32 MiB，默认缓存允许全部留下，此负载没有明显 AV 收益。64 KiB 控制组仍走原分配器。最大的连续空闲块在这些负载中没有随总 AV 同幅增加，跨进程 ASLR 基址也不同，不将它宣称为已改善。游戏必须同时测 va_free 与 largest_free。

重度 4K 用户若主要是纹理、引擎存储、小于阈值或动态 VB/IB 压力，本方案可能没有足够覆盖。先看 `vbib_section_backing_bytes` 与 mapped/idle 字段；该值很小就不能期待大量 AV。section 不减少全系统提交量；资源 Lock 期间仍需要完整连续视图，无法承诺解决所有碎片化分配失败。

## 回归与交付

已经执行：x86/x64 section 核心（含并发、映射/解除映射失败、历史内容与销毁）、真实生产模板的 DEFAULT/MANAGED VB、IB16/IB32、部分写入、重叠 Lock、READONLY、全量静态上传、失败保持源及创建失败。复用现有完整 IPC 测试，新增慢 Host 模式后为六档 × 两种 Host × 十类负载 × 三轮，共 360 对；另有 24 个 fail-closed。新增模式通过真实生产模板和真实 serializer/共享队列校验 128 条 VB/IB 上传，Client unmap/销毁后慢 Host 仍获得完整正确内容。

现有 20 个原生脚本组（含 Buffer/arena、Volume、Reset、Readback、生命周期、PageBlock、诊断和 x86 backend loader）保持全部运行，PageBlock 沿原 SkipPluginTests 方式。Python 合约增加精确名称匹配用例，现为 40 项。没有删除内容断言、改变原有受保护 runtime hash 或忽略测试失败。

- 分支 `codex/vbib-mapped-shadow`，编译提交 `eaeee728f2b09d71a213d0e113d66a8cd9f1d29b`。之后报告提交不改变生产指纹。
- 三端构建 `ldb-vbib-map-dev+0064e06ff547c453`，完整 IPC v2 ABI `0x00020004`，六档支持不变。
- [最终 CI 和构件](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/actions/runs/38183934718)，artifact `11719400512`：全部上述测试、40 项 Python 和三个实际 Release 构建通过。
- ZIP `l4d2-bridge-vbib-map-dev-0064e06ff547c453.zip`，SHA-256 `f40b0e248b661933f034a2dfd0e84f9015ddf25009557170df8f31cb3e9db8b3`。
- 构件已下载验证三端 SHA、嵌入构建 ID、PE x86/x86/x64、x86 Host LAA 及实际 Release /O2/NDEBUG 编译记录。包内不含配置、DXVK、ReShade 或 retention DB。
- <!-- DOWNLOAD_LINK -->

建议先以同一包、同一 IPC 档位比较 mapped=True/False，默认 cache=32；AV 阶段开启现有 memoryMonitoring，FPS 阶段保持双方诊断一致。只有目标用户的实际 AV、连续空闲区、FPS/低帧及加载时间综合收益通过，才考虑后续采用。当前仅用于实机验证，不合并或发布正式 Release。
