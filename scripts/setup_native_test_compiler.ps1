# SPDX-License-Identifier: MIT
# Native regression tools use an installed MSVC compiler in a fresh process.
function Set-NativeTestCompiler {
  param([ValidateSet('x86','x64')][string]$Architecture)
  $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
  if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio installer locator not found' }
  $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if ($LASTEXITCODE -ne 0 -or !$installation) { throw 'MSVC build tools not found' }
  $vcvars = Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
  & cmd.exe /d /s /c "`"$vcvars`" $Architecture >nul && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Force -Path "env:$($Matches[1])" -Value $Matches[2] }
  }
  if ($LASTEXITCODE -ne 0) { throw 'MSVC environment initialization failed' }
  if (!(Get-Command cl.exe -ErrorAction SilentlyContinue)) { throw 'MSVC compiler is unavailable after initialization' }
}
