param(
  [ValidateSet('x86','x64')][string]$CompileArchitecture,
  [string]$Source,
  [string]$Output
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
if (-not $Source) { $Source = Join-Path $repoRoot '.deps/dxvk-remix' }
if (-not $Output) { $Output = Join-Path $repoRoot '.deps/client-parameter-contract' }
$Source = [IO.Path]::GetFullPath($Source)
$Output = [IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Force $Output | Out-Null
if (-not $CompileArchitecture) {
  foreach ($arch in @('x86','x64')) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -CompileArchitecture $arch -Source $Source -Output $Output
    if ($LASTEXITCODE -ne 0) { throw "Client parameter contracts failed: $arch" }
  }
  return
}
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC build tools not found' }
$vcvars = Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
& cmd.exe /d /s /c "`"$vcvars`" $CompileArchitecture >nul && set" | ForEach-Object {
  if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Force -Path "env:$($Matches[1])" -Value $Matches[2] }
}
Push-Location $Output
try {
  foreach ($control in @('none','range','query','pixel-size')) {
    python "$repoRoot/tests/client_parameter_contract_generate.py" --source $Source --output $Output --control $control
    if ($LASTEXITCODE -ne 0) { throw 'Client method extraction failed' }
    $exe = "client-parameter-$CompileArchitecture-$control.exe"
    & cl.exe /nologo /std:c++17 /EHsc /O2 /W3 /DNDEBUG /DNOMINMAX "/I$Output" "$repoRoot/tests/client_parameter_contract.cpp" "/Fe:$exe"
    if ($LASTEXITCODE -ne 0) { throw "Client parameter compilation failed: $control" }
    $testOutput = & (Join-Path $Output $exe) 2>&1
    $code = $LASTEXITCODE
    $testOutput | Tee-Object -FilePath (Join-Path $Output "$exe.log") | Write-Output
    if ($control -eq 'none') {
      if ($code -ne 0) { throw 'Client parameter contract regression failed' }
    } else {
      $expected = switch ($control) { 'range' { 'wrapped constant range was accepted' } 'query' { 'pending query overwrote caller data' } 'pixel-size' { 'query SDK result size mismatch' } }
      if ($code -ne 1 -or ($testOutput -join "`n") -notmatch [regex]::Escape($expected)) {
        throw "Negative control failed to detect original regression: $control"
      }
    }
  }
  python "$repoRoot/tests/client_parameter_contract_generate.py" --source $Source --output $Output
  if ($LASTEXITCODE -ne 0) { throw 'Could not restore production extraction' }
} finally { Pop-Location }

