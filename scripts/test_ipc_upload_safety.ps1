# SPDX-License-Identifier: MIT
# Copyright (c) 2026 yeyunyyds.
param([ValidateSet('x86','x64')][string]$Platform)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$source=Join-Path $root '.deps/dxvk-remix'
$baseline=Join-Path $root '.deps/ipc-safety-baseline'
if (!$Platform) {
  python "$PSScriptRoot/prepare_ipc_safety_tests.py"
  if($LASTEXITCODE -ne 0) {throw 'Safety baseline preparation failed'}
  foreach($arch in @('x86','x64')) {
    & powershell.exe -NoProfile -File $PSCommandPath -Platform $arch
    if($LASTEXITCODE -ne 0) {throw "Upload safety failed: $arch"}
  }
  python "$PSScriptRoot/run_ipc_safety_replies.py"
  if($LASTEXITCODE -ne 0) {throw 'Reply budgets failed'}
  exit 0
}
. (Join-Path $source 'bridge/build_common.ps1')
. (Join-Path $PSScriptRoot 'assert_native_architecture.ps1')
SetupVS -Platform $Platform -VcVarsVer '14.29'
$out=Join-Path $root "work/upload-safety-$Platform"
New-Item -ItemType Directory -Force $out | Out-Null
foreach($variant in @('fixed','baseline')) {
  $inputSource=if($variant -eq 'fixed') {$source} else {$baseline}
  $gen=Join-Path $out $variant
  python "$root/tests/host_upload_safety_generate.py" --source $inputSource --output $gen
  if($LASTEXITCODE -ne 0) {throw 'Upload extraction failed'}
  Push-Location $gen
  try {
    foreach($layout in @('packed','rows')) {
      [string[]]$flags=@()
      if($variant -eq 'fixed') {$flags+='/DFIXED_UPLOAD_SOURCE'}
      if($layout -eq 'rows') {$flags+='/DHOST_ROW_UPLOADS'}
      & cl.exe /nologo /std:c++17 /EHsc /O2 /DNDEBUG /W3 /DNOMINMAX @flags "/I$gen" "/I$inputSource/bridge/src/server" "/I$inputSource/bridge/src/util" "$root/tests/host_upload_safety.cpp" "/Fe:upload-$layout.exe"
      if($LASTEXITCODE -ne 0) {throw "Upload compilation failed $variant $layout"}
      Assert-NativeArchitecture -Path (Join-Path $gen "upload-$layout.exe") -Architecture $Platform
      if($variant -eq 'fixed') {
        & ".\upload-$layout.exe"
        if($LASTEXITCODE -ne 0) {throw "Upload correctness failed $layout"}
      } else {
        foreach($handler in @('vb','ib','surface','volume','volume-texture')) {
          & ".\upload-$layout.exe" "lock-failure-$handler"
          if($LASTEXITCODE -ne 2) {throw "Old backend failure was not detected: $handler"}
        }
        if($Platform -eq 'x64') {
          & ".\upload-$layout.exe" small-mip
          if($LASTEXITCODE -ne 2) {throw 'Old x64 small mip widening was not detected'}
        }
      }
    }
  } finally {Pop-Location}
}
$transport=Join-Path $root 'work/ipc-safety-baseline'
Push-Location $transport
try {
  & cl.exe /nologo /std:c++17 /EHsc /O2 /W3 /DNOMINMAX "/I$transport" full-test.cpp "/Fe:full-$Platform.exe"
  if($LASTEXITCODE -ne 0) {throw 'Old FullQueue control compilation failed'}
  & ".\full-$Platform.exe" --close-tail-only
  if($LASTEXITCODE -ne 1) {throw 'Old FullQueue close race was not detected'}
  & cl.exe /nologo /std:c++17 /EHsc /O2 /W3 /DNOMINMAX "/I$transport" core-test.cpp "/Fe:core-$Platform.exe"
  if($LASTEXITCODE -ne 0) {throw 'Old Reader control compilation failed'}
  & ".\core-$Platform.exe" --close-tail-only
  if($LASTEXITCODE -ne 1) {throw 'Old Reader close race was not detected'}
  $role=if($Platform -eq 'x64') {'host64'} else {'client32'}
  [string[]]$flags=@('/DLDB_MAPPED_BUFFER_TRANSPORT')
  if($role -eq 'host64') {$flags+=@('/DREMIX_BRIDGE_SERVER')}
  & cl.exe /nologo /std:c++17 /EHsc /O2 /DNDEBUG /W3 /DNOMINMAX /DWIN32 @flags "/I$transport" ipc_transport.cpp util_sharedmemory.cpp util_semaphore.cpp data_diagnostics.cpp exception_diagnostics.cpp "/Fe:transport-$role.exe" /link /LARGEADDRESSAWARE ole32.lib psapi.lib
  if($LASTEXITCODE -ne 0) {throw 'Baseline reply control compilation failed'}
} finally {Pop-Location}
