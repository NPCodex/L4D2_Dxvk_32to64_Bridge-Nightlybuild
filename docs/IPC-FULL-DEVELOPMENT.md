# 完整 IPC v2 主传输移植实验

分支 `codex/ipc-v2-full-integration`。基于已发布 C1/C32 生产实验版，保留 VA allocator。实现和验证进行中；此文档不表示已经完成原生或游戏验收。

四方向使用统一 v2 消息记录，小参数和适合的 Blob 内联。大数据及借出指针使用独立有界上传 lease；原 PR6 Data Ring、Header ring 和其 published/consumed/pin 协议不再承担生产传输。消息块完成仅表示 Host CPU 不再读该消息；长期 Lock 通过 lease 引用保留，直到 Unlock/Destroy。资源持久 Shadow 和 DXVK 仍保留。

默认 `client.ipcBatchSize=1`，32 只对白名单命令开启已有实验批处理；同步和资源生命周期边界强制提交，无后台提交线程。没有 Reply Slot，回复沿反向 v2 流传输；Slot 是可选设计，不是完成主消息流替换的条件。

上传池使用固定预算映射和 4KiB 页，1024 个带 generation 的 lease 描述符。只有最后一次消费/资源 pin 引用释放后才能复用；不能把 GPU 完成与消息完成混为一谈。借出指针不移动，连续空间不足采用有界背压，失败终止。未知布局可能需要增长或一次前缀迁移；已知 Packet/Surface 声明完整大小后一次预留。不声称所有 generic 调用只预留一次，也不声称减少所有复制。

备份并成套替换 x86 Client、x86 Host、x64 Host。保留现有 DXVK、配置、ReShade 和 retention DB；不整体覆盖配置目录。回退时退出进程并恢复三个备份。实机验收必须记录匹配 build ID、C1/C32、Host 位数、Avg FPS、1% Low、0.1% Low、帧时间、Client AV、最大连续空闲区及反复跨图加载时间。

构建与原生结果待完成后补充；不自动合并、创建 Tag 或发布 Release。
