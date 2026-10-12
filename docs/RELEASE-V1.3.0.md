# LDB v1.3.0

整合完整 IPC v2、VB/IB VA allocator、默认关闭的按需映射优化，以及已分别通过用户实机验收的 L4N 菜单和 Steam Overlay 修复。默认 IPC 档位为 C8，六档 C1/C2/C4/C8/C16/C32 保留。此次整合版重新编译三端和插件并运行原生回归；不据此宣称新的 FPS 或重度 4K AV 收益。

Steam 修复的实机记录来自 x64 Host：Shift+Tab 打开/关闭、鼠标交互、Join Server 和创意工坊入口通过。窗口适配只作用于符合条件的 Steam Renderer 查询；工坊代理只处理已验证 GameUI 的三个调用位置，并要求新鲜、存活的 Host 可用性状态。未知文件/指令版本不强制套用，也不全局强制 Steam 可用、不注入 URL。

新安装完整包默认 **x64 Host + C8 + learned-aggressive**，启用已验收的 Steam 输入/窗口/工坊适配；内存、API/数据/偏色、Steam 观察等详细诊断保持关闭。x86 Host 继续配套提供；工坊 getter 代理尚无 x86 实机验收，当前验证逻辑仅支持所调查的 x64 Renderer。ReShade 新组合不在这次 Steam 验收范围内。

## 下载与安装

- 首次安装：`l4d2-bridge-v1.3.0.zip`，含三端、原官方 DXVK 2.6.1 x86/x64 和默认配置。
- 已有安装：`l4d2-bridge-patch-v1.3.0.zip`，只更新三端，保留运行配置、DXVK、ReShade、retention DB。
- 可选 L4N：`l4d2-bridge-l4n-v1.3.0.zip`，单独更新 `bin/neko/plugins/L4D2BridgePlugin.dll`；本体无需插件也可运行。
- 使用 `SHA256SUMS.txt` 校验下载。Source code ZIP 不含编译程序；准确编译提交、构建标识、位数和文件校验值见 `BUILD-INFO.json`。

完整退出游戏和所有 Host，先备份，再合并对应包的 `bin/`。三端必须成套替换：`bin/dxvk_d3d9.dll`、`bin/.l4d2bridge/L4D2Bridge32.exe`、`bin/.l4d2bridge/L4D2Bridge64.exe`。Client 若原来使用其他 DLL 名称，按实际已加载的 Bridge 文件更新，避免覆盖 ReShade。

**旧配置的显式值优先，升级补丁不改旧 conf。** 保留已成功的 Steam 配置；从未启用修复的用户，可以参考包内 `config/STEAM-OVERLAY-FIX.conf`，只将相关键合并到实际 `bin/.l4d2bridge/bridge.conf`，不要用片段替换整份配置。该片段选择已验收的 x64 Host 并关闭 Steam 诊断。完整重启后生效。

IPC / Shadow 的本版默认值是：

```ini
client.ipcBatchSize = 8
client.vbibMappedShadow = False
```

L4N 显示 conf 当前值，提供六档或 True/False，选择只暂存，Save 才写入；重启游戏和 Host 后生效。Shadow 提示仅在 Client AV 非常紧张时建议启用。默认关闭时继续保留原 FULL_SHADOW；开启按需映射也保留完整历史内容，冷访问可能增加映射和加载成本。

L4N 默认英文，提供 English/中文切换，本会话有效；重新进入菜单刷新已缓存页面。语言只影响插件界面，不写 conf、不修改运行中的 IPC。

## 验证、边界与回退

当前分支和 Steam 分支已分别取得用户实机通过确认。发布流程验证整合版的实际完整 v2 serializer、六档 x86/x86 与 x86/x64、上传 lease/回绕/失败关闭、资源生命周期/Reset/回读、诊断关闭门槛、Steam 真正调用约定及状态通道、L4N 配置隔离和旧接口兼容，并核对实际 MSVC Release `/O2 NDEBUG` 三端与插件。

这些检查不扩展到未测试的 Steam Renderer/GameUI 版本或所有 ReShade/Mod 组合。没有新的 FPS、1% Low、0.1% Low 或 AV 提升保证。IPC 会话发生故障后完整重启；启用 `memoryMonitoring` 仍可能在调用线程上进行周期 VA 扫描，正常性能测试请关闭诊断。

回退时退出所有进程，成套恢复三端备份；若升级了插件也恢复插件 DLL，并恢复需要回退的配置值。不要混用旧 Client 和新 Host。原预发布及旧正式版附件保留。
