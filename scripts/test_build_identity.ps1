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
if (!$Output) { $Output = Join-Path $repoRoot '.deps/build-identity-contract' }
$Source = [IO.Path]::GetFullPath($Source)
$Output = [IO.Path]::GetFullPath($Output)
if (!$Architecture) {
  & python (Join-Path $repoRoot 'tests/build_identity_contract.py') --source $Source --output $Output
  if ($LASTEXITCODE -ne 0) { throw 'Identity source extraction failed' }
  $compilerArgs = @('-VcVarsVer', $VcVarsVer)
  if ($UseInstalledCompiler) { $compilerArgs = @('-UseInstalledCompiler') }
  foreach ($arch in @('x86','x64')) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Source $Source -Output $Output -Architecture $arch @compilerArgs
    if ($LASTEXITCODE -ne 0) { throw "Identity compile failed: $arch" }
  }
  & python (Join-Path $repoRoot 'tests/build_identity_contract.py') --output $Output --run
  if ($LASTEXITCODE -ne 0) { throw 'Identity native assertions failed' }
  return
}
if ($UseInstalledCompiler) {
  . (Join-Path $PSScriptRoot 'setup_native_test_compiler.ps1')
  Set-NativeTestCompiler -Architecture $Architecture
} else {
  . (Join-Path $Source 'bridge/build_common.ps1')
  SetupVS -Platform $Architecture -VcVarsVer $VcVarsVer
  if (!$env:VCToolsVersion -or !$env:VCToolsVersion.StartsWith($VcVarsVer)) { throw 'Requested MSVC toolset not initialized' }
}
$directory = Join-Path $Output $Architecture
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source
[pscustomobject]@{architecture=$Architecture;compiler=$compiler;file_version=(Get-Item -LiteralPath $compiler).VersionInfo.FileVersion;vc_tools_version=$env:VCToolsVersion;formal_ci_toolset_equivalence=(!$UseInstalledCompiler -and $env:VCToolsVersion.StartsWith('14.29'))} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $directory 'compiler.json') -Encoding UTF8
Push-Location $directory
try {
  & cl.exe /nologo /std:c++17 /EHsc /O2 /W4 /WX /wd4324 /DNOMINMAX "/I$Output" (Join-Path $repoRoot 'tests/build_identity_contract.cpp') /Fe:identity.exe /link /LARGEADDRESSAWARE
  if ($LASTEXITCODE -ne 0) { throw "Identity compile failed: $Architecture" }
  $bytes = [IO.File]::ReadAllBytes((Join-Path $directory 'identity.exe'))
  $pe = [BitConverter]::ToInt32($bytes, 0x3c)
  $expected = if ($Architecture -eq 'x86') { 0x14c } else { 0x8664 }
  if ([BitConverter]::ToUInt16($bytes, $pe + 4) -ne $expected) { throw 'Identity test PE architecture mismatch' }
  $sha = [Security.Cryptography.SHA256]::Create()
  try { $binaryHash = ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant() }
  finally { $sha.Dispose() }
  [pscustomobject]@{path=(Join-Path $directory 'identity.exe');sha256=$binaryHash;machine=$expected} | ConvertTo-Json | Set-Content binary.json -Encoding UTF8
} finally { Pop-Location }
