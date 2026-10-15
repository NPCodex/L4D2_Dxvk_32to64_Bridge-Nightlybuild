param(
  [string]$Source,
  [string]$Output,
  [string]$VcVarsVer,
  [ValidateSet('x86','x64')][string]$Architecture
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
if (!$Source) { $Source = Join-Path $repoRoot '.deps/dxvk-remix' }
if (!$Output) { $Output = Join-Path $repoRoot '.deps/device-destroy-contract' }
$Output = [IO.Path]::GetFullPath($Output)
$Source = [IO.Path]::GetFullPath($Source)
if (!$Architecture) {
  $toolsetArgs = @()
  if ($VcVarsVer) { $toolsetArgs = @('-VcVarsVer', $VcVarsVer) }
  foreach ($arch in @('x86','x64')) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Source $Source -Output $Output -Architecture $arch @toolsetArgs
    if ($LASTEXITCODE -ne 0) { throw "Device destroy suite failed: $arch" }
  }
  return
}
if ($VcVarsVer) {
  . (Join-Path $Source 'bridge/build_common.ps1')
  SetupVS -Platform $Architecture -VcVarsVer $VcVarsVer
  if (!$env:VCToolsVersion -or !$env:VCToolsVersion.StartsWith($VcVarsVer)) {
    throw "Requested MSVC toolset $VcVarsVer was not initialized"
  }
} else {
  . (Join-Path $PSScriptRoot 'setup_native_test_compiler.ps1')
  Set-NativeTestCompiler -Architecture $Architecture
}
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source
$compilerVersion = (Get-Item -LiteralPath $compiler).VersionInfo.FileVersion
New-Item -ItemType Directory -Force -Path $Output | Out-Null
[pscustomobject]@{architecture=$Architecture;compiler=$compiler;file_version=$compilerVersion;vc_tools_version=$env:VCToolsVersion;windows_sdk_version=$env:WindowsSDKVersion;formal_ci_toolset_equivalence=$false} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Output "compiler-$Architecture.json") -Encoding UTF8
$rows = @()
foreach ($variant in @('strict-destroy','none')) {
  $directory = Join-Path $Output "$Architecture/$variant"
  New-Item -ItemType Directory -Force -Path $directory | Out-Null
  & python (Join-Path $repoRoot 'tests/device_destroy_contract_generate.py') --source $Source --output $directory --control $variant
  if ($LASTEXITCODE -ne 0) { throw "Production extraction failed: $variant" }
  Push-Location $directory
  try {
    $exe = Join-Path $directory 'device-destroy.exe'
    & cl.exe /nologo /std:c++17 /EHsc /O2 /W4 /DNDEBUG /DNOMINMAX "/I$directory" (Join-Path $repoRoot 'tests/device_destroy_contract.cpp') "/Fe:$exe" /link /LARGEADDRESSAWARE
    if ($LASTEXITCODE -ne 0) { throw "Native compile failed: $Architecture $variant" }
    $bytes = [IO.File]::ReadAllBytes($exe)
    $pe = [BitConverter]::ToInt32($bytes, 0x3c)
    $machine = [BitConverter]::ToUInt16($bytes, $pe + 4)
    $expectedMachine = if ($Architecture -eq 'x86') { 0x14c } else { 0x8664 }
    if ($machine -ne $expectedMachine) { throw "Wrong native architecture: $variant" }
    & cmd.exe /d /c 'device-destroy.exe > run.log 2>&1'
    $code = $LASTEXITCODE
    $log = [IO.File]::ReadAllText((Join-Path $directory 'run.log'))
    Write-Output $log
    $expected = 'DEVICE_DESTROY_FAIL: duplicate device destroy faulted channel'
    $pass = if ($variant -eq 'none') { $code -eq 0 -and $log.Contains('DEVICE_DESTROY_PASS') } else { $code -eq 1 -and $log.Contains($expected) }
    $rows += [pscustomobject]@{architecture=$Architecture;variant=$variant;exit_code=$code;passed=$pass;expected_failure=($variant -eq 'strict-destroy');exe_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash;output=$log}
    $rows | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Output "results-$Architecture.json") -Encoding UTF8
    if (!$pass) { throw "Unexpected native result: $Architecture $variant (exit $code)" }
  } finally { Pop-Location }
}
