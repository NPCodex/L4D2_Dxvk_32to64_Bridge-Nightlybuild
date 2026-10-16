# SPDX-License-Identifier: MIT
param(
  [string]$Source,
  [string]$Output,
  [string]$VcVarsVer = '14.29',
  [switch]$UseInstalledCompiler,
  [ValidateSet('x86','x64')][string]$Architecture
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
if (!$Source) { $Source = Join-Path $repoRoot '.deps/dxvk-remix' }
if (!$Output) { $Output = Join-Path $repoRoot '.deps/remix-init-contract' }
$Output = [IO.Path]::GetFullPath($Output)
$Source = [IO.Path]::GetFullPath($Source)
if (!$Architecture) {
  $compilerArgs = @('-VcVarsVer', $VcVarsVer)
  if ($UseInstalledCompiler) { $compilerArgs = @('-UseInstalledCompiler') }
  foreach ($arch in @('x86','x64')) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Source $Source -Output $Output -Architecture $arch @compilerArgs
    if ($LASTEXITCODE -ne 0) { throw "Remix initialization suite failed: $arch" }
  }
  return
}
if ($UseInstalledCompiler) {
  . (Join-Path $PSScriptRoot 'setup_native_test_compiler.ps1')
  Set-NativeTestCompiler -Architecture $Architecture
} else {
  . (Join-Path $Source 'bridge/build_common.ps1')
  SetupVS -Platform $Architecture -VcVarsVer $VcVarsVer
  if (!$env:VCToolsVersion -or !$env:VCToolsVersion.StartsWith($VcVarsVer)) {
    throw "Requested MSVC toolset $VcVarsVer was not initialized"
  }
}
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source
New-Item -ItemType Directory -Force -Path $Output | Out-Null
$harnessHashes = [ordered]@{}
foreach ($relativePath in @('tests/remix_init_contract.cpp', 'tests/remix_init_contract_generate.py', 'scripts/test_remix_init.ps1', 'scripts/native_source.py')) {
  $harnessHashes[$relativePath] = (Get-FileHash -LiteralPath (Join-Path $repoRoot $relativePath) -Algorithm SHA256).Hash.ToLowerInvariant()
}
$repositoryCommit = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Unable to record harness repository commit' }
# This records the checkout identity, not a clean-tree claim. Formal release
# verification also compares these file hashes with the committed blobs.
[pscustomobject]@{schema=1;architecture=$Architecture;repository_commit=$repositoryCommit;harness_sha256=$harnessHashes;recorded_utc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Output "provenance-$Architecture.json") -Encoding UTF8
[pscustomobject]@{architecture=$Architecture;compiler=$compiler;file_version=(Get-Item -LiteralPath $compiler).VersionInfo.FileVersion;vc_tools_version=$env:VCToolsVersion;windows_sdk_version=$env:WindowsSDKVersion;formal_ci_toolset_equivalence=(!$UseInstalledCompiler -and $env:VCToolsVersion.StartsWith('14.29'))} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Output "compiler-$Architecture.json") -Encoding UTF8
$expectedFailures = @{
  'before-start' = 'before-start request emitted IPC'
  'wrong-uid' = 'host response UID does not match request'
  'discard-status' = 'host initialization result was not propagated'
}
$rows = @()
foreach ($variant in @('none', 'before-start', 'wrong-uid', 'discard-status')) {
  $directory = Join-Path $Output "$Architecture/$variant"
  New-Item -ItemType Directory -Force -Path $directory | Out-Null
  & python (Join-Path $repoRoot 'tests/remix_init_contract_generate.py') --source $Source --output $directory --control $variant
  if ($LASTEXITCODE -ne 0) { throw "Production extraction failed: $variant" }
  Push-Location $directory
  try {
    $exe = Join-Path $directory 'remix-init.exe'
    & cl.exe /nologo /std:c++17 /EHsc /O2 /W4 /DNDEBUG "/I$directory" "/I$Source/public/include" (Join-Path $repoRoot 'tests/remix_init_contract.cpp') "/Fe:$exe" /link /LARGEADDRESSAWARE
    if ($LASTEXITCODE -ne 0) { throw "Native compile failed: $Architecture $variant" }
    $bytes = [IO.File]::ReadAllBytes($exe)
    $pe = [BitConverter]::ToInt32($bytes, 0x3c)
    $machine = [BitConverter]::ToUInt16($bytes, $pe + 4)
    $expectedMachine = if ($Architecture -eq 'x86') { 0x14c } else { 0x8664 }
    if ($machine -ne $expectedMachine) { throw "Wrong native architecture: $variant" }
    & cmd.exe /d /c 'remix-init.exe > run.log 2>&1'
    $code = $LASTEXITCODE
    $log = [IO.File]::ReadAllText((Join-Path $directory 'run.log'))
    Write-Output $log
    $pass = if ($variant -eq 'none') { $code -eq 0 -and $log.Contains('REMIX_INIT_PASS') } else { $code -eq 1 -and $log.Contains("REMIX_INIT_FAIL: $($expectedFailures[$variant])") }
    $rows += [pscustomobject]@{architecture=$Architecture;variant=$variant;exit_code=$code;passed=$pass;expected_failure=($variant -ne 'none');exe_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash;output=$log}
    $rows | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Output "results-$Architecture.json") -Encoding UTF8
    if (!$pass) { throw "Unexpected native result: $Architecture $variant (exit $code)" }
  } finally { Pop-Location }
}
