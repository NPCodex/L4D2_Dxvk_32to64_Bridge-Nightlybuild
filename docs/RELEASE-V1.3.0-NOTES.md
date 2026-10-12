# LDB v1.3.0

本次整合 IPC v2、VA 分配器、可选 Shadow 映射优化、Steam Overlay 修复，以及新版 L4N 配置菜单。

- **Steam Overlay 修复**：修复 Shift+Tab 开关、鼠标交互、Join Server 界面及创意工坊入口的相关问题；增加 Steam 窗口与输入适配、限定调用位置的工坊可用性代理。
- **完整 IPC v2**：统一 Client 与 Host 的双向传输，使用独立上传池管理上传和长 Lock 数据；默认 C8，保留 C1/C2/C4/C8/C16/C32 六档。
- **VB/IB VA 分配器**：使用可释放的 Windows VA 分配器，空分配区域归还系统，减少缓冲销毁后的地址空间保留。
- **可选 Shadow 按需映射**：符合条件的静态 VB/IB 在 Lock 期间映射，解锁后使用限额缓存或解除映射，同时保留完整历史内容。默认关闭，Client AV 非常紧张时可选择启用。
- **L4N 配置菜单**：新增 `ipcBatchSize` 和 `vbibMappedShadow` 菜单，显示 conf 当前值；选择后点击 Save 写入配置，完整重启游戏与 Host 后生效。
- **语言切换**：L4N 默认英文，可切换中文；本会话有效，重新进入菜单刷新文字。
- **默认配置**：新安装使用 x64 Host、C8、learned-aggressive，开启 Steam 修复；详细诊断与 Shadow 映射优化默认关闭。升级包保留已有配置。

| 下载 | 用途 |
| --- | --- |
| `l4d2-bridge-v1.3.0.zip` | 首次安装完整包，含配套三端、DXVK 和默认配置。 |
| `l4d2-bridge-patch-v1.3.0.zip` | 三端升级包，保留配置、DXVK、ReShade 和 retention DB。 |
| `l4d2-bridge-l4n-v1.3.0.zip` | 可选 L4N 插件，包含新配置菜单及中英文切换。 |

升级前退出游戏和全部 Host，备份后成套替换桥二进制；L4N 插件单独更新。旧 conf 的显式值优先于新默认值。需要开启 Steam 修复时，将包内 `config/STEAM-OVERLAY-FIX.conf` 的相关项合并进实际 conf，不要替换整份配置。

默认 `client.ipcBatchSize=8`、`client.vbibMappedShadow=False`。Steam 工坊可用性代理使用 x64 Host，并对未知 GameUI/Renderer 版本保持原行为。完整安装及回退说明见包内文档。
