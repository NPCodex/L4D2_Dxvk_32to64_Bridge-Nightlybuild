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
if (!$Output) { $Output = Join-Path $repoRoot '.deps/remix-material-contract' }
$Source = [IO.Path]::GetFullPath($Source)
$Output = [IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Force -Path $Output | Out-Null
if (!$Architecture) {
  $compilerArgs = @('-VcVarsVer', $VcVarsVer)
  if ($UseInstalledCompiler) { $compilerArgs = @('-UseInstalledCompiler') }
  foreach ($arch in @('x86','x64')) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Source $Source -Output $Output -Architecture $arch @compilerArgs
    if ($LASTEXITCODE -ne 0) { throw "Material native build failed: $arch" }
  }
  $rows = @()
  foreach ($producer in @('x86','x64')) {
    foreach ($consumer in @('x86','x64')) {
      foreach ($variant in @('none','transform-order')) {
        $fixture = Join-Path $Output "$producer/client-$variant/fixtures.bin"
        $exe = Join-Path $Output "$consumer/server-none/remix-material.exe"
        $logPath = Join-Path $Output "cross-$producer-$consumer-$variant.log"
        $start = New-Object System.Diagnostics.ProcessStartInfo
        $start.FileName = $exe
        $start.Arguments = '"' + $fixture + '"'
        $start.UseShellExecute = $false
        $start.CreateNoWindow = $true
        $start.RedirectStandardOutput = $true
        $start.RedirectStandardError = $true
        $process = [Diagnostics.Process]::Start($start)
        $stdout = $process.StandardOutput.ReadToEnd()
        $stderr = $process.StandardError.ReadToEnd()
        $process.WaitForExit()
        $code = $process.ExitCode
        $log = $stdout + $stderr
        $process.Dispose()
        [IO.File]::WriteAllText($logPath, $log)
        Write-Output $log
        $pass = if ($variant -eq 'none') { $code -eq 0 -and $log.Contains('REMIX_MATERIAL_PASS role=server scenarios=12') } else { $code -eq 1 -and $log.Contains('REMIX_MATERIAL_FAIL: field mismatch: textureRotation') }
        $rows += [pscustomobject]@{producer=$producer;consumer=$consumer;variant=$variant;exit_code=$code;passed=$pass;expected_failure=($variant -ne 'none');fixture_sha256=(Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash.ToLowerInvariant();exe_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant();log=(Split-Path $logPath -Leaf);output=$log}
        $rows | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Output 'cross-results.json') -Encoding UTF8
        if (!$pass) { throw "Material cross-architecture result unexpected: $producer -> $consumer / $variant" }
      }
    }
  }
  # The wire contract must be architecture-independent, including full framing.
  $x86 = (Get-FileHash -LiteralPath (Join-Path $Output 'x86/client-none/fixtures.bin') -Algorithm SHA256).Hash
  $x64 = (Get-FileHash -LiteralPath (Join-Path $Output 'x64/client-none/fixtures.bin') -Algorithm SHA256).Hash
  if ($x86 -ne $x64) { throw 'x86/x64 material wire fixtures differ' }
  [pscustomobject]@{schema=1;passed=$true;scenario_count=12;cross_pairings=4;negative_cross_pairings=4;architecture_identical_fixture_sha256=$x86.ToLowerInvariant();completed_utc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Output 'completed.json') -Encoding UTF8
  return
}
if ($UseInstalledCompiler) {
  . (Join-Path $PSScriptRoot 'setup_native_test_compiler.ps1')
  Set-NativeTestCompiler -Architecture $Architecture
} else {
  . (Join-Path $Source 'bridge/build_common.ps1')
  SetupVS -Platform $Architecture -VcVarsVer $VcVarsVer
  if (!$env:VCToolsVersion -or !$env:VCToolsVersion.StartsWith($VcVarsVer)) { throw "MSVC $VcVarsVer was not initialized" }
}
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source
[pscustomobject]@{architecture=$Architecture;compiler=$compiler;file_version=(Get-Item -LiteralPath $compiler).VersionInfo.FileVersion;vc_tools_version=$env:VCToolsVersion;windows_sdk_version=$env:WindowsSDKVersion;formal_ci_toolset_equivalence=(!$UseInstalledCompiler -and $env:VCToolsVersion.StartsWith('14.29'))} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Output "compiler-$Architecture.json") -Encoding UTF8
$harnessHashes = [ordered]@{}
foreach ($name in @('tests/remix_material_contract.cpp','tests/remix_material_contract_generate.py','scripts/test_remix_material.ps1','scripts/native_source.py')) {
  $harnessHashes[$name] = (Get-FileHash -LiteralPath (Join-Path $repoRoot $name) -Algorithm SHA256).Hash.ToLowerInvariant()
}
$commit = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Unable to record harness repository commit' }
[pscustomobject]@{schema=1;architecture=$Architecture;repository_commit=$commit;harness_sha256=$harnessHashes;recorded_utc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Output "provenance-$Architecture.json") -Encoding UTF8
$rows = @()
foreach ($item in @(@('client','none'),@('client','prefix-overread'),@('client','transform-order'),@('server','none'))) {
  $role = $item[0]; $variant = $item[1]
  $directory = Join-Path $Output "$Architecture/$role-$variant"
  New-Item -ItemType Directory -Force -Path $directory | Out-Null
  & python (Join-Path $repoRoot 'tests/remix_material_contract_generate.py') --source $Source --output $directory --role $role --control $variant
  if ($LASTEXITCODE -ne 0) { throw "Material source extraction failed: $role $variant" }
  Push-Location $directory
  try {
    $exe = Join-Path $directory 'remix-material.exe'
    $define = if ($role -eq 'client') { '/DMATERIAL_CLIENT' } else { '/DMATERIAL_SERVER' }
    & cl.exe /nologo /std:c++17 /EHsc /O2 /W4 /DNDEBUG $define "/I$directory" "/I$Source/bridge/src/util" "/I$Source/public/include" (Join-Path $repoRoot 'tests/remix_material_contract.cpp') "/Fe:$exe" /link /LARGEADDRESSAWARE *> compile.log
    if ($LASTEXITCODE -ne 0) { Get-Content compile.log; throw "Material native compile failed: $Architecture $role $variant" }
    $bytes = [IO.File]::ReadAllBytes($exe); $pe = [BitConverter]::ToInt32($bytes, 0x3c)
    $machine = [BitConverter]::ToUInt16($bytes, $pe + 4)
    $expectedMachine = if ($Architecture -eq 'x86') { 0x14c } else { 0x8664 }
    if ($machine -ne $expectedMachine) { throw 'Wrong material executable architecture' }
    if ($role -eq 'client') {
      & cmd.exe /d /c 'remix-material.exe fixtures.bin > run.log 2>&1'
      $code = $LASTEXITCODE
      $log = [IO.File]::ReadAllText((Join-Path $directory 'run.log'))
      Write-Output $log
      $pass = if ($variant -eq 'prefix-overread') { $code -eq 1 -and $log.Contains('REMIX_MATERIAL_FAIL: guard page access') } else { $code -eq 0 -and $log.Contains('REMIX_MATERIAL_PASS role=client scenarios=12') }
      $rows += [pscustomobject]@{architecture=$Architecture;role=$role;variant=$variant;exit_code=$code;passed=$pass;expected_failure=($variant -eq 'prefix-overread');exe_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant();output=$log}
      $rows | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Output "results-$Architecture.json") -Encoding UTF8
      if (!$pass) { throw "Unexpected material producer result: $Architecture $variant" }
    }
  } finally { Pop-Location }
}
