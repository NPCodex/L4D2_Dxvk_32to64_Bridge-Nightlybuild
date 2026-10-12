$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$source = Join-Path $repoRoot '.deps/dxvk-remix'
. (Join-Path $source 'bridge/build_common.ps1')
$output = Join-Path $repoRoot '.deps/buffer-shadow-arena-test'
New-Item -ItemType Directory -Force $output | Out-Null
foreach ($platform in @('x86', 'x64')) {
  SetupVS -Platform $platform -VcVarsVer '14.29'
  Push-Location $output
  try {
    & cl.exe /nologo /std:c++17 /EHsc /O2 /W4 /WX /DNOMINMAX "/I$source/bridge/src/client" "$repoRoot/tests/buffer_shadow_arena.cpp" "/Fe:arena-$platform.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Arena core compilation failed' }
    & ".\arena-$platform.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Arena core regression failed' }
  } finally { Pop-Location }
}
