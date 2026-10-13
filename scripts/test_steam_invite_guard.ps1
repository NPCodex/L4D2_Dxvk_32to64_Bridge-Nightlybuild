# SPDX-License-Identifier: MIT
param(
  [string]$SourceDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) '.deps/dxvk-remix'),
  [string]$OutputDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) '.deps/steam-invite-guard-test')
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$source = (Resolve-Path -LiteralPath $SourceDirectory).Path
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$testDir = (Resolve-Path -LiteralPath $OutputDirectory).Path
. (Join-Path $PSScriptRoot 'assert_native_architecture.ps1')
. (Join-Path $PSScriptRoot 'setup_native_test_compiler.ps1')
Set-NativeTestCompiler -Architecture 'x86'
$control = Join-Path $testDir 'old-guard-headers'
& python "$PSScriptRoot/generate_steam_native_controls.py" --source $source --output $control --mode invite-old-guard
if ($LASTEXITCODE -ne 0) { throw 'Exact invitation old-guard control generation failed' }
$results = @()
Push-Location $testDir
try {
  foreach ($variant in @('production', 'old-guard')) {
    $include = if ($variant -eq 'production') { "$source/bridge/src/util" } else { $control }
    $name = "invite-$variant.exe"
    & cl.exe /nologo /std:c++17 /EHsc /O2 /MT /W4 /WX "/I$include" "/I$source/bridge/src/util" "$repoRoot/tests/steam_invite_guard.cpp" "/Fe:$name"
    if ($LASTEXITCODE -ne 0) { throw "Invitation guard compilation failed: $variant" }
    Assert-NativeArchitecture -Path (Join-Path $testDir $name) -Architecture 'x86'
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo.FileName = Join-Path $testDir $name
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    try {
      if (!$process.Start()) { throw 'Cannot start invitation guard fixture' }
      $output = $process.StandardOutput.ReadToEnd() + $process.StandardError.ReadToEnd()
      $process.WaitForExit()
      $code = $process.ExitCode
    } finally { $process.Dispose() }
    $output | Set-Content -LiteralPath (Join-Path $testDir "$variant.txt") -Encoding utf8
    Write-Host $output
    $expected = if ($variant -eq 'production') { 0 } else { 43 }
    if ($code -ne $expected -or ($variant -eq 'old-guard' -and $output -notmatch 'FAIL invitation helper availability rejected at 0x11a03; three prior guard sites passed')) {
      throw "Invitation guard regression did not produce its exact expected result: $variant exit=$code"
    }
    $results += @{ variant=$variant; architecture='x86'; exit_code=$code; expected_exit_code=$expected; passed=$true }
  }
  @{ source=$source; source_header_sha256=(Get-FileHash "$source/bridge/src/util/steam_workshop_status.h" -Algorithm SHA256).Hash.ToLower(); tests=$results } |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $testDir 'results.json') -Encoding utf8
} finally { Pop-Location }
