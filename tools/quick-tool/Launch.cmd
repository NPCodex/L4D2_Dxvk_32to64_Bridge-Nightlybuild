@echo off
setlocal DisableDelayedExpansion
cd /d "%~dp0" || exit /b 1
if not exist "%~dp0left4dead2.exe" (
  echo Extract the complete release ZIP into your L4D2 game directory first.
  pause
  exit /b 1
)
if not exist "%~dp0tools\BridgeQuickTool\BridgeQuickTool.exe" (
  echo The quick tool is missing. Extract the complete release ZIP again.
  pause
  exit /b 1
)
start "" "%~dp0tools\BridgeQuickTool\BridgeQuickTool.exe" --game-root "%~dp0."
