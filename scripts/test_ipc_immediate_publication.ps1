# SPDX-License-Identifier: MIT
# Keep compiler environments isolated and pin the formal CI suite to production MSVC 14.29.
param(
  [Alias('SourceDirectory')][string]$Source,
  [Alias('OutputDirectory')][string]$Output,
  [string]$VcVarsVersion,
  [ValidateSet('x86','x64')][string]$CompileArchitecture
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$Source) { $Source = Join-Path $root '.deps/dxvk-remix' }
if (!$Output) { $Output = Join-Path $root 'work/ipc-full-native/immediate-publication' }
$Source = [IO.Path]::GetFullPath($Source)
$Output = [IO.Path]::GetFullPath($Output)
if (!$VcVarsVersion) { $VcVarsVersion = if ($env:GITHUB_ACTIONS -eq 'true') { '14.29' } else { 'latest' } }
if ($VcVarsVersion -ne 'latest' -and $VcVarsVersion -notmatch '^\d+\.\d+(\.\d+)?$') {
  throw 'VcVarsVersion must be latest or a numeric MSVC toolset version'
}
if ($env:GITHUB_ACTIONS -eq 'true' -and $VcVarsVersion -notlike '14.29*') {
  throw 'Formal immediate-publication CI requires MSVC 14.29'
}
if ($CompileArchitecture) {
  if ($VcVarsVersion -eq 'latest') {
    . (Join-Path $PSScriptRoot 'setup_native_test_compiler.ps1')
    Set-NativeTestCompiler -Architecture $CompileArchitecture
  } else {
    $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
    if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio locator unavailable' }
    $installations = & $vswhere -all -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0) { throw 'Visual Studio lookup failed' }
    $installation = $installations | Where-Object {
      @(Get-ChildItem -LiteralPath (Join-Path $_ 'VC/Tools/MSVC') -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq $VcVarsVersion -or $_.Name.StartsWith("$VcVarsVersion.") }).Count -gt 0
    } | Select-Object -First 1
    if (!$installation) { throw "Requested MSVC $VcVarsVersion is not installed" }
    $vcvars = Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
    & cmd.exe /d /s /c "`"$vcvars`" $CompileArchitecture -vcvars_ver=$VcVarsVersion >nul && set" | ForEach-Object {
      if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Force -Path "env:$($Matches[1])" -Value $Matches[2] }
    }
    if ($LASTEXITCODE -ne 0) { throw 'Pinned MSVC initialization failed' }
    if (!$env:VCToolsVersion.TrimEnd('\').StartsWith($VcVarsVersion)) { throw 'Selected MSVC toolset differs from request' }
  }
  $compiler = (Get-Command cl.exe -ErrorAction Stop).Source
  $compilerVersion = (Get-Item -LiteralPath $compiler).VersionInfo.FileVersion
  [ordered]@{ architecture=$CompileArchitecture; requested=$VcVarsVersion; compiler=$compiler;
    compiler_file_version=$compilerVersion; vc_tools_version=$env:VCToolsVersion } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Output "compiler-$CompileArchitecture.json") -Encoding UTF8
  . (Join-Path $PSScriptRoot 'assert_native_architecture.ps1')
  foreach ($variant in @('production','negative-no-fallback')) {
    $fixture = Join-Path $Output $variant
    $build = Join-Path $fixture $CompileArchitecture
    New-Item -ItemType Directory -Path $build -Force | Out-Null
    Push-Location $build
    try {
      & cl.exe /nologo /std:c++17 /EHsc /O2 /MT /DNDEBUG /W3 /DNOMINMAX /DWIN32 "/I$fixture" `
        "$fixture/ipc_transport.cpp" "$fixture/util_sharedmemory.cpp" "$fixture/util_semaphore.cpp" `
        "$fixture/data_diagnostics.cpp" "$fixture/exception_diagnostics.cpp" /Fe:immediate-publication.exe `
        /link /LARGEADDRESSAWARE ole32.lib psapi.lib 2>&1 |
        Tee-Object -FilePath (Join-Path $Output "compile-$variant-$CompileArchitecture.txt")
      if ($LASTEXITCODE -ne 0) { throw "IPC immediate-publication compile failed: $variant $CompileArchitecture" }
      Assert-NativeArchitecture -Path (Join-Path $build 'immediate-publication.exe') -Architecture $CompileArchitecture
    } finally { Pop-Location }
  }
} else {
  & python (Join-Path $PSScriptRoot 'generate_ipc_immediate_publication_test.py') --source $Source --output $Output
  if ($LASTEXITCODE -ne 0) { throw 'IPC immediate-publication extraction failed' }
  foreach ($arch in @('x86','x64')) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Source $Source -Output $Output `
      -VcVarsVersion $VcVarsVersion -CompileArchitecture $arch
    if ($LASTEXITCODE -ne 0) { throw "IPC immediate-publication build failed: $arch" }
  }
  & python (Join-Path $PSScriptRoot 'run_ipc_immediate_publication_tests.py') --output $Output
  if ($LASTEXITCODE -ne 0) { throw 'IPC immediate-publication native regression failed' }
}
