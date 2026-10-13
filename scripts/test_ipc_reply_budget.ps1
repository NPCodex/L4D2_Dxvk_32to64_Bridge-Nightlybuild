# SPDX-License-Identifier: MIT
param(
  [ValidateSet('client32','host32','host64')][string]$CompileRole,
  [ValidateSet('candidate','without-uid-budget-preservation')][string]$Variant = 'candidate',
  [string]$Source,
  [string]$OutputDir
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$Source) { $Source = Join-Path $root '.deps/dxvk-remix' }
if (!$OutputDir) { $OutputDir = Join-Path $root '.deps/ipc-reply-budget' }
$Source = [IO.Path]::GetFullPath($Source)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
if (!$CompileRole) {
  python "$PSScriptRoot/prepare_ipc_reply_budget.py" --source "$Source" --output "$OutputDir"
  if ($LASTEXITCODE -ne 0) { throw 'Reply-budget source extraction failed' }
  foreach ($role in @('client32','host32','host64')) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -CompileRole $role -Source "$Source" -OutputDir "$OutputDir"
    if ($LASTEXITCODE -ne 0) { throw "Reply-budget compilation failed: $role" }
  }
  & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -CompileRole client32 -Variant without-uid-budget-preservation -Source "$Source" -OutputDir "$OutputDir"
  if ($LASTEXITCODE -ne 0) { throw 'Reply-budget negative-control compilation failed' }
  python "$PSScriptRoot/run_ipc_safety_replies.py" --output "$OutputDir"
  if ($LASTEXITCODE -ne 0) { throw 'Reply-budget regression failed' }
  exit 0
}
. (Join-Path $PSScriptRoot 'setup_native_test_compiler.ps1')
. (Join-Path $PSScriptRoot 'assert_native_architecture.ps1')
$arch = if ($CompileRole -eq 'host64') { 'x64' } else { 'x86' }
Set-NativeTestCompiler -Architecture $arch
$out = Join-Path $OutputDir $Variant
Push-Location $out
try {
  [string[]]$flags = if ($CompileRole -eq 'host32') { @('/DREMIX_BRIDGE_X86_SERVER','/DREMIX_BRIDGE_SERVER') } else { @() }
  if (Test-Path "$out/mapped_buffer_template.h") { $flags += '/DLDB_MAPPED_BUFFER_TRANSPORT' }
  & cl.exe /nologo /std:c++17 /EHsc /O2 /DNDEBUG /W3 /DNOMINMAX /DWIN32 @flags "/I$out" ipc_transport.cpp util_sharedmemory.cpp util_semaphore.cpp data_diagnostics.cpp exception_diagnostics.cpp "/Fe:transport-$CompileRole.exe" /link /LARGEADDRESSAWARE ole32.lib psapi.lib
  if ($LASTEXITCODE -ne 0) { throw "Reply-budget compile failed: $Variant $CompileRole" }
  Assert-NativeArchitecture -Path (Join-Path $out "transport-$CompileRole.exe") -Architecture $arch
} finally { Pop-Location }
