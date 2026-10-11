# 高频标量命令的一次整包编码

本候选在 v1.2.2 / IPC 3 上优化 `SetRenderState`、`SetTextureStageState`、`SetSamplerState`、`DrawPrimitive` 和 `DrawIndexedPrimitive`。使用已有 `bridge_data::fields` / `ClientMessage` 整包构造，把 UID 与参数的两次预留合为一次。只改 Client 的五个发送作用域；Host 逐字段解码、命令核心及线协议不变。

保留原参数检查、设备锁、state block 记录、状态去重与 dirty 标记、可选回复等待、错误转换和作用域结束时即时提交。没有过滤新的命令、跨 API 批处理、放宽所有权检查、修改后端或默认配置，也没有恢复 v1.1.13 的绑定引用优化。

本次验证从真实生产源码提取发送作用域，通过实际 Command、serializer、共享内存和队列与旧字段序列比较，并交给原语义的逐字段 Host 解码。覆盖 UID、flags、句柄、负 BaseVertexIndex、极值、立即可见、回绕、满队列失败及失败后停止。

完整补丁已在 NVIDIA `9aa74f8`、`5fd30ea` 和 `3a75b81` 基准上验证，应用后的 Bridge 树一致；与原补丁相比只改变 `client/d3d9_device.cpp` 的这五个作用域。具体测试与构建结果以本候选的 CI 记录为准。

一次有限的本机传输微基准使用独立编译的生产 Command、五类实际发送块及同一原版 x64 Host，按旧/新交替测三对，每次发送 100 万条命令。Client 墙钟成本分别减少 12.70%、18.61%、18.48%，线程周期降幅中位数为 16.98%；字段、UID、flags、末尾偏移及小环校验通过。这是 MinGW `-O2 -DNDEBUG`、无 LTO、CPU 0–15、关闭诊断的约 0.2 秒合成负载，各类命令等权，不包含 D3D9/DXVK/GPU。原始复核记录保存在本地 `results/ipc-hot-packets/review/`。它支持进一步构建验证，不是 MSVC 正式构建或游戏的性能结论。

代码减少重复工作不等于游戏已提帧；传输微基准也不等于 FPS、low 帧或输入延迟。本候选没有新的游戏采样，不复用其他补丁、C32 或旧版的游戏成绩。只有验证构建，没有因本项优化发布正式 Release。
