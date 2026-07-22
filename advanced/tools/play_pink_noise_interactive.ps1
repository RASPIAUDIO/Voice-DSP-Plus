param(
    [double]$Dbfs = -36.0,
    [double]$Seconds = 5.0
)

$ErrorActionPreference = 'Stop'
$taskName = 'VoiceDSPPlusPinkNoise'
$python = "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe"
$script = "$env:USERPROFILE\voice-dsp-plus-pink-noise.py"
$userId = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name

if (-not (Test-Path -LiteralPath $python)) {
    throw "Python not found: $python"
}
if (-not (Test-Path -LiteralPath $script)) {
    throw "Pink-noise script not found: $script"
}

$action = New-ScheduledTaskAction `
    -Execute $python `
    -Argument "`"$script`" --device Realtek --host-api `"Windows WASAPI`" --dbfs $Dbfs --seconds $Seconds"
$trigger = New-ScheduledTaskTrigger -Once -At (Get-Date).AddMinutes(5)
$principal = New-ScheduledTaskPrincipal `
    -UserId $userId `
    -LogonType Interactive `
    -RunLevel Limited

Register-ScheduledTask `
    -TaskName $taskName `
    -Action $action `
    -Trigger $trigger `
    -Principal $principal `
    -Force | Out-Null
Start-ScheduledTask -TaskName $taskName
Write-Output "Started $Seconds s of pink noise at $Dbfs dBFS in the interactive session."
