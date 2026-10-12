# LDB v1.3.0-pre.1 — IPC v2 预发布

这是当前实验分支的配套升级包，保持 Draft PR，不合并 main，也不替代最新正式版。默认采用 **C8**，默认**关闭静态 VB/IB 按需映射优化**。原 FULL_SHADOW、动态缓冲 Lock/Unlock 和历史内容语义保留。

包含完整 IPC v2 四方向主传输、独立上传/长 Lock lease、已有 VB/IB VA allocator，以及可手动开启的静态 VB/IB section 映射实验。Reply Slot 尚未实现；关闭映射开关不会切回 PR6 IPC。

## 下载、安装与回退

下载 Release 附件 `l4d2-bridge-patch-v1.3.0-pre.1.zip`，不要把 GitHub 自动生成的 Source code ZIP 当作安装包。用 `SHA256SUMS.txt` 校验压缩包；准确编译提交、三端构建标识和文件校验值见 `BUILD-INFO.json`。

1. 完整退出游戏及所有 Bridge Host，备份下面三个文件和现有 `bridge.conf`。
2. 将包里的 `bin/` 合并到游戏目录，成套替换：
   - `bin/dxvk_d3d9.dll`：x86 Client。
   - `bin/.l4d2bridge/L4D2Bridge32.exe`：x86 Host。
   - `bin/.l4d2bridge/L4D2Bridge64.exe`：x64 Host。
3. 保留 DXVK、ReShade、配置、L4N 插件和 retention DB。本包不包含、不覆盖这些内容。若 Client 原来使用其他 DLL 文件名，按实际加载的 Bridge 文件更新，避免覆盖 ReShade。
4. 检查 Client 与所用 Host 日志的 `Project build` 为相同的 `l4d2-1.3.0-pre.1+…`。`IPC_FULL` 应显示 `legacy_data_ring=0 reply_v2=1 upload_leases=1` 及选用档位。
5. 回退时退出所有进程，成套恢复三个二进制及需要回退的配置值。不要混用不同构建的 Client/Host。

## 默认值与已有配置

**旧配置中的显式值优先于新默认值。** 本包保留旧配置，因此之前的 `client.ipcBatchSize=32` 或 `client.vbibMappedShadow=True` 不会自动改写。希望使用本次默认策略时，请在实际使用的 `bin/.l4d2bridge/bridge.conf` 中编辑现有项，避免重复键：

```ini
client.ipcBatchSize = 8
client.vbibMappedShadow = False
```

完整重启游戏与 Host 后生效。已有 Host 位数选择保持不变；C8 并不强制使用 32 位或 64 位 Host。

| 配置 | 本版默认 | 用途 |
| --- | --- | --- |
| `client.ipcBatchSize` | `8` | 已有档位开关，支持 1/2/4/8/16/32；其他值回退 C1。 |
| `client.vbibMappedShadow` | `False` | 新实验开关；True 才启用符合条件的静态 VB/IB 按需映射。 |
| `client.vbibMappedShadowMinKB` | `256` | 新实验最小资源大小，单位 KiB，下限 64 KiB。 |
| `client.vbibShadowCacheMB` | `32` | 新实验空闲映射预算，单位 MiB，上限 1024 MiB；0 为最后 Unlock 后立即 unmap。 |

本次没有再增加配置开关。后三项来自按需映射实验；默认关闭时阈值和缓存预算不参与普通 VB/IB 分配。它们不会关闭纹理 Shadow、改变 `client.surfaceShadowCacheMB` 或调整 IPC 上传池容量。

## 性能与验证边界

C8 是本次选择的默认档位，不代表所有机器和场景的最优值。可在相同场景下比较 C1/C2/C4/C8/C16/C32，记录 Avg FPS、1% Low、0.1% Low、帧时间、Client AV、最大连续空闲区及进图时间。

按需映射默认关闭时沿原 FULL_SHADOW 和 VA allocator 路径，仍存在新实现的少量指针判断与元数据，不能声称与旧二进制完全零差异。开启后保留完整内容的 section backing，占用系统提交量；热缓存需要锁与维护，冷访问增加 Map/Unmap 和潜在缺页。原生成分基准显示这些代价可能影响加载或高频访问，因此保留为手动实验选项，不保证重度 4K Mod 的 AV 或 FPS 改善。

发布流程先运行现有原生正确性、资源生命周期、六档 x86/x86 与 x86/x64 真实 serializer/队列回归，再完整编译三端 Release，校验位数、匹配版本、x86 Host LAA 和 SHA-256。准确结果见对应 Actions 运行与随包构建信息。通过这些检查不等于本次新默认值已通过游戏 GPU 实机验收；不宣称尚未测量的 FPS 收益。

建议覆盖训练图↔枪店图切换、烟花/粒子、长时间游玩、窗口切换和 Reset。IPC 会话故障后需要完整重启游戏与 Host。Steam Overlay/Shift+Tab 等既有未解决问题不会因这次预发布自动修复。
