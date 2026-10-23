L4D2 Bridge Quick Tool / L4D2 Bridge 快捷工具

No compilation required / 不需要自己编译
1. Download the complete Nightly release ZIP, exit the game/Hosts, and extract
   the package into the game directory following its README.txt.
2. Double-click L4D2-Quick-Tool.cmd in the game root.
3. Open the Mode / Launch / Check tab (模式 / 启动 / 自检).

下载完整 Nightly ZIP，按包内 README.txt 退出游戏并解压到游戏根目录。
双击根目录的 L4D2-Quick-Tool.cmd 即可使用；无需安装编译器或 Python。
系统要求：Windows 10/11，.NET Framework 4.8。

Bridge mode uses the installed matching Bridge files. No-Bridge mode disables
the identified Bridge entry/plugin while retaining existing L4N/ThinFlex and
other independent components. It does not install missing optimizations.
退出游戏和 Host 后再切换模式。无 Bridge 模式保留现有独立组件，不会下载新优化。

The tool checks install locations and renderer flags when opened. Steam launch
options are saved only on request; exit Steam first. After a launch requested
through the tool, an optional check observes matching processes and fresh logs.
自检不代表帧率、地图、画质或长期稳定性验收；没有观察到启动不直接等于崩溃。

Backups/state: tools/BridgeQuickTool/state/
Local reports and ZIP logs: tools/BridgeQuickTool/reports/
Keep state/ when updating the tool. Steam configuration backups may contain
private account settings; keep them local. Nothing is uploaded automatically.
更新工具请保留 state/。启动项备份可能包含账户设置，请保留在本机。

Source, build/test instructions and limitations:
https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/blob/main/docs/QUICK-TOOL.md
