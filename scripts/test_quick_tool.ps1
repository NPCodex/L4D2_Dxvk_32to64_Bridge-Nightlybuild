# SPDX-License-Identifier: MIT
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$BridgeClient,
      [Parameter(Mandatory=$true)][string]$BridgePlugin,
      [string]$OutputDirectory,
      [switch]$UI)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$BridgeClient=(Resolve-Path -LiteralPath $BridgeClient).ProviderPath
$BridgePlugin=(Resolve-Path -LiteralPath $BridgePlugin).ProviderPath
if(!$OutputDirectory){$OutputDirectory=Join-Path ([IO.Path]::GetTempPath()) ('ldbqt-'+[Guid]::NewGuid().ToString('N').Substring(0,8))}
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $OutputDirectory){throw 'Test output already exists. Use a new directory to preserve fixture evidence.'}
& (Join-Path $PSScriptRoot 'build_quick_tool.ps1') -Tests
$binary=Join-Path $repo 'dist\quick-tool'
& (Join-Path $binary 'BridgeQuickTool.Tests.exe') (Join-Path $OutputDirectory 'core') $BridgeClient $BridgePlugin
if($LASTEXITCODE -ne 0){throw 'Core tests failed.'}
& (Join-Path $binary 'BridgeQuickTool.LauncherTests.exe') (Join-Path $OutputDirectory 'launcher') $BridgeClient $BridgePlugin
if($LASTEXITCODE -ne 0){throw 'Launcher tests failed.'}
if($UI){
    $powershell=Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'
    & $powershell -NoProfile -STA -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'test_quick_tool_ui.ps1') -Executable (Join-Path $binary 'BridgeQuickTool.exe') -EvidenceDirectory (Join-Path $OutputDirectory 'ui')
    if($LASTEXITCODE -ne 0){throw 'UI tests failed.'}
}
Write-Output ('All quick-tool tests passed. Evidence: '+$OutputDirectory)
