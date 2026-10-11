# 静态 VB/IB 按需映射实验

基于完整 IPC v2＋VA allocator。保留四方向 v2、C1/C2/C4/C8/C16/C32，以及动态 VB/IB 的原有存储、Lock/Unlock 和上传。仅非 DYNAMIC、大小达到阈值、非 SharedHeap 的 VB/IB 使用新的 section 存储。FULL_SHADOW 的完整历史内容仍保存；减少的是解锁后的 x86 映射，内容没有转移到 Host。

默认实验设置（旧 bridge.conf 无这些键也生效）：

```ini
client.vbibMappedShadow = True
client.vbibMappedShadowMinKB = 256
client.vbibShadowCacheMB = 32
```

最小阈值下限 64 KiB；默认 256 KiB 用于限制大量小 section 的对象/地址粒度开销，不表示这个阈值已被证明最优。缓存预算上限 1024 MiB；0 为最后一个 Lock 结束后立即解除映射。纹理的 surfaceShadowCacheMB、PageBlock、retention DB、DXVK 与 IPC 容量不变。缓存预算只约束未锁定的 VB/IB 映射，活动 Lock 可以超出预算。

创建 section 保留 Windows 零初始化与创建失败的 bad_alloc 边界；首次 Lock 才映射。重叠 Lock 共用一个稳定视图，FIFO Unlock 保留；只有最后一个 Lock 结束且 serializer 已复制上传数据后才允许缓存/解除映射。普通部分写入、READONLY 和 alwaysCopyEntireStaticBuffer 保留旧内容和原有 wire 布局。失败提交保留 Lock 及视图，不释放仍被引用的数据。映射失败返回 E_OUTOFMEMORY，不发布指针；映射重试会先淘汰空闲缓存，不强制解除活动 Lock。Reset 和 COM 销毁沿原有路径；有锁销毁仍按原资源生命周期释放存储。

热映射命中仍有 acquire/release 的缓存 mutex 与计数成本；冷访问增加 MapViewOfFile、UnmapViewOfFile 和可能的缺页；创建/销毁增加 section 句柄操作。静态 Usage 不是低频证明，LRU 不预测未来访问频率，频繁但超过缓存预算的工作集仍可能反复映射。没有新增 Host 取回 RPC、后台线程或资源恢复 generation 状态机。内核 section backing 仍消耗系统提交量；这是 AV 方案，不是消除 CPU 副本或减少整个系统 RAM 的保证。

client.memoryMonitoring=True 时，原有 vertex/index 字节字段继续表示逻辑 FULL_SHADOW 存活量；新增 `vbib_section_backing_bytes`、`vbib_section_count`、`vbib_mapped_va_bytes`、`vbib_mapped_count`、`vbib_idle_view_budget_bytes`、`vbib_active_locks`、maps/unmaps 与失败计数。用 va_free、largest_free 和这些新字段核对真实效果，不能把 backing 字节视为 Client VA。关闭监控不扫描 VA、不记录日志；缓存自身必须维护安全与预算计数。

Windows 成本对照使用冻结的原生产 LockableBuffer 和新生产模板，相同 MSVC 14.29 Release /O2、相同调用与上传捕获适配器。比较 old、新实现禁用、零缓存和 32 MiB 缓存；64 KiB/256 KiB/1 MiB/8 MiB，动态/静态热点、完整创建上传销毁、加载后资源存活空闲。外层批次计时，不逐调用计时；原始结果保存，不能推导游戏 FPS。真实 IPC 回归另验证 Client 解除映射后，慢 Host 仍读取正确的已复制 payload。

## 安装和 A/B

配套包只包含 x86 Client、x86 Host、x64 Host，以及构建/校验、许可证与说明。退出游戏和全部 Host，备份并成套替换 `bin/dxvk_d3d9.dll`、`bin/.l4d2bridge/L4D2Bridge32.exe`、`bin/.l4d2bridge/L4D2Bridge64.exe`。保留原 DXVK、配置、ReShade 和 retention DB；自定义 Client DLL 名称按实际 Bridge 文件更新。三端必须为同一 ldb-vbib-map-dev 构建。

保持同一个 IPC 档位与 Host 位数：先用上述默认设置，再将 client.vbibMappedShadow=False 重启比较。这一关闭开关恢复原 VB/IB allocator，IPC v2 不变。还可以单独测试缓存 0，观察极限 VA 回收及冷映射代价。回退时成套恢复三个备份，不混用版本。

实机记录 Avg FPS、1% Low、0.1% Low、帧时间、Client AV、最大连续空闲区及加载时间；覆盖重度 4K Mod、枪店↔训练图、烟花/粒子、长时间游玩与 Reset。原生回归和构建通过不等于游戏性能及稳定性已验收。本分支保持实验/Draft，不修改 main、正式 VERSION、Tag 或 Release。
