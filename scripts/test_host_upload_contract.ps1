# SPDX-License-Identifier: MIT
param(
  [string]$Source,
  [ValidateSet('x86','x64')][string]$Platform
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
if (-not $Source) { $Source = Join-Path $repoRoot '.deps/dxvk-remix' }
if (-not $Platform) {
  foreach ($arch in @('x86','x64')) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Source $Source -Platform $arch
    if ($LASTEXITCODE -ne 0) { throw "Host upload contract failed: $arch" }
  }
  return
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw 'An installed MSVC x86/x64 toolchain is required' }
Push-Location (Join-Path $vsPath 'VC/Auxiliary/Build')
try {
  & cmd.exe /d /c "vcvarsall.bat $Platform >nul && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Force -Path "Env:\$($Matches[1])" -Value $Matches[2] }
  }
  if ($LASTEXITCODE -ne 0) { throw 'MSVC environment initialization failed' }
} finally { Pop-Location }
$output = Join-Path $repoRoot ".deps/host-upload-contract-$Platform"
New-Item -ItemType Directory -Force $output | Out-Null
Push-Location $output
try {
  foreach ($control in @('none','pitch','lock','query','row','row-allocation','empty-buffer')) {
    & python "$repoRoot/tests/host_upload_contract_generate.py" $Source $output --control $control
    if ($LASTEXITCODE -ne 0) { throw 'Host contract source extraction failed' }
    if ($control -eq 'none') {
      & cl.exe /nologo /std:c++17 /EHsc /W4 /WX /O2 /DNDEBUG /DNOMINMAX /DLDB_SHADOW_ARENA "/I$Source/bridge/src/util" "/I$Source/bridge/src/client" "/I$repoRoot/tests" "$output/host_empty_client.cpp" "$Source/bridge/src/util/data_diagnostics.cpp" '/Fe:host-empty-client.exe'
      if ($LASTEXITCODE -ne 0) { throw 'Client empty-upload reachability compilation failed' }
      & .\host-empty-client.exe
      if ($LASTEXITCODE -ne 0) { throw 'Client empty-upload reachability failed' }
    }
    # Deliberately broken controls can make later checks provably unreachable.
    $controlFlags = @()
    if ($control -ne 'none') { $controlFlags += '/wd4702' }
    if ($control -in @('row','row-allocation')) { $controlFlags += '/DHOST_ROW_PACKETS' }
    if ($control -eq 'row-allocation') { $controlFlags += '/DHOST_FAIL_VOLUME_ALLOCATION' }
    if ($control -eq 'empty-buffer') { $controlFlags += '/DHOST_ONLY_EMPTY_PACKETS'; $controlFlags += '/wd4505' }
    & cl.exe /nologo /std:c++17 /EHsc /W4 /WX /O2 /DNDEBUG @controlFlags "/I$output" "/I$Source/bridge/src/util" "$repoRoot/tests/host_upload_contract.cpp" "/Fe:host-contract-$control.exe"
    if ($LASTEXITCODE -ne 0) { throw "Host contract compilation failed: $control" }
    & cmd.exe /d /c "host-contract-$control.exe > host-contract-$control.log 2>&1"
    $exit = $LASTEXITCODE
    $log = Get-Content -LiteralPath "host-contract-$control.log" -Raw
    Write-Output $log
    if ($control -in @('none','row','row-allocation')) {
      if ($exit -ne 0) { throw 'Host contract valid-input regression failed' }
    } else {
      $expected = switch ($control) {
        'pitch' { 'short destination pitch accepted' }
        'lock' { 'failed surface lock copied or unlocked' }
        'query' { 'pending query leaked' }
        'empty-buffer' { 'empty VB/IB packet skipped or changed backend Lock' }
      }
      if ($exit -ne 1 -or $log -notmatch [regex]::Escape($expected)) {
        throw "Host negative control $control did not detect the intended regression"
      }
    }
  }
} finally { Pop-Location }
