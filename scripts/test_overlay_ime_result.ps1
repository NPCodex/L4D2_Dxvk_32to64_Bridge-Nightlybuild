param(
  [string]$Source,
  [string]$Output,
  [string]$PreFixSource,
  [ValidateSet('14.29','14.51')][string]$VcVarsVer = '14.29',
  [ValidateSet('x86','x64')][string]$Architecture
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
if (!$Source) { $Source = Join-Path $repoRoot '.deps/dxvk-remix' }
if (!$Output) { $Output = Join-Path $repoRoot '.deps/overlay-ime-result' }
$Source = [IO.Path]::GetFullPath($Source)
$Output = [IO.Path]::GetFullPath($Output)
if ($PreFixSource) { $PreFixSource = [IO.Path]::GetFullPath($PreFixSource) }
if (!$Architecture) {
  $extra = @()
  if ($PreFixSource) { $extra += @('-PreFixSource', $PreFixSource) }
  foreach ($arch in @('x86','x64')) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Source $Source -Output $Output -Architecture $arch -VcVarsVer $VcVarsVer @extra
    if ($LASTEXITCODE -ne 0) { throw "Overlay IME result suite failed: $arch" }
  }
  return
}
if ($VcVarsVer -eq '14.51') {
  # The upstream setup excludes VS 18, which supplies the local 14.51 toolset.
  . (Join-Path $PSScriptRoot 'setup_native_test_compiler.ps1')
  Set-NativeTestCompiler -Architecture $Architecture
} else {
  . (Join-Path $Source 'bridge/build_common.ps1')
  SetupVS -Platform $Architecture -VcVarsVer $VcVarsVer
}
if (!$env:VCToolsVersion -or !$env:VCToolsVersion.StartsWith($VcVarsVer)) {
  throw "Requested MSVC toolset $VcVarsVer was not initialized"
}
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source
New-Item -ItemType Directory -Force -Path $Output | Out-Null
[pscustomobject]@{
  architecture=$Architecture; compiler=$compiler;
  file_version=(Get-Item -LiteralPath $compiler).VersionInfo.FileVersion;
  requested_toolset=$VcVarsVer; vc_tools_version=$env:VCToolsVersion; windows_sdk_version=$env:WindowsSDKVersion;
  validation_scope='extracted production windowProc and helpers; no HWNDs, input injection, UI or game';
  formal_ci_toolset_equivalence=$false
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Output "compiler-$Architecture.json") -Encoding UTF8
$variants = @([pscustomobject]@{name='candidate';source=$Source;negative=$false})
if ($PreFixSource) {
  # Accept an exact saved pre-fix header or a source tree. Never rewrite the
  # candidate to manufacture a passing control; the manifest records its hash.
  $variants = @([pscustomobject]@{name='pre-fix-control';source=$PreFixSource;negative=$true}) + $variants
}
$rows = @()
foreach ($variant in $variants) {
  $directory = Join-Path $Output "$Architecture/$($variant.name)"
  New-Item -ItemType Directory -Force -Path $directory | Out-Null
  & python (Join-Path $repoRoot 'tests/overlay_ime_result_generate.py') --source $variant.source --dependencies (Join-Path $Source 'bridge/src/util') --output $directory
  if ($LASTEXITCODE -ne 0) { throw "Production windowProc extraction failed: $($variant.name)" }
  Push-Location $directory
  try {
    $exe = Join-Path $directory 'ime-result-boundary.exe'
    & cl.exe /nologo /std:c++17 /EHsc /O2 /W4 /DNDEBUG "/I$directory" (Join-Path $repoRoot 'tests/overlay_ime_result.cpp') "/Fe:$exe" /link user32.lib /LARGEADDRESSAWARE 2>&1 | Tee-Object -FilePath (Join-Path $directory 'compile.log')
    if ($LASTEXITCODE -ne 0) { throw "Native compile failed: $Architecture $($variant.name)" }
    $bytes = [IO.File]::ReadAllBytes($exe)
    $pe = [BitConverter]::ToInt32($bytes, 0x3c)
    $machine = [BitConverter]::ToUInt16($bytes, $pe + 4)
    $expectedMachine = if ($Architecture -eq 'x86') { 0x14c } else { 0x8664 }
    if ($machine -ne $expectedMachine) { throw "Wrong native architecture: $($variant.name)" }
    & cmd.exe /d /c 'ime-result-boundary.exe > run.log 2>&1'
    $code = $LASTEXITCODE
    $log = [IO.File]::ReadAllText((Join-Path $directory 'run.log'))
    Write-Output $log
    $expected = 'IME_RESULT_BOUNDARY_FAIL: native Steam result reached DefWindowProc assertions=1'
    $pass = if ($variant.negative) { $code -eq 1 -and $log.Contains($expected) } else { $code -eq 0 -and $log.Contains('IME_RESULT_BOUNDARY_PASS') }
    $rows += [pscustomobject]@{
      architecture=$Architecture; variant=$variant.name; exit_code=$code; passed=$pass;
      expected_failure=$variant.negative; exe_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash;
      fixture_sha256=(Get-FileHash -LiteralPath (Join-Path $repoRoot 'tests/overlay_ime_result.cpp') -Algorithm SHA256).Hash;
      generator_sha256=(Get-FileHash -LiteralPath (Join-Path $repoRoot 'tests/overlay_ime_result_generate.py') -Algorithm SHA256).Hash;
      output=$log
    }
    $rows | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Output "results-$Architecture.json") -Encoding UTF8
    if (!$pass) { throw "Unexpected native result: $Architecture $($variant.name) (exit $code)" }
  } finally { Pop-Location }
}
