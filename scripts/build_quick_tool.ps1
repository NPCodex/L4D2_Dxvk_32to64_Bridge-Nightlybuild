# SPDX-License-Identifier: MIT
[CmdletBinding()]
param([string]$OutputDirectory,[switch]$Tests)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if(!$OutputDirectory){$OutputDirectory=Join-Path $repo 'dist\quick-tool'}
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
$compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if(!(Test-Path -LiteralPath $compiler)){$compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe'}
if(!(Test-Path -LiteralPath $compiler)){throw 'Windows .NET Framework 4.x C# compiler was not found.'}
$source=Join-Path $repo 'tools\quick-tool'
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$references=@('/r:System.Windows.Forms.dll','/r:System.Drawing.dll','/r:System.Web.Extensions.dll','/r:System.IO.Compression.dll','/r:System.IO.Compression.FileSystem.dll')
$runtimeSources=@('Core.cs','HoverHelp.cs','LogMonitor.cs','SteamSupport.cs','Installation.cs','StartupCheck.cs','LaunchPane.cs','MainForm.cs','AssemblyInfo.cs')
$inputPaths=@($runtimeSources | ForEach-Object {'tools/quick-tool/'+$_})+@('scripts/build_quick_tool.ps1')
$hashes=[ordered]@{}
foreach($relative in $inputPaths){$hashes[$relative]=(Get-FileHash -LiteralPath (Join-Path $repo $relative) -Algorithm SHA256).Hash.ToLowerInvariant()}
$files=$runtimeSources | ForEach-Object {Join-Path $source $_}
& $compiler /nologo /utf8output /target:winexe /platform:anycpu /optimize+ ('/out:'+(Join-Path $OutputDirectory 'BridgeQuickTool.exe')) @references @files
if($LASTEXITCODE -ne 0){throw 'Quick tool compilation failed.'}
if($Tests){
    $files=@('Core.cs','Tests.cs') | ForEach-Object {Join-Path $source $_}
    & $compiler /nologo /utf8output /target:exe /platform:anycpu /optimize+ ('/out:'+(Join-Path $OutputDirectory 'BridgeQuickTool.Tests.exe')) @references @files
    if($LASTEXITCODE -ne 0){throw 'Core test compilation failed.'}
    $files=@('Core.cs','SteamSupport.cs','Installation.cs','StartupCheck.cs','LauncherTests.cs') | ForEach-Object {Join-Path $source $_}
    & $compiler /nologo /utf8output /target:exe /platform:anycpu /optimize+ ('/out:'+(Join-Path $OutputDirectory 'BridgeQuickTool.LauncherTests.exe')) @references @files
    if($LASTEXITCODE -ne 0){throw 'Launcher test compilation failed.'}
}
$exe=Join-Path $OutputDirectory 'BridgeQuickTool.exe'
foreach($relative in $inputPaths){if((Get-FileHash -LiteralPath (Join-Path $repo $relative) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $hashes[$relative]){throw 'Quick-tool source changed during compilation.'}}
$version=[Reflection.AssemblyName]::GetAssemblyName($exe).Version.ToString(3)
$receipt=[ordered]@{schema=1;product='BridgeQuickTool';version=$version;exe_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant();sources=$hashes}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'quick-tool-build.json') -Encoding utf8
Write-Output ('Built '+(Join-Path $OutputDirectory 'BridgeQuickTool.exe'))
