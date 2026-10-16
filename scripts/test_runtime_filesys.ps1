# SPDX-License-Identifier: MIT
param(
  [string]$Source,
  [string]$Output,
  [string]$VcVarsVer = '14.29',
  [ValidateSet('x86','x64')][string]$Architecture,
  [switch]$RealOnly
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
if (!$Source) { $Source = Join-Path $repoRoot '.deps/dxvk-remix' }
if (!$Output) { $Output = Join-Path $repoRoot '.deps/runtime-filesys' }
$Source = [IO.Path]::GetFullPath($Source)
$Output = [IO.Path]::GetFullPath($Output)
if (!$Architecture) {
  foreach ($arch in @('x86','x64')) {
    $extra = @()
    if ($RealOnly) { $extra = @('-RealOnly') }
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Source $Source -Output $Output -VcVarsVer $VcVarsVer -Architecture $arch @extra
    if ($LASTEXITCODE -ne 0) { throw "Runtime filesystem failed: $arch" }
  }
  return
}
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or !$installation) { throw 'MSVC build tools not found' }
$vcvars = Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
& cmd.exe /d /s /c "`"$vcvars`" $Architecture -vcvars_ver=$VcVarsVer >nul && set" | ForEach-Object {
  if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Force -Path "env:$($Matches[1])" -Value "$($Matches[2])" }
}
if ($LASTEXITCODE -ne 0 -or !$env:VCToolsVersion -or !$env:VCToolsVersion.StartsWith($VcVarsVer)) { throw "Requested toolset $VcVarsVer not initialized" }
$directory = Join-Path $Output $Architecture
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source
$harnessHashes = [ordered]@{}
foreach ($relativePath in @('tests/runtime_filesys.cpp', 'tests/runtime_filesys_extract.py', 'scripts/test_runtime_filesys.ps1', 'scripts/native_source.py')) {
  $harnessHashes[$relativePath] = (Get-FileHash -LiteralPath (Join-Path $repoRoot $relativePath) -Algorithm SHA256).Hash.ToLowerInvariant()
}
$repositoryCommit = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Unable to record harness repository commit' }
[pscustomobject]@{schema=1;architecture=$Architecture;repository_commit=$repositoryCommit;harness_sha256=$harnessHashes;recorded_utc=[DateTime]::UtcNow.ToString('o')} |
  ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Output "provenance-$Architecture.json") -Encoding UTF8
[pscustomobject]@{ source=$Source; architecture=$Architecture; compiler=$compiler; file_version=(Get-Item -LiteralPath $compiler).VersionInfo.FileVersion; vc_tools_version=$env:VCToolsVersion; windows_sdk_version=$env:WindowsSDKVersion; formal_ci_toolset_equivalence=$env:VCToolsVersion.StartsWith('14.29') } |
  ConvertTo-Json | Set-Content -LiteralPath (Join-Path $directory 'provenance.json') -Encoding UTF8
$expectedFailures = @{
  'no-root-routing' = 'root and bin clients resolve package bin'
  'no-fallback' = 'native preference and x64 fallback'
  'false-success' = 'failed startup must return false'
}
$rows = @()
$variants = if ($RealOnly) { @('real') } else { @('real','no-root-routing','no-fallback','false-success') }
foreach ($variant in $variants) {
  $build = Join-Path $directory $variant
  New-Item -ItemType Directory -Force -Path $build | Out-Null
  & python (Join-Path $repoRoot 'tests/runtime_filesys_extract.py') --source $Source --output $build --variant $variant
  if ($LASTEXITCODE -ne 0) { throw 'Filesystem source extraction failed' }
  $exe = Join-Path $build 'runtime-filesys.exe'
  Push-Location $build
  try {
    & cl.exe /nologo /std:c++17 /EHsc /O2 /W4 /WX /DNOMINMAX /wd4505 "/I$build" (Join-Path $repoRoot 'tests/runtime_filesys.cpp') "/Fe:$exe" "/Fo:$build/runtime-filesys.obj" 2>&1 | Tee-Object -FilePath (Join-Path $build 'compile.log')
    if ($LASTEXITCODE -ne 0) { throw 'Filesystem fixture compilation failed' }
  } finally { Pop-Location }
  $bytes = [IO.File]::ReadAllBytes($exe)
  $pe = [BitConverter]::ToInt32($bytes, 0x3c)
  $machine = [BitConverter]::ToUInt16($bytes, $pe + 4)
  $expected = if ($Architecture -eq 'x86') { 0x14c } else { 0x8664 }
  if ($machine -ne $expected) { throw 'Wrong native fixture architecture' }
  $modes = switch ($variant) {
    'real' { @('bin-client','root-client','unicode-root-client','native-fallback','native-preferred','missing-root','missing-remix','missing-executable','reinitialize') }
    'no-root-routing' { @('root-client') }
    'no-fallback' { @('native-fallback') }
    'false-success' { @('missing-root') }
  }
  foreach ($mode in $modes) {
    $working = Join-Path $build $mode
    New-Item -ItemType Directory -Force -Path $working | Out-Null
    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = $exe; $start.Arguments = $mode; $start.WorkingDirectory = $working
    $start.UseShellExecute = $false; $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $start
    if (!$process.Start()) { throw 'Filesystem fixture failed to start' }
    $stdout = $process.StandardOutput.ReadToEndAsync(); $stderr = $process.StandardError.ReadToEndAsync()
    $timedOut = !$process.WaitForExit(30000)
    if ($timedOut) { $process.Kill(); $process.WaitForExit() }
    $code = $process.ExitCode; $log = $stdout.Result + $stderr.Result; $process.Dispose()
    $log | Set-Content -LiteralPath (Join-Path $build "$mode.log") -Encoding UTF8
    Write-Output $log.Trim()
    $pass = if ($variant -eq 'real') { !$timedOut -and $code -eq 0 -and $log.Contains('RUNTIME_FILESYS_PASS') } else {
      !$timedOut -and $code -eq 1 -and $log.Contains("RUNTIME_FILESYS_FAIL mode=$mode ") -and $log.Contains("reason=$($expectedFailures[$variant])")
    }
    $rows += [pscustomobject]@{ architecture=$Architecture; variant=$variant; mode=$mode; passed=$pass; expected_failure=($variant -ne 'real'); expected_failure_reason=$expectedFailures[$variant]; timed_out=$timedOut; exit_code=$code; output=$log; exe_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash }
    $rows | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $directory 'results.json') -Encoding UTF8
    if (!$pass) { throw "Runtime filesystem regression failed: $Architecture $variant $mode" }
  }
}
