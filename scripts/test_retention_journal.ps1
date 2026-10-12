# SPDX-License-Identifier: MIT
param([ValidateSet('', 'x86', 'x64')][string]$Platform = '', [string]$Source)
$ErrorActionPreference = 'Stop'
if (!$Platform) {
  foreach ($arch in @('x86','x64')) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Platform $arch -Source $Source
    if ($LASTEXITCODE -ne 0) { throw "Journal $arch regression failed" }
  }
  exit 0
}
$root = Split-Path $PSScriptRoot -Parent
if (!$Source) { $Source = Join-Path $root '.deps/dxvk-remix' }
$Source = [IO.Path]::GetFullPath($Source)
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'MSVC build tools not found' }
$vcvars = Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
& cmd.exe /d /s /c "`"$vcvars`" $Platform >nul && set" | ForEach-Object {
  if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Force -Path "env:$($Matches[1])" -Value $Matches[2] }
}
if ($LASTEXITCODE -ne 0) { throw 'MSVC environment initialization failed' }
$output = Join-Path $root '.deps/retention-journal-test'
New-Item -ItemType Directory -Force $output | Out-Null
Push-Location $output
try {
  & cl.exe /nologo /std:c++17 /EHsc /O2 /DNDEBUG /W4 /WX "/I$source/bridge/src/client" "$root/tests/retention_journal_contract.cpp" "/Fe:retention-journal-$Platform.exe" /link bcrypt.lib
  if ($LASTEXITCODE -ne 0) { throw 'Journal contract compilation failed' }
  & ".\retention-journal-$Platform.exe" | Tee-Object -FilePath "retention-journal-$Platform.log"
  if ($LASTEXITCODE -ne 0) { throw 'Journal contract failed' }
} finally { Pop-Location }
