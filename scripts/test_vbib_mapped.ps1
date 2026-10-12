param([ValidateSet('x86','x64')][string]$CompileArch)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$source = Join-Path $root '.deps/dxvk-remix'
$out = Join-Path $root 'work/vbib-mapped-native'
if ($CompileArch) {
  . (Join-Path $source 'bridge/build_common.ps1')
  SetupVS -Platform $CompileArch -VcVarsVer '14.29'
  Push-Location $out
  try {
    & cl.exe /nologo /std:c++17 /EHsc /O2 /MT /W4 /WX /DNOMINMAX "/I$source/bridge/src/client" "$root/tests/vbib_section_core.cpp" "/Fe:core-$CompileArch.exe" /link /LARGEADDRESSAWARE
    if ($LASTEXITCODE -ne 0) { throw 'Section policy compilation failed' }
    & ".\core-$CompileArch.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Section policy regression failed' }
    if ($CompileArch -eq 'x86') {
      foreach ($label in @('old','new')) {
        & cl.exe /nologo /std:c++17 /EHsc /O2 /MT /DNDEBUG /W4 /WX /DNOMINMAX /DLDB_SHADOW_ARENA /DLDB_PRESSURE_NATIVE_ALLOCATOR "/I$source/bridge/src/util" "/I$source/bridge/src/client" "/I$root/tests" "$label.cpp" "$source/bridge/src/util/data_diagnostics.cpp" "/Fe:$label-x86.exe" /link /LARGEADDRESSAWARE psapi.lib
        if ($LASTEXITCODE -ne 0) { throw "Actual production template compilation failed: $label" }
      }
      & .\new-x86.exe
      if ($LASTEXITCODE -ne 0) { throw 'Actual mapped LockableBuffer contracts failed' }
    }
  } finally { Pop-Location }
} else {
  python "$root/scripts/generate_vbib_mapped_test.py"
  if ($LASTEXITCODE -ne 0) { throw 'Mapped source generation failed' }
  foreach ($arch in @('x86','x64')) {
    & powershell.exe -NoProfile -File $PSCommandPath -CompileArch $arch
    if ($LASTEXITCODE -ne 0) { throw 'Native mapped test failed' }
  }
  python "$root/scripts/run_vbib_mapped_costs.py"
  if ($LASTEXITCODE -ne 0) { throw 'Mapped A/B costs failed' }
}
