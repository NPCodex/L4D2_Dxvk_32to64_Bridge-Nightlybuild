param([ValidateSet('client32','host32','host64')][string]$CompileRole)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$source=Join-Path $root '.deps/dxvk-remix'
$out=Join-Path $root 'work/ipc-full-comparison'
if ($CompileRole) {
  $arch=if ($CompileRole -eq 'host64') {'x64'} else {'x86'}
  . (Join-Path $source 'bridge/build_common.ps1')
  SetupVS -Platform $arch -VcVarsVer '14.29'
  Push-Location $out
  try {
    [string[]]$flags=if ($CompileRole -eq 'host32') {@('/DREMIX_BRIDGE_X86_SERVER','/DREMIX_BRIDGE_SERVER')} else {@()}
    & cl.exe /nologo /std:c++17 /EHsc /O2 /DNDEBUG /W3 /DNOMINMAX /DWIN32 @flags "/I$out" ipc_transport.cpp util_sharedmemory.cpp util_semaphore.cpp data_diagnostics.cpp exception_diagnostics.cpp "/Fe:transport-$CompileRole.exe" /link /LARGEADDRESSAWARE ole32.lib psapi.lib
    if ($LASTEXITCODE -ne 0) {throw "Frozen C32 compile failed: $CompileRole"}
  } finally {Pop-Location}
} else {
  python "$PSScriptRoot/generate_ipc_full_comparison.py"
  if ($LASTEXITCODE -ne 0) {throw 'Frozen production extraction failed'}
  foreach ($role in @('client32','host32','host64')) {
    & powershell.exe -NoProfile -File $PSCommandPath -CompileRole $role
    if ($LASTEXITCODE -ne 0) {throw "Frozen C32 build failed: $role"}
  }
  python "$PSScriptRoot/run_ipc_full_comparison.py"
  if ($LASTEXITCODE -ne 0) {throw 'Full comparison failed'}
}
