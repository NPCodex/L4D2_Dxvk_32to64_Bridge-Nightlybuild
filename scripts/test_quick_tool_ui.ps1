# SPDX-License-Identifier: MIT
param([Parameter(Mandatory=$true)][string]$Executable,[Parameter(Mandatory=$true)][string]$EvidenceDirectory)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$taskEvidence=[IO.Path]::GetFullPath($EvidenceDirectory)
New-Item -ItemType Directory -Path $taskEvidence -Force | Out-Null
$taskRoot=Join-Path $taskEvidence ('fixture-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $taskRoot 'bin') -Force | Out-Null
[IO.File]::WriteAllBytes((Join-Path $taskRoot 'left4dead2.exe'),[byte[]]@(0))
$taskAssembly=[Reflection.Assembly]::LoadFrom([IO.Path]::GetFullPath($Executable))
$taskCoreType=$taskAssembly.GetType('BridgeQuickTool.Core')
$taskCore=[Activator]::CreateInstance($taskCoreType,[object[]]@($taskRoot.PSObject.BaseObject))
$taskRead=$taskAssembly.GetType('BridgeQuickTool.LogTail').GetMethod('Read')
$taskPath=Join-Path $taskRoot 'live.log'
$taskGroups=New-Object 'System.Collections.Generic.List[string]'
function Read-TaskLog { $taskRead.Invoke($null,[object[]]@($taskCore,$taskPath.PSObject.BaseObject)) }
foreach($taskEncoding in @([Text.UTF8Encoding]::new($true),[Text.Encoding]::Unicode,[Text.Encoding]::BigEndianUnicode,[Text.Encoding]::GetEncoding(936))){
    [IO.File]::WriteAllText($taskPath,"中文日志`r`n第二行`r`n",$taskEncoding)
    if((Read-TaskLog).Text -notmatch '中文日志[\s\S]*第二行'){throw 'Encoding regression'}
    [IO.File]::WriteAllText($taskPath,(('一行中文 abc123'+"`r`n")*35000),$taskEncoding)
    $taskSnapshot=Read-TaskLog
    if($taskSnapshot.CapturedBytes -gt 262144 -or $taskSnapshot.Text.Split("`n").Count -gt 2001 -or $taskSnapshot.Text -notmatch '一行中文'){throw 'Tail bounds or encoding failed'}
}
$taskGroups.Add('UTF-8 BOM, UTF-16 LE/BE and GBK bounded Unicode tails')
$taskWriter=[IO.File]::Open($taskPath,[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete)
try {$taskData=[Text.Encoding]::UTF8.GetBytes("first`r`npassword = secret-value`r`n");$taskWriter.Write($taskData,0,$taskData.Length);$taskWriter.Flush();$taskSnapshot=Read-TaskLog;if($taskSnapshot.Text -match 'secret-value' -or $taskSnapshot.Text -notmatch 'redacted'){throw 'Shared read/redaction failed'}}finally{$taskWriter.Dispose()}
$taskGroups.Add('Shared read while writer is open; credential redaction')
foreach($taskBad in @((Join-Path $taskRoot '..\outside.log'),(Join-Path $taskRoot 'setting.ini'))){$taskRejected=$false;try {$taskRead.Invoke($null,[object[]]@($taskCore,$taskBad.PSObject.BaseObject))}catch{$taskRejected=$true};if(!$taskRejected){throw 'Bad path accepted'}}
$taskGroups.Add('Outside-root and non-log paths rejected')
[IO.File]::WriteAllText($taskPath,"initial-live`r`n",[Text.Encoding]::UTF8)
$taskBefore=(Get-FileHash -LiteralPath $taskPath).Hash
$taskFormType=$taskAssembly.GetType('BridgeQuickTool.MainForm')
$taskForm=[Activator]::CreateInstance($taskFormType,[object[]]@($taskCore))
$taskFlags=[Reflection.BindingFlags]'Instance,NonPublic'
$taskHelp=$taskFormType.GetField('help',$taskFlags).GetValue($taskForm)
function Get-TaskControls([Windows.Forms.Control]$Control){foreach($taskChild in $Control.Controls){$taskChild;Get-TaskControls $taskChild}}
function Pump-Task([int]$Milliseconds){$taskEnd=[DateTime]::UtcNow.AddMilliseconds($Milliseconds);while([DateTime]::UtcNow -lt $taskEnd){[Windows.Forms.Application]::DoEvents();Start-Sleep -Milliseconds 20}}
$taskControls=@(Get-TaskControls $taskForm)
$taskTabs=$taskControls | Where-Object {$_ -is [Windows.Forms.TabControl]} | Select-Object -First 1
$taskMonitor=$taskControls | Where-Object {$_.GetType().FullName -eq 'BridgeQuickTool.LogMonitor'} | Select-Object -First 1
$taskTimer=$taskMonitor.GetType().GetField('timer',$taskFlags).GetValue($taskMonitor)
$taskOutput=$taskMonitor.GetType().GetField('output',$taskFlags).GetValue($taskMonitor)
$taskPause=$taskMonitor.GetType().GetField('pause',$taskFlags).GetValue($taskMonitor)
try {
    $taskFormType.GetField('rendering',$taskFlags).SetValue($taskForm,$true);$taskForm.ShowInTaskbar=$false;$taskForm.Opacity=0;$taskForm.Show();$taskTabs.SelectedIndex=3;Pump-Task 1300
    if($taskOutput.Text -notmatch 'initial-live' -or !$taskTimer.Enabled){throw 'Live initial read failed'}
    if((Get-FileHash -LiteralPath $taskPath).Hash -ne $taskBefore){throw 'Viewer modified source'}
    [IO.File]::AppendAllText($taskPath,"appended-live`r`n");Pump-Task 1400
    if($taskOutput.Text -notmatch 'appended-live'){throw 'Live append failed'}
    $taskPause.PerformClick();$taskFrozen=$taskOutput.Text
    [IO.File]::WriteAllText($taskPath,"truncated-replacement`r`n");Pump-Task 1100
    if($taskOutput.Text -ne $taskFrozen -or $taskTimer.Enabled){throw 'Pause failed'}
    $taskPause.PerformClick();Pump-Task 1300
    if($taskOutput.Text -notmatch 'truncated-replacement' -or $taskOutput.Text -match 'initial-live'){throw 'Resume/truncation failed'}
    [IO.File]::Move($taskPath,$taskPath+'.old-'+[Guid]::NewGuid().ToString('N'));[IO.File]::WriteAllText($taskPath,"rotated-live`r`n");Pump-Task 1300
    if($taskOutput.Text -notmatch 'rotated-live'){throw 'Rotation failed'}
    [IO.File]::Delete($taskPath);Pump-Task 1100
    [IO.File]::WriteAllText($taskPath,"recreated-live`r`n");Pump-Task 1300
    if($taskOutput.Text -notmatch 'recreated-live'){throw 'Missing-file recovery failed'}
    $taskGroups.Add('Actual UI initial read, append, pause/resume, truncation, rotation and missing-file recovery')
    $taskReadTip=$taskHelp.GetType().GetMethod('TextFor',$taskFlags)
    $taskInteractive=@($taskControls | Where-Object {$_ -is [Windows.Forms.Button] -or $_ -is [Windows.Forms.ComboBox] -or $_ -is [Windows.Forms.CheckBox]})
    foreach($taskControl in $taskInteractive){if([string]::IsNullOrWhiteSpace($taskReadTip.Invoke($taskHelp,[object[]]@($taskControl.PSObject.BaseObject)))){throw ('Missing hover explanation: '+$taskControl.Text)}}
    $taskBitmap=New-Object Drawing.Bitmap $taskForm.Width,$taskForm.Height
    try {$taskForm.DrawToBitmap($taskBitmap,[Drawing.Rectangle]::new([Drawing.Point]::Empty,$taskForm.Size));$taskBitmap.Save((Join-Path $taskEvidence 'monitor-preview.png'))} finally {$taskBitmap.Dispose()}
    $taskTabs.SelectedIndex=0;Pump-Task 200;if($taskTimer.Enabled){throw 'Hidden page continued polling'}
    $taskTabs.SelectedIndex=3;Pump-Task 200;$taskForm.Hide();Pump-Task 200;if($taskTimer.Enabled){throw 'Hidden window continued polling'}
    $taskGroups.Add('Hover help coverage; polling stops on hidden tab/window')
} finally {$taskForm.Dispose()}
@{version='1.1.0';passed=@($taskGroups);interactiveControls=$taskInteractive.Count;gameLaunched=$false;productionConfigWritten=$false} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $taskEvidence 'verification.json') -Encoding utf8
Write-Output ('PASS: '+$taskGroups.Count+' monitor groups; '+$taskInteractive.Count+' controls have help; no game launched.')
