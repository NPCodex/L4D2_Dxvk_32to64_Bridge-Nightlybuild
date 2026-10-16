# ReShade 6.8 输入修复：v1.5.17 原位修订

本次修复 ReShade 6.8 菜单首次用 Home 打开后无法再次关闭，以及效果复选框先出现蓝色焦点边框、需要反复点击才切换的问题。用户已在正式 Steam 游戏安装中验收本地候选，确认 Home 正常开关、复选框正常勾选，并明确要求替换原 v1.5.17，不另发版本。该反馈属于本地候选验收；正式发布另须通过构建、源码收据与附件核验门禁。

## 安装位置

在 ReShade 安装程序中选择游戏目录的 `bin/.l4d2bridge/L4D2Bridge64.exe`，API 选择 Vulkan，不要安装到根目录 `left4dead2.exe`。本包默认通过 x64 Host 执行 Vulkan 渲染。长期安装说明保留于 [README](../README.md)，本次发布说明也提醒一次。

## 机制与改动范围

固定核对 ReShade [v6.8.0 源码提交](https://github.com/crosire/reshade/commit/18deaa52de0c425a78b329e9cb3c497281cd00ec)。其 [`SetWindowsHookExW` 包装器](https://github.com/crosire/reshade/blob/18deaa52de0c425a78b329e9cb3c497281cd00ec/source/input_windows.cpp#L850) 可以接管应用的低级键盘钩子；当菜单阻塞键盘时，[包装后的回调](https://github.com/crosire/reshade/blob/18deaa52de0c425a78b329e9cb3c497281cd00ec/source/input_windows.cpp#L769) 跳过应用的原回调并继续 Windows 钩子链。旧 Presenter 依赖该回调把按键转发到 Host 子窗口，因而可能打开菜单后就收不到关闭所需的 Home。

修复让已绑定的 ReShade 捕获输入时取得 Host Presenter 的原生键盘焦点，真实 Windows 消息直接进入 Host 队列。ReShade 在消息泵中先更新自己的输入状态，再将需要屏蔽的消息改成 WM_NULL，因此菜单仍能处理 Home。原生焦点期间不再额外投递同一按键的转发副本。

Steam 和 ReShade 的捕获来源独立保存，并聚合决定游戏输入是否被占用；来源改变时即使聚合状态仍为开启，也重新应用焦点模式。焦点恢复继续受游戏前台、窗口树和所属线程检查约束。未绑定 ReShade 时，可选 `server.presenterHotkeyFallback` 保留原有转发行为，不因占用同一个内部来源位而切换到本次原生焦点路径。

本次生产源码仅调整 Presenter 原生焦点及对应回归，没有扩大 Client 的鼠标 Capture 交接条件、添加合成点击或更改 ReShade 本体。用户实机反馈表明这套窄修复同时解决了所报复选框现象，但不能由蓝色边框或验收结果反推出所有可能的鼠标丢事件原因。早期鼠标 Capture 建议保留为调查记录，没有作为修复合入。

无需新增配置键；默认 `server.presenterWindow=True`、`server.presenterInput=True` 与配套 Client 钩子继续生效。Steam 中文输入与快捷键属于既有功能回归范围。用户已撤回最初的 Shift+Tab 故障描述，不把它列为本次新修复。

## 已有证据与验证边界

| 证据 | 已完成内容 | 能证明的范围 |
|---|---|---|
| 用户实机验收 | 正式 Steam 安装中的本地候选：Home 可正常开关，效果复选框正常勾选 | 默认 x64 候选的这两项交互；不等于正式下载包重测，也不等于 x86 游戏验收 |
| x86/x64 原生键盘回归 | 既有每架构 88 项断言通过 | 原生键盘路由回归；没有替代游戏验收 |
| ReShade 6.8 源码提取模型 | 每架构 7 个 Home/热键兜底必需场景、54 项断言通过；旧实现保留直接输入成功和 5 个包装器场景失败的负对照 | 包装器跳过 Bridge 回调时，候选仍通过原生队列关闭菜单；覆盖未启用 Steam、活动控件、忽略快捷键、长按重复及未绑定 ReShade 的热键兜底边界 |
| 本地候选构建 | x86 Client、x86 Host、x64 Host 编译与文件身份核验通过 | 本地 MSVC 14.51 候选；不冒充正式 MSVC 14.29 CI |
| 正式发布构建 | 发布门禁要求 | 必须另核对三端 MSVC 14.29、必需原生回归、最终源码收据和原 Release 替换后的 ZIP/SHA |

输入模型使用提取的 Presenter 与固定 ReShade 6.8 源码函数，Windows 队列、焦点和钩子调用边界由测试控制；它没有加载真实 ReShade 渲染器、安装操作系统钩子或操作 Steam。两个 Shift 诊断场景不属于本次缺陷的必需场景，也不能代替实际 Steam 行为验收。

本轮证据目录为 `results/reshade-home-20261016/`：`upstream-reshade/source-manifest.json` 保存 19 份固定上游源码的哈希与 Git blob 校验、Presenter 和 Client 审计快照；`keyboard-model-review.json` 保存早期双架构 6 个 Home 场景与旧实现对照；`keyboard-final-lf/` 保存生成文件字节哈希修正后、补齐未绑定 ReShade 兜底保护的 7 个必需场景及工具链来源；`build-verification.json` 和 `game/installed.json` 绑定用户验收的本地三端候选及安装文件身份。兜底保护由另行回归覆盖，不冒充用户重新验收。历史失败与前期试验仍保留。结果目录属于本地审计材料，不加入玩家 ZIP。

本次没有新增 FPS、1% low、画质、长期稳定性或首次启动/开机冷启动测试，也没有修改 GPLALL 后端。原 v1.5.17 的 NVIDIA 初始化更新与驱动门槛源码范围保留在 [历史发布记录](RELEASE-HISTORY.md) 和 [NVIDIA 源码采用记录](NVIDIA-UPSTREAM-UPDATES.md)，不作为本次新增修复或验证重复发布。
