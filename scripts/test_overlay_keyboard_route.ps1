param(
  [string]$Source,
  [string]$Output,
  [string]$ControlSource,
  [string]$VcVarsVer = '14.29',
  [ValidateSet('x86','x64')][string]$Architecture
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
if (!$Source) { $Source = Join-Path $repoRoot '.deps/dxvk-remix' }
if (!$Output) { $Output = Join-Path $repoRoot '.deps/overlay-keyboard-route' }
$Source = [IO.Path]::GetFullPath($Source)
$Output = [IO.Path]::GetFullPath($Output)
if ($ControlSource) { $ControlSource = [IO.Path]::GetFullPath($ControlSource) }
if (!$Architecture) {
  $extra = @()
  if ($VcVarsVer) { $extra += @('-VcVarsVer', $VcVarsVer) }
  if ($ControlSource) { $extra += @('-ControlSource', $ControlSource) }
  foreach ($arch in @('x86','x64')) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Source $Source -Output $Output -Architecture $arch @extra
    if ($LASTEXITCODE -ne 0) { throw "Overlay keyboard routing suite failed: $arch" }
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
New-Item -ItemType Directory -Force -Path $Output | Out-Null
[pscustomobject]@{
  architecture=$Architecture; compiler=$compiler;
  file_version=(Get-Item -LiteralPath $compiler).VersionInfo.FileVersion;
  vc_tools_version=$env:VCToolsVersion; windows_sdk_version=$env:WindowsSDKVersion;
  validation_scope='extracted production hook with deterministic Win32 adapters; no input injection or UI';
  formal_ci_toolset_equivalence=$false
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Output "compiler-$Architecture.json") -Encoding UTF8
$variants = @([pscustomobject]@{name='candidate';source=$Source;negative=$false})
if ($ControlSource) {
  $variants = @([pscustomobject]@{name='production-control';source=$ControlSource;negative=$true}) + $variants
}
$rows = @()
foreach ($variant in $variants) {
  $directory = Join-Path $Output "$Architecture/$($variant.name)"
  New-Item -ItemType Directory -Force -Path $directory | Out-Null
  & python (Join-Path $repoRoot 'tests/overlay_keyboard_route_generate.py') --source $variant.source --output $directory
  if ($LASTEXITCODE -ne 0) { throw "Production hook extraction failed: $($variant.name)" }
  Push-Location $directory
  try {
    $exe = Join-Path $directory 'overlay-keyboard-route.exe'
    & cl.exe /nologo /std:c++17 /EHsc /O2 /W4 /DNDEBUG "/I$directory" (Join-Path $repoRoot 'tests/overlay_keyboard_route.cpp') "/Fe:$exe" /link /LARGEADDRESSAWARE
    if ($LASTEXITCODE -ne 0) { throw "Native compile failed: $Architecture $($variant.name)" }
    $bytes = [IO.File]::ReadAllBytes($exe)
    $pe = [BitConverter]::ToInt32($bytes, 0x3c)
    $machine = [BitConverter]::ToUInt16($bytes, $pe + 4)
    $expectedMachine = if ($Architecture -eq 'x86') { 0x14c } else { 0x8664 }
    if ($machine -ne $expectedMachine) { throw "Wrong native architecture: $($variant.name)" }
    & cmd.exe /d /c 'overlay-keyboard-route.exe > run.log 2>&1'
    $code = $LASTEXITCODE
    $log = [IO.File]::ReadAllText((Join-Path $directory 'run.log'))
    Write-Output $log
    $expected = 'OVERLAY_KEYBOARD_ROUTE_FAIL: Win+Space down was swallowed or reposted'
    $pass = if ($variant.negative) { $code -eq 1 -and $log.Contains($expected) } else { $code -eq 0 -and $log.Contains('OVERLAY_KEYBOARD_ROUTE_PASS') }
    $rows += [pscustomobject]@{
      architecture=$Architecture; variant=$variant.name; exit_code=$code; passed=$pass;
      expected_failure=$variant.negative; exe_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash;
      fixture_sha256=(Get-FileHash -LiteralPath (Join-Path $repoRoot 'tests/overlay_keyboard_route.cpp') -Algorithm SHA256).Hash;
      output=$log
    }
    $rows | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Output "results-$Architecture.json") -Encoding UTF8
    if (!$pass) { throw "Unexpected native result: $Architecture $($variant.name) (exit $code)" }
  } finally { Pop-Location }
}
