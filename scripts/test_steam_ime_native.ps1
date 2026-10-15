# SPDX-License-Identifier: MIT
param(
  [string]$Source,
  [string]$Output,
  [string]$VcVarsVer = '14.29',
  [ValidateSet('x86','x64')][string]$Architecture
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
if (!$Source) { $Source = Join-Path $repoRoot '.deps/dxvk-remix' }
if (!$Output) { $Output = Join-Path $repoRoot '.deps/steam-ime-native' }
$Source = [IO.Path]::GetFullPath($Source)
$Output = [IO.Path]::GetFullPath($Output)
if (!$Architecture) {
  foreach ($arch in @('x86','x64')) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Source $Source -Output $Output -VcVarsVer $VcVarsVer -Architecture $arch
    if ($LASTEXITCODE -ne 0) { throw "Steam IME lifecycle failed: $arch" }
  }
  return
}
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio installer locator not found' }
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or !$installation) { throw 'MSVC build tools not found' }
$vcvars = Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
& cmd.exe /d /s /c "`"$vcvars`" $Architecture -vcvars_ver=$VcVarsVer >nul && set" | ForEach-Object {
  if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Force -Path "env:$($Matches[1])" -Value "$($Matches[2])" }
}
if ($LASTEXITCODE -ne 0 -or !$env:VCToolsVersion -or !$env:VCToolsVersion.StartsWith($VcVarsVer)) {
  throw "Requested MSVC toolset $VcVarsVer was not initialized"
}
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source
$directory = Join-Path $Output $Architecture
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$helper = Join-Path $Source 'bridge/src/server/steam_ime_native.h'
$fixture = Join-Path $repoRoot 'tests/steam_ime_native.cpp'
[pscustomobject]@{
  architecture=$Architecture; compiler=$compiler; compiler_version=(Get-Item -LiteralPath $compiler).VersionInfo.FileVersion
  vc_tools_version=$env:VCToolsVersion; windows_sdk_version=$env:WindowsSDKVersion
  source=$Source; helper_sha256=(Get-FileHash -LiteralPath $helper -Algorithm SHA256).Hash
  fixture_sha256=(Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
  runner_sha256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $directory 'provenance.json') -Encoding UTF8
$rows = @()
Push-Location $directory
try {
  foreach ($variant in @('real','faults')) {
    $exe = Join-Path $directory "ime-$variant.exe"
    $defines = @()
    if ($variant -eq 'faults') { $defines = @('/DIME_FAULT_INJECTION') }
    & cl.exe /nologo /std:c++17 /EHsc /O2 /W4 /WX /DNOMINMAX @defines "/I$Source/bridge/src/server" $fixture user32.lib ole32.lib "/Fe:$exe" "/Fo:$directory/ime-$variant.obj" 2>&1 | Tee-Object -FilePath (Join-Path $directory "compile-$variant.log")
    if ($LASTEXITCODE -ne 0) { throw "IME lifecycle compilation failed: $Architecture $variant" }
    $bytes = [IO.File]::ReadAllBytes($exe)
    $pe = [BitConverter]::ToInt32($bytes, 0x3c)
    $machine = [BitConverter]::ToUInt16($bytes, $pe + 4)
    $expectedMachine = if ($Architecture -eq 'x86') { 0x14c } else { 0x8664 }
    if ($machine -ne $expectedMachine) { throw 'Wrong IME native architecture' }
    $modes = if ($variant -eq 'real') { @('disabled','fresh','sta','mta','nested') } else {
      @('missing-ole','missing-init','missing-uninit','init-fail','missing-ctf','missing-get','get-fail','missing-create','create-fail','activate-fail','success','existing-sta')
    }
    foreach ($mode in $modes) {
      $logPath = Join-Path $directory "$variant-$mode.log"
      $start = New-Object System.Diagnostics.ProcessStartInfo
      $start.FileName = $exe
      $start.Arguments = $mode
      $start.UseShellExecute = $false
      $start.CreateNoWindow = $true
      $start.RedirectStandardOutput = $true
      $start.RedirectStandardError = $true
      $process = New-Object System.Diagnostics.Process
      $process.StartInfo = $start
      if (!$process.Start()) { throw 'IME lifecycle process failed to start' }
      $stdout = $process.StandardOutput.ReadToEndAsync()
      $stderr = $process.StandardError.ReadToEndAsync()
      $timedOut = !$process.WaitForExit(30000)
      if ($timedOut) { $process.Kill(); $process.WaitForExit() }
      $code = $process.ExitCode
      $log = $stdout.Result + $stderr.Result
      $process.Dispose()
      $log | Set-Content -LiteralPath $logPath -Encoding UTF8
      Write-Output $log.Trim()
      $pass = !$timedOut -and $code -eq 0 -and $log.Contains('IME_NATIVE_PASS')
      $rows += [pscustomobject]@{
        architecture=$Architecture; variant=$variant; mode=$mode; exit_code=$code; timed_out=$timedOut; passed=$pass
        exe_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash; output=$log
      }
      $rows | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $directory 'results.json') -Encoding UTF8
      if (!$pass) { throw "IME lifecycle failed: $Architecture $variant $mode" }
    }
  }
} finally { Pop-Location }
