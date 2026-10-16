param(
  [string]$Source,
  [string]$ReShadeSource,
  [string]$Output,
  [ValidateSet('x86','x64')][string]$Architecture,
  [string]$VcVarsVer = '14.29',
  [switch]$UseInstalledCompiler
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
if (!$Source) { $Source = Join-Path $repoRoot '.deps/dxvk-remix' }
if (!$ReShadeSource) { $ReShadeSource = Join-Path $repoRoot '.deps/reshade-keyboard-source' }
if (!$Output) { $Output = Join-Path $repoRoot '.deps/overlay-reshade-keyboard' }
$Source = [IO.Path]::GetFullPath($Source)
$ReShadeSource = [IO.Path]::GetFullPath($ReShadeSource)
$Output = [IO.Path]::GetFullPath($Output)
function Get-BytesHash([string]$Path) {
  $sha = [Security.Cryptography.SHA256]::Create()
  try { return [BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($Path))).Replace('-','').ToLowerInvariant() }
  finally { $sha.Dispose() }
}
if (!$Architecture) {
  $failed = $false
  foreach ($arch in @('x86','x64')) {
    $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',$PSCommandPath,'-Source',$Source,'-ReShadeSource',$ReShadeSource,'-Output',$Output,'-Architecture',$arch,'-VcVarsVer',$VcVarsVer)
    if ($UseInstalledCompiler) { $arguments += '-UseInstalledCompiler' }
    & powershell.exe @arguments
    if ($LASTEXITCODE -ne 0) { $failed = $true }
  }
  if ($failed) { exit 1 }; exit 0
}
if ($UseInstalledCompiler) {
  . (Join-Path $PSScriptRoot 'setup_native_test_compiler.ps1')
  Set-NativeTestCompiler -Architecture $Architecture
} else {
  . (Join-Path $Source 'bridge/build_common.ps1')
  SetupVS -Platform $Architecture -VcVarsVer $VcVarsVer
  if (!$env:VCToolsVersion.StartsWith($VcVarsVer)) { throw 'Unexpected compiler toolset' }
}
$directory = Join-Path $Output $Architecture
New-Item -ItemType Directory -Force -Path $directory | Out-Null
& python (Join-Path $repoRoot 'tests/overlay_reshade_keyboard_generate.py') --source $Source --reshade-source $ReShadeSource --output $directory
if ($LASTEXITCODE -ne 0) { throw 'Source extraction failed' }
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source
$hashes = @{}
foreach ($p in @('tests/overlay_reshade_keyboard.cpp','tests/overlay_reshade_keyboard_generate.py','scripts/test_overlay_reshade_keyboard.ps1','scripts/native_source.py','scripts/fetch_reshade_keyboard_source.py')) { $hashes[$p] = Get-BytesHash (Join-Path $repoRoot $p) }
[pscustomobject]@{architecture=$Architecture;compiler=$compiler;file_version=(Get-Item -LiteralPath $compiler).VersionInfo.FileVersion;vc_tools_version=$env:VCToolsVersion;formal_ci_toolset_equivalence=(!$UseInstalledCompiler -and $env:VCToolsVersion.StartsWith('14.29'));harness_sha256=$hashes;scope='No-UI deterministic native/post queue model with extracted Presenter and ReShade 6.8.0 bodies'} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $directory 'provenance.json') -Encoding UTF8
Push-Location $directory
try {
  & cl.exe /nologo /std:c++17 /EHsc /W4 /WX /utf-8 /O2 "/I$directory" (Join-Path $repoRoot 'tests/overlay_reshade_keyboard.cpp') /Fe:overlay-reshade-keyboard.exe /link user32.lib > compile.log 2>&1
  if ($LASTEXITCODE -ne 0) { Get-Content compile.log; throw 'Fixture compilation failed' }
  $exe = Join-Path $directory 'overlay-reshade-keyboard.exe'
  $bytes = [IO.File]::ReadAllBytes($exe); $pe = [BitConverter]::ToInt32($bytes,0x3c); $machine = [BitConverter]::ToUInt16($bytes,$pe+4)
  $expectedMachine = if ($Architecture -eq 'x86') { 0x14c } else { 0x8664 }
  if ($machine -ne $expectedMachine) { throw 'Wrong executable architecture' }
  $rows = @(); $failed = $false
  foreach ($case in @('home-direct','home-wrapper','home-wrapper-no-steam','home-wrapper-active-widget','home-wrapper-ignore-shortcuts','home-wrapper-repeat','fallback-home','steam-direct','steam-wrapper')) {
    & cmd.exe /d /c "overlay-reshade-keyboard.exe $case > $case.log 2>&1"
    $code = $LASTEXITCODE; $log = [IO.File]::ReadAllText((Join-Path $directory "$case.log"))
    $passed = $code -eq 0 -and $log.Contains("RESHade_KEYBOARD_PASS case=$case")
    $required = !$case.StartsWith('steam-')
    if (!$passed -and $required) { $failed = $true }
    $rows += [pscustomobject]@{architecture=$Architecture;case=$case;required=$required;exit_code=$code;passed=$passed;exe_sha256=Get-BytesHash $exe;output=$log}
    Write-Output $log
  }
  $rows | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $directory 'results.json') -Encoding UTF8
  if ($failed) { exit 1 }
} finally { Pop-Location }
