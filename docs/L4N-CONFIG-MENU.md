# L4N 配置菜单与语言切换

插件版本 `common-settings-3`，需要 L4N SDK v2 HUD 与本次更新的 Bridge 配置接口。安装前退出游戏和所有 Host，备份 `bin/neko/plugins/L4D2BridgePlugin.dll`，再替换为本包的同名文件。

IPC/Shadow 菜单需要 v1.3.0 配套 Bridge，不能只安装插件后继续使用旧的 v1.3.0-pre.1 Release 二进制。请成套更新升级包中的 x86 Client、x86 Host、x64 Host，三端标识以 `BUILD-INFO.json` 为准；保留 DXVK、ReShade、配置、retention DB。原 v1.3.0-pre.1 Release 与 Tag 保持原样。

主菜单新增 `ipcBatchSize`、`vbibMappedShadow`、`Language`。

- `ipcBatchSize`：首行显示当前 conf 值，提供 1、2、4、8、16、32 和 Save。选择仅暂存，Save 才写入 `client.ipcBatchSize`。
- `vbibMappedShadow`：首行显示当前 conf 值，提供 True、False 和 Save。提示“仅在 Client AV 非常紧张时建议启用”。只更新 `client.vbibMappedShadow`，不修改阈值、缓存预算或纹理 Shadow。
- 两项保存后均需**完整退出并重启游戏和 Host**。保存不改变当前会话的 IPC 或资源存储；未保存退出，再次进入菜单时丢弃暂存选择。读取或保存失败明确提示，不伪装保存成功。
- `Language`：默认 English，提供 English / 中文。切换立即影响新生成的插件菜单，不写 conf，无需重启 Bridge；关闭游戏后恢复默认英文。L4N 已打开的父页面由 HUD 缓存，请退出再进入插件菜单以刷新。技术键名、True/False、位数、错误码保持原形式，系统错误描述可能仍为英文。

配套 Bridge 缺少配置键时使用 C8 / False 默认值；已存在的 conf 值优先。插件通过 Client 的既有配置编辑器读取和保存实际 conf 路径，保留注释、未知键和换行，不直接打开猜测的配置路径，也不新增运行时切换。

新插件在旧 Bridge 上仍可使用其已有的 Status/GC/Host/Presenter 功能；不支持新配置操作时显示不可用，不尝试写入。回退可恢复插件 DLL；若同时回退 Bridge，三个桥二进制必须成套恢复。
