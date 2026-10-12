# SPDX-License-Identifier: MIT
param(
  [ValidateSet('baseline','candidate')][string]$Variant,
  [ValidateSet('client32','host32','host64')][string]$CompileRole,
  [string]$Source,
  [string]$OutputDir,
  [switch]$Compare,
  [switch]$SkipTransport,
  [switch]$HeapOnly
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$OutputDir) { $OutputDir = if ($Compare) { Join-Path $root 'results/optimization45/ipc' } else { Join-Path $root '.deps/optimization45-ipc-test' } }
$outRoot = [IO.Path]::GetFullPath($OutputDir)
if (!$Source) { $Source = Join-Path $root '.deps/dxvk-remix' }
$Source = [IO.Path]::GetFullPath($Source)
if ($CompileRole) {
  $out = Join-Path $outRoot $Variant
  $arch = if ($CompileRole -eq 'host64') { 'x64' } else { 'x86' }
  $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
  $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if (!$vs) { throw 'MSVC build tools not found' }
  $vcvars = Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
  & cmd.exe /d /s /c "`"$vcvars`" $arch >nul && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Force -Path "env:$($Matches[1])" -Value $Matches[2] }
  }
  if ($HeapOnly) { $out = Join-Path $outRoot 'sharedheap' }
  Push-Location $out
  try {
    [string[]]$flags = if ($CompileRole -eq 'host32') { @('/DREMIX_BRIDGE_X86_SERVER','/DREMIX_BRIDGE_SERVER') } else { @() }
    if ($HeapOnly) {
      & cl.exe /nologo /std:c++17 /EHsc /O2 /DNDEBUG /W3 /DNOMINMAX /DWIN32 @flags "/I$out" "$root/tests/optimization45_ipc_sharedheap.cpp" util_sharedheap.cpp util_sharedmemory.cpp "/Fe:heap-$CompileRole.exe" /link /LARGEADDRESSAWARE ole32.lib
      if ($LASTEXITCODE -ne 0) { throw "SharedHeap compile failed: $CompileRole" }
      if ($CompileRole -ne 'host32') {
        & cl.exe /nologo /std:c++17 /EHsc /O2 /DNDEBUG /W3 /DNOMINMAX /DWIN32 "/I$out" "/I$Source/bridge/src/client" "/I$Source/bridge/src/util" "$root/tests/optimization45_ipc_clientheap.cpp" "$Source/bridge/src/util/data_diagnostics.cpp" "/Fe:clientheap-$arch.exe"
        if ($LASTEXITCODE -ne 0) { throw "Client SharedHeap compile failed: $arch" }
      }
      return
    }
    if (Test-Path "$out/mapped_buffer_template.h") { $flags += '/DLDB_MAPPED_BUFFER_TRANSPORT' }
    & cl.exe /nologo /std:c++17 /EHsc /O2 /DNDEBUG /W3 /DNOMINMAX /DWIN32 @flags "/I$out" ipc_transport.cpp util_sharedmemory.cpp util_semaphore.cpp data_diagnostics.cpp exception_diagnostics.cpp "/Fe:transport-$CompileRole.exe" /link /LARGEADDRESSAWARE ole32.lib psapi.lib
    if ($LASTEXITCODE -ne 0) { throw "Transport compile failed: $Variant $CompileRole" }
    if ($CompileRole -ne 'host32') {
      & cl.exe /nologo /std:c++17 /EHsc /O2 /DNDEBUG /W4 /wd4324 /DNOMINMAX "/I$out" "$root/tests/optimization45_ipc.cpp" "/Fe:pool-$arch.exe" /link /LARGEADDRESSAWARE
      if ($LASTEXITCODE -ne 0) { throw "Pool compile failed: $Variant $arch" }
      & cl.exe /nologo /std:c++17 /EHsc /O2 /DNDEBUG /W4 /wd4324 /DNOMINMAX "/I$out" "$root/tests/ipc-v2/full.cpp" "/Fe:full-$arch.exe" /link /LARGEADDRESSAWARE
      if ($LASTEXITCODE -ne 0) { throw "Policy compile failed: $Variant $arch" }
      & cl.exe /nologo /std:c++17 /EHsc /O2 /DNDEBUG /W4 /wd4324 /DNOMINMAX "/I$out/ipc_v2" "$root/tests/ipc-v2/native.cpp" "/Fe:core-$arch.exe" /link /LARGEADDRESSAWARE
      if ($LASTEXITCODE -ne 0) { throw "Core compile failed: $Variant $arch" }
    }
  } finally { Pop-Location }
} else {
  if ($HeapOnly) {
    python "$root/tests/optimization45_ipc_heap_prepare.py" --source "$Source" --output "$outRoot/sharedheap" --logger "$outRoot/candidate/log/log.h"
    if ($LASTEXITCODE -ne 0) { throw 'SharedHeap extraction failed' }
    foreach ($role in @('client32','host32','host64')) {
      & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Variant candidate -CompileRole $role -HeapOnly -Source "$Source" -OutputDir "$outRoot"
      if ($LASTEXITCODE -ne 0) { throw "SharedHeap build failed: $role" }
    }
    python "$root/tests/optimization45_ipc_heap_runner.py" --output "$outRoot/sharedheap"
    if ($LASTEXITCODE -ne 0) { throw 'SharedHeap test failed' }
    return
  }
  $variants = if ($Compare) { @('baseline','candidate') } else { @('candidate') }
  foreach ($v in $variants) {
    $variantSource = if ($Compare) { Join-Path $root ".deps/optimization45/$v" } else { $Source }
    python "$root/tests/optimization45_ipc_prepare.py" --source "$variantSource" --output "$outRoot/$v"
    if ($LASTEXITCODE -ne 0) { throw "Extraction failed: $v" }
    foreach ($role in @('client32','host32','host64')) {
      & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Variant $v -CompileRole $role -Source "$variantSource" -OutputDir "$outRoot"
      if ($LASTEXITCODE -ne 0) { throw "Build failed: $v $role" }
    }
  }
  $runArgs = @("$root/tests/optimization45_ipc_runner.py", '--root', "$outRoot")
  if (!$Compare) { $runArgs += '--candidate-only' }
  if ($SkipTransport) { $runArgs += '--skip-transport' }
  python @runArgs
  if ($LASTEXITCODE -ne 0) { throw 'Optimization45 IPC verification failed' }
  $heapSource = if ($Compare) { Join-Path $root '.deps/optimization45/candidate' } else { $Source }
  & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -HeapOnly -Source "$heapSource" -OutputDir "$outRoot"
  if ($LASTEXITCODE -ne 0) { throw 'Optimization45 SharedHeap verification failed' }
}
