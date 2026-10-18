# Optional Windows quick tool

This optional desktop helper ships inside the Nightly player ZIP. It does not
install a renderer or download missing components.

## Download and use (no compilation)

Download the complete package from [Releases](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/releases),
extract it into the game directory following its README, and double-click
**`L4D2-Quick-Tool.cmd`** in the game root. The executable is included at
`tools/BridgeQuickTool/BridgeQuickTool.exe`; no compiler or Python is required.
Close the existing helper before extracting an update and retain `state/`.

The dedicated **Build player quick tool** Actions workflow also produces a
ready-to-extract `L4D2-Quick-Tool.zip` plus checksum, for updating the helper on an
existing installation without downloading game/runtime components. Download that
workflow's `L4D2-Quick-Tool` artifact and extract the inner ZIP into the game root.
Downloading Actions artifacts requires a signed-in GitHub account; public Nightly
release ZIPs remain the normal user download route.

The release workflow compiles/tests the tool before packaging, checks the build
receipt against current source hashes, and requires the executable, launcher and
player guide. A missing, modified or stale tool fails packaging. Test programs,
build receipts, local backups and reports are excluded. Tool source changes are
included in the release recipe fingerprint. Existing published ZIPs are not
retroactively changed: distribution starts with a release built from this change.

## Build from source (contributors)

Requires Windows 10/11 with .NET Framework 4.8. No NuGet dependencies, Python or
third-party runtime is needed by the built application.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build_quick_tool.ps1
```

Output: `dist/quick-tool/BridgeQuickTool.exe`. Copy that executable to the game's
`tools/BridgeQuickTool/` directory, or run it separately. For an existing helper
installation, close it and replace its executable; retain `state/` and `reports/`.
The tool first checks its parent folders for the game. Otherwise it reads Steam's
registered libraries and App 550 manifest. If no unique game can be found it asks
for the game folder. A folder must contain `left4dead2.exe` and `bin/`.

An explicit location is also supported:

```powershell
BridgeQuickTool.exe --game-root "X:\SteamLibrary\steamapps\common\Left 4 Dead 2"
```

No drive letter, account ID, developer backup path, or particular game version is
hardcoded. Network libraries are not automatically probed. Reparse-point game
paths are rejected; select the real local folder. This .NET Framework build uses
the normal Windows path-length limits; use a reasonably short installation path.

## Two modes

- **Bridge:** uses the installed Bridge and its current Host/backend settings.
  Enabling restores a hash-checked backup made by this tool, with matching
  Host/backend/L4N files. A clean optimization-only installation without such a
  backup needs the full official Bridge package before Bridge can be enabled.
- **No Bridge / existing optimizations:** moves only the identified Bridge loader
  and its menu plugin into a recoverable snapshot. L4N, the ThinFlex repair,
  necola extensions, other mods and their configurations remain installed. It
  also works on an installation where Bridge was never installed; Bridge-specific
  settings need not exist there. Missing optimizations are reported, not installed.

No-Bridge mode does not reproduce Bridge's IPC, memory or DXVK features in the
native renderer. It is a way to retain the package's independent components while
disabling Bridge, not a promise of higher FPS or compatibility.

Exit the game and both Hosts before changing modes/configuration. Files are
checked and backed up, and interrupted loader changes have a recovery journal.
The default `bin/d3d9.dll` and historical `bin/dxvk_d3d9.dll` paths are distinct.
An inactive alternate is retained; the startup check flags a renderer argument
that would accidentally load it. Unknown DLLs are never silently replaced.

## Steam launch options

The new tab reads App 550's complete launch-options string for the current Steam
account. It uses the active-account registry entry, or the single MostRecent
login record; it never guesses from the first userdata directory.

Edit the string and explicitly save. **Steam must be exited before saving** to
avoid its cached configuration overwriting the edit. The helper checks that the
account/file has not changed, backs up the original bytes, and atomically changes
only the nested App 550 `LaunchOptions` value. Other games, comments, encoding and
unknown settings remain unchanged. Duplicate/ambiguous KeyValues, stale files and
multiline values are rejected. Changing modes does not rewrite launch arguments.

Full-file backups are in `state/steam-launch-backups/`. They may contain private
Steam settings; keep them local. They are excluded from the log ZIP collector.
The helper does not enable RCON, change unrelated arguments, or store credentials
in startup receipts. Launching uses `steam://rungameid/550` and the saved options.
Unsaved editor changes must be saved or discarded before launching.

## Automatic installation and startup checks

Opening the tool performs a read-only installation scan and saves a local report
under `reports/checks/`. The same scan runs before each requested launch. It checks:

- local folder structure and x86 game PE identity;
- the unique App 550 installation in Steam's library manifests;
- active loader mode and conflicts involving `-vulkan`;
- required Bridge Client, both Hosts and both backend architectures in Bridge mode;
- Client/Host embedded build markers when available; missing markers are explicitly
  unverified rather than silently accepted as proof of a matched build;
- readable Bridge configuration, L4N presence and the known ThinFlex file hash.

Conflicts block a Steam launch request. Warnings describe missing optional
components or limits of the evidence. The tool does not silently relocate,
reinstall, patch engine files, or apply experimental optimization settings.

With the startup-check box selected, a launch initiated by the tool is observed
for at most 90 seconds while the tool remains open. It looks for a **new process
at the matching game path**, the expected matching-path Host in Bridge mode, and
new/changed game logs. Its own audit log is not counted as game-start evidence.
An unreadable process path is not counted as a verified match. The UI distinguishes
startup observed, incomplete evidence, unexpected Host, and no process observed.
Steam dialogs, manual cancellation or exit may explain an incomplete result;
absence is not automatically called a crash.

Closing the helper stops observation. A pending receipt can be read on the next
opening; any monitoring gap is shown, and an expired check is not marked as a
success. “Stop startup check” cancels observation without terminating the game.
Configuration/mode writes are blocked during a pending startup observation.

These checks **do not validate maps, image correctness, FPS, input handling or
long-term stability**. A successful install/startup check is not gameplay acceptance.
They also do not audit every L4N/necola feature configuration or third-party mod.

## Existing controls and logs

The helper also provides common Bridge settings, explicit configuration saves
with backups, local bounded/redacted log ZIPs, and a read-only live-log tab.
Live viewing reads at most 256 KiB / 2000 source lines every second while its tab
is visible. It supports pause/resume, file-list refresh, file location and scrolling;
truncation/recreation are handled. Leaving the tab stops live-log polling.

Detailed diagnostics are never enabled by merely viewing or packing logs. Log
bundles are local, exclude Steam userdata/dumps/databases/executable payloads, and
have per-file/total-size limits. Redaction is best-effort: inspect player/server
information before sharing. No uploads, scheduled tasks, installed services,
automatic updates or forced process termination are performed.

## Verification

The optional test runner builds fixture-only tests using a genuine Bridge Client
and menu-plugin file supplied from a built/unpacked bundle. It copies them into
temporary fixtures; it never loads them or launches a game.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test_quick_tool.ps1 `
  -BridgeClient "X:\unpacked\bin\d3d9.dll" `
  -BridgePlugin "X:\unpacked\bin\neko\plugins\L4D2BridgePlugin.dll" -UI
```

The output directory defaults to a unique short temporary folder. Use
`-OutputDirectory` to keep evidence at a chosen new, short path. Tests refuse an
existing output directory. UI tests run with Windows PowerShell 5.1 in STA mode.

Coverage includes 15 existing switch/config/ZIP groups, 10 launcher/installation
groups, and 5 live-view/UI groups. Steam writes use fake account files, and startup
outcomes use injected observations; no production Steam configuration or game is
modified. Unicode paths, quoted values, exact backup bytes, both optimization-only
and Bridge fixtures, stale-save guards, renderer conflicts, wrong architecture,
build mismatches, monitoring gaps, and missing startup evidence are exercised.

`DrawToBitmap` does not capture the RichTextBox's native text surface. UI tests
inspect that control's actual text and state as well as the rendered surrounding
layout. Actual gameplay and Steam-launch acceptance remain a manual follow-up.

## 说明（中文）

这是随 Nightly 完整 ZIP 提供的可选 Windows 工具。解压后双击游戏根目录的
`L4D2-Quick-Tool.cmd` 即可，不需要自己编译。也可从 Build player quick tool
工作流下载已构建的更新 ZIP。现有已发布旧包不会自动改变。支持原版 Bridge 和
“停用 Bridge、保留已有 L4N/ThinFlex 等组件”两种模式。不是额外优化的自动安装器。
开发者可运行 `scripts/build_quick_tool.ps1` 构建；程序位于游戏的 `tools/BridgeQuickTool/`，
也可单独运行并由 Steam 安装记录定位。升级工具请保留 `state/`。

打开工具自动检查安装目录、Steam App 550 目标、文件位数和已知启动参数冲突。
Steam 启动项只在显式保存时修改，保存前先退出 Steam，且会备份原文件。
通过工具启动后可观察最多 90 秒，检查游戏/Host 进程和新日志；未观察到不等于崩溃。
自检不证明地图、画质、FPS 或长期稳定性。关闭工具即停止观察，没有后台服务。
设置、日志查看和一键打包均在本机完成；分享前仍需检查内容。
